#include "fluid.h"

#include <string.h>

/* Particles closer than this (in cells) push each other apart. */
#define COLLISION_THRESHOLD 1.0f

/* Below this squared distance two particles sit on top of each other and
 * have no direction to separate in. */
#define MIN_DISTANCE_SQUARED 0.0001f

/* Gravity acceleration multiplier. */
#define GRAVITY 0.45f

/* Responsiveness to acceleration: lower is calmer, higher (0.4-0.5) more
 * dynamic but can turn unstable. */
#define ACCELERATION_SCALE 0.3f

/* Velocity smoothing / inertia (0.0-1.0). */
#define VELOCITY_SMOOTHING 0.85f

/* Fraction of the distance moved kept as velocity: friction (0.0-1.0). */
#define VELOCITY_DECAY 0.98f

/* Fraction of the overlap between two particles corrected per solver pass. */
#define COLLISION_STIFFNESS 0.8f

/* Collision solver passes per step. More passes keep a deep pile from
 * compressing. */
#define SOLVER_ITERATIONS 4

/* Fraction of the cube's spin the fluid resists each step. Spinning the
 * cube turns it under the fluid, so in cube space the fluid slides back
 * against the spin (0.0-1.0). */
#define ROTATION_LAG 0.7f

/* Multiplier on the centrifugal acceleration of a spinning cube. The real
 * force at hand-spin speeds is far weaker than gravity; this makes spinning
 * visibly push the fluid out to the walls. */
#define CENTRIFUGAL_GAIN 20.0f

/* How far (in cells) a particle must move from its displayed cell's centre
 * before its LED moves. Above 0.5 stops particles sitting on a cell
 * boundary from flickering. */
#define DISPLAY_HYSTERESIS 0.65f

#define NONE UINT16_MAX

/* The 13 neighbouring cells "after" a cell (in z, then y, then x order).
 * Checking a cell against itself and these visits every pair of adjacent
 * cells exactly once. */
static const int8_t FORWARD_NEIGHBORS[13][3] = {
    {1, 0, 0},  {-1, 1, 0}, {0, 1, 0},  {1, 1, 0},  {-1, -1, 1},
    {0, -1, 1}, {1, -1, 1}, {-1, 0, 1}, {0, 0, 1},  {1, 0, 1},
    {-1, 1, 1}, {0, 1, 1},  {1, 1, 1},
};

void fluid_init(fluid_t *f)
{
    memset(f, 0, sizeof *f);
}

void fluid_populate(fluid_t *f, int n)
{
    if (n > MAX_PARTICLES) {
        n = MAX_PARTICLES;
    }
    f->n = 0;
    f->ncells = 0;
    for (int y = CUBE_SIZE / 2; y < CUBE_SIZE && f->n < n; y++) {
        for (int z = 0; z < CUBE_SIZE && f->n < n; z++) {
            for (int x = 0; x < CUBE_SIZE && f->n < n; x++) {
                f->p[f->n++] = particle_at((float)x, (float)y, (float)z);
            }
        }
    }
}

bool fluid_add(fluid_t *f, particle_t p)
{
    if (f->n >= MAX_PARTICLES) {
        return false;
    }
    f->p[f->n++] = p;
    return true;
}

void fluid_set_rotation(fluid_t *f, vec3_t rotation)
{
    f->rotation = rotation;
}

/* Fast 1/sqrt(x) for x > 0: a bit-level estimate refined by two Newton
 * steps, multiplies only. The ESP32 FPU has no single-instruction divide or
 * square root; the collision solver runs this thousands of times a frame. */
static inline float inv_sqrt(float x)
{
    union {
        float f;
        uint32_t i;
    } u = {.f = x};
    u.i = 0x5f3759dfu - (u.i >> 1);
    float y = u.f;
    const float half = 0.5f * x;
    y = y * (1.5f - half * y * y);
    y = y * (1.5f - half * y * y);
    return y;
}

/* Push particles a and b apart if they overlap. */
static inline void separate(particle_t *p, int a, int b)
{
    const vec3_t delta = v3_sub(p[b].pos, p[a].pos);
    const float d2 = v3_dot(delta, delta);
    if (d2 < COLLISION_THRESHOLD * COLLISION_THRESHOLD && d2 > MIN_DISTANCE_SQUARED) {
        const float inv = inv_sqrt(d2);
        const float dist = d2 * inv;
        const float push = (COLLISION_THRESHOLD - dist) * 0.5f * COLLISION_STIFFNESS;
        const vec3_t offset = v3_scale(delta, push * inv);
        p[a].pos = v3_sub(p[a].pos, offset);
        p[b].pos = v3_add(p[b].pos, offset);
    }
}

static inline int grid_axis(float v)
{
    return (int)clampf(v, 0.0f, (float)(FLUID_GRID - 1));
}

/* One solver pass: separate every overlapping pair in proportion to its
 * overlap. Walks the grid cell by cell and pairs each cell only with itself
 * and its forward neighbours, so every pair is tested once and neighbour
 * lookups happen per cell, not per particle. This dominates frame time. */
static void resolve_collisions(fluid_t *f)
{
    const int g = FLUID_GRID;
    for (int i = 0; i < FLUID_GRID_CELLS; i++) {
        f->head[i] = NONE;
    }
    for (int i = 0; i < f->n; i++) {
        const vec3_t pos = f->p[i].pos;
        const int cell = grid_axis(pos.x) + grid_axis(pos.y) * g + grid_axis(pos.z) * g * g;
        f->next[i] = f->head[cell];
        f->head[cell] = (uint16_t)i;
    }

    int neighbors[13];
    for (int cz = 0; cz < g; cz++) {
        for (int cy = 0; cy < g; cy++) {
            for (int cx = 0; cx < g; cx++) {
                const int cell = cx + cy * g + cz * g * g;
                if (f->head[cell] == NONE) {
                    continue;
                }

                int count = 0;
                for (int k = 0; k < 13; k++) {
                    const int nx = cx + FORWARD_NEIGHBORS[k][0];
                    const int ny = cy + FORWARD_NEIGHBORS[k][1];
                    const int nz = cz + FORWARD_NEIGHBORS[k][2];
                    if (nx >= 0 && nx < g && ny >= 0 && ny < g && nz < g) {
                        const int neighbor = nx + ny * g + nz * g * g;
                        if (f->head[neighbor] != NONE) {
                            neighbors[count++] = neighbor;
                        }
                    }
                }

                for (uint16_t a = f->head[cell]; a != NONE; a = f->next[a]) {
                    /* Particles later in this cell's list, then every
                     * particle next door. */
                    for (uint16_t b = f->next[a]; b != NONE; b = f->next[b]) {
                        separate(f->p, a, b);
                    }
                    for (int k = 0; k < count; k++) {
                        for (uint16_t b = f->head[neighbors[k]]; b != NONE; b = f->next[b]) {
                            separate(f->p, a, b);
                        }
                    }
                }
            }
        }
    }
}

static inline void follow_axis(uint8_t *cell, float pos, uint8_t target)
{
    const float d = pos - (float)*cell;
    if (d > DISPLAY_HYSTERESIS || d < -DISPLAY_HYSTERESIS) {
        *cell = target;
    }
}

/* Move each particle's displayed cell only once it has clearly left it. */
static void update_cells(fluid_t *f)
{
    for (; f->ncells < f->n; f->ncells++) {
        f->cells[f->ncells] = particle_cell(&f->p[f->ncells]);
    }
    for (int i = 0; i < f->n; i++) {
        const cell_t target = particle_cell(&f->p[i]);
        cell_t *c = &f->cells[i];
        follow_axis(&c->x, f->p[i].pos.x, target.x);
        follow_axis(&c->y, f->p[i].pos.y, target.y);
        follow_axis(&c->z, f->p[i].pos.z, target.z);
    }
}

void fluid_step(fluid_t *f, vec3_t accel)
{
    const vec3_t scaled = v3_scale(accel, ACCELERATION_SCALE * GRAVITY);
    const vec3_t center = v3(CUBE_BOUND / 2.0f, CUBE_BOUND / 2.0f, CUBE_BOUND / 2.0f);
    const vec3_t spin = f->rotation;
    const float spin2 = v3_dot(spin, spin);

    /* Integrate velocity and move every particle to its predicted position. */
    for (int i = 0; i < f->n; i++) {
        particle_t *p = &f->p[i];
        f->previous[i] = p->pos;

        /* In a spinning cube, the part of the offset from the centre that
         * is perpendicular to the spin axis is flung outwards. */
        const vec3_t offset = v3_sub(p->pos, center);
        const vec3_t centrifugal = v3_scale(
            v3_sub(v3_scale(offset, spin2), v3_scale(spin, v3_dot(spin, offset))),
            CENTRIFUGAL_GAIN);

        p->vel = v3_add(v3_add(v3_scale(p->vel, VELOCITY_SMOOTHING), scaled), centrifugal);
        p->vel.x = clampf(p->vel.x, -FLUID_MAX_VELOCITY, FLUID_MAX_VELOCITY);
        p->vel.y = clampf(p->vel.y, -FLUID_MAX_VELOCITY, FLUID_MAX_VELOCITY);
        p->vel.z = clampf(p->vel.z, -FLUID_MAX_VELOCITY, FLUID_MAX_VELOCITY);

        /* The fluid partly stays put while the cube turns, so it slides
         * against the spin. */
        const vec3_t lag = v3_scale(v3_cross(spin, offset), -ROTATION_LAG);

        p->pos = v3_add(v3_add(p->pos, p->vel), lag);
        clamp_to_cube(&p->pos);
    }

    /* Push overlapping particles apart, keeping them inside the cube. */
    for (int it = 0; it < SOLVER_ITERATIONS; it++) {
        resolve_collisions(f);
        for (int i = 0; i < f->n; i++) {
            clamp_to_cube(&f->p[i].pos);
        }
    }

    /* Velocity is the distance actually travelled. */
    for (int i = 0; i < f->n; i++) {
        f->p[i].vel = v3_scale(v3_sub(f->p[i].pos, f->previous[i]), VELOCITY_DECAY);
    }

    update_cells(f);
}

cell_t fluid_cell(const fluid_t *f, int i)
{
    return i < f->ncells ? f->cells[i] : particle_cell(&f->p[i]);
}
