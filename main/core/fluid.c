#include "fluid.h"

#include <math.h>
#include <string.h>

#define N CUBE_SIZE

/* Gravity in cells per step squared: at an 18 ms step, 1 g for 2.3 cm
 * voxels (9.81 m/s^2 * 0.018^2 s^2 / 0.023 m), so the fluid moves a
 * little slower than it would at the cube's real size. */
#define GRAVITY 0.14f

/* Multiplier on the cube's rotation forces (Euler, Coriolis, centrifugal).
 * 1.0 is the real thing. */
#define ROTATION_GAIN 1.0f

/* Particle radius in cells: particles closer than twice this are pushed
 * apart, and they stay this far from the walls. */
#define PARTICLE_RADIUS 0.3f

/* Pushing-apart passes per step. */
#define SEPARATION_PASSES 1

/* Gauss-Seidel passes of the pressure solve, and its over-relaxation. */
#define PRESSURE_ITERATIONS 30
#define OVER_RELAXATION     1.9f

/* How strongly cells packed above the rest density push particles out
 * again, so the fluid keeps its volume over time. */
#define DRIFT_STIFFNESS 1.0f

/* Share of FLIP (change in grid velocity) against PIC (grid velocity) in
 * the velocity particles take back: lower is calmer and more viscous. */
#define FLIP_RATIO 0.8f

/* A dark voxel lights once the particle density at its centre reaches
 * LIGHT_ON and goes dark again below LIGHT_OFF (rest density is
 * FLUID_PARTICLES_PER_CELL), so particles hovering at a cell boundary do
 * not flicker. */
#define LIGHT_ON  1.2f
#define LIGHT_OFF 0.4f

#define NONE UINT16_MAX

/* The 13 neighbouring cells "after" a cell (in z, then y, then x order).
 * Checking a cell against itself and these visits every pair of adjacent
 * cells exactly once. */
static const int8_t FORWARD_NEIGHBORS[13][3] = {
    {1, 0, 0},  {-1, 1, 0}, {0, 1, 0},  {1, 1, 0},  {-1, -1, 1},
    {0, -1, 1}, {1, -1, 1}, {-1, 0, 1}, {0, 0, 1},  {1, 0, 1},
    {-1, 1, 1}, {0, 1, 1},  {1, 1, 1},
};

/* 1/s for the pressure solve, where s is how many of a cell's six faces
 * are open (3 in a corner, 6 inside). */
static const float INV_OPEN[7] = {0.0f, 1.0f, 0.5f, 1.0f / 3.0f, 0.25f, 0.2f, 1.0f / 6.0f};

static inline float axis(vec3_t v, int a)
{
    return a == 0 ? v.x : a == 1 ? v.y : v.z;
}

static inline void set_axis(vec3_t *v, int a, float value)
{
    if (a == 0) {
        v->x = value;
    } else if (a == 1) {
        v->y = value;
    } else {
        v->z = value;
    }
}

static inline int cell_index(int x, int y, int z)
{
    return x + N * (y + N * z);
}

/* Face (i, j, k) of the faces carrying component a: index i, j or k runs
 * to N along axis a, to N - 1 along the others. */
static inline int face_index(int a, int i, int j, int k)
{
    const int nx = N + (a == 0), ny = N + (a == 1);
    return i + nx * (j + ny * k);
}

static inline int cell_of(float v)
{
    const int c = (int)v;
    return c < 0 ? 0 : c >= N ? N - 1 : c;
}

/* Trilinear weights of a point over the 8 nearest samples of a grid whose
 * samples sit at integer coordinates plus `offset` per axis, with `size`
 * samples per axis. */
typedef struct {
    int idx[8];
    float w[8];
} stencil_t;

static void stencil(vec3_t pos, const float offset[3], const int size[3], stencil_t *s)
{
    /* Per axis: the two samples either side and their weights. A sample
     * beyond the grid (past a wall) gets no weight rather than having it
     * folded into the sample inside, which would make cells along the
     * walls look packed. */
    int at[3][2];
    float w[3][2];
    for (int d = 0; d < 3; d++) {
        const float g = axis(pos, d) - offset[d];
        const int b = (int)(g + 1.0f) - 1; /* floor for g > -1 */
        const float t = g - (float)b;
        for (int k = 0; k < 2; k++) {
            const int i = b + k;
            const bool inside = i >= 0 && i < size[d];
            at[d][k] = inside ? i : 0;
            w[d][k] = inside ? (k ? t : 1.0f - t) : 0.0f;
        }
    }
    for (int c = 0; c < 8; c++) {
        const int dx = c & 1, dy = (c >> 1) & 1, dz = c >> 2;
        s->idx[c] = at[0][dx] + size[0] * (at[1][dy] + size[1] * at[2][dz]);
        s->w[c] = w[0][dx] * w[1][dy] * w[2][dz];
    }
}

/* Stencil over the faces carrying component a: they sit on the cell
 * boundary along a and at the cell centre along the other two axes. */
static void face_stencil(int a, vec3_t pos, stencil_t *s)
{
    const float offset[3] = {a == 0 ? 0.0f : 0.5f, a == 1 ? 0.0f : 0.5f, a == 2 ? 0.0f : 0.5f};
    const int size[3] = {N + (a == 0), N + (a == 1), N + (a == 2)};
    stencil(pos, offset, size, s);
}

static void cell_stencil(vec3_t pos, stencil_t *s)
{
    static const float offset[3] = {0.5f, 0.5f, 0.5f};
    static const int size[3] = {N, N, N};
    stencil(pos, offset, size, s);
}

/* Keep a particle off the walls; a wall stops motion into it. */
static void collide_walls(particle_t *p)
{
    for (int a = 0; a < 3; a++) {
        const float v = axis(p->pos, a);
        if (v < PARTICLE_RADIUS) {
            set_axis(&p->pos, a, PARTICLE_RADIUS);
            if (axis(p->vel, a) < 0.0f) {
                set_axis(&p->vel, a, 0.0f);
            }
        } else if (v > N - PARTICLE_RADIUS) {
            set_axis(&p->pos, a, N - PARTICLE_RADIUS);
            if (axis(p->vel, a) > 0.0f) {
                set_axis(&p->vel, a, 0.0f);
            }
        }
    }
}

/* Fast 1/sqrt(x) for x > 0: a bit-level estimate refined by two Newton
 * steps, multiplies only. The FPU has no single-instruction divide or
 * square root; pushing particles apart runs this thousands of times a
 * frame. */
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
    const float min_dist = 2.0f * PARTICLE_RADIUS;
    const vec3_t delta = v3_sub(p[b].pos, p[a].pos);
    const float d2 = v3_dot(delta, delta);
    if (d2 < min_dist * min_dist && d2 > 1e-8f) {
        const float inv = inv_sqrt(d2);
        const float push = (min_dist - d2 * inv) * 0.5f;
        const vec3_t offset = v3_scale(delta, push * inv);
        p[a].pos = v3_sub(p[a].pos, offset);
        p[b].pos = v3_add(p[b].pos, offset);
    }
}

/* Sort particles into cells, and count them per cell. */
static void bin_particles(fluid_t *f)
{
    for (int i = 0; i < FLUID_CELLS; i++) {
        f->head[i] = NONE;
        f->count[i] = 0;
    }
    for (int i = 0; i < f->n; i++) {
        const vec3_t pos = f->p[i].pos;
        const int cell = cell_index(cell_of(pos.x), cell_of(pos.y), cell_of(pos.z));
        f->next[i] = f->head[cell];
        f->head[cell] = (uint16_t)i;
        f->count[cell]++;
    }
}

/* One pass pushing every overlapping pair apart. Walks the cells, pairing
 * each only with itself and its forward neighbours, so every pair is
 * tested once. */
static void push_apart(fluid_t *f)
{
    bin_particles(f);
    for (int cz = 0; cz < N; cz++) {
        for (int cy = 0; cy < N; cy++) {
            for (int cx = 0; cx < N; cx++) {
                const int cell = cell_index(cx, cy, cz);
                if (f->head[cell] == NONE) {
                    continue;
                }
                int neighbors[13];
                int count = 0;
                for (int k = 0; k < 13; k++) {
                    const int nx = cx + FORWARD_NEIGHBORS[k][0];
                    const int ny = cy + FORWARD_NEIGHBORS[k][1];
                    const int nz = cz + FORWARD_NEIGHBORS[k][2];
                    if (nx >= 0 && nx < N && ny >= 0 && ny < N && nz < N) {
                        const int neighbor = cell_index(nx, ny, nz);
                        if (f->head[neighbor] != NONE) {
                            neighbors[count++] = neighbor;
                        }
                    }
                }
                for (uint16_t a = f->head[cell]; a != NONE; a = f->next[a]) {
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
    for (int i = 0; i < f->n; i++) {
        collide_walls(&f->p[i]);
    }
}

/* Spread particle velocities onto the faces. Faces on the walls carry no
 * flow. */
static void particles_to_grid(fluid_t *f)
{
    memset(f->vel, 0, sizeof f->vel);
    memset(f->weight, 0, sizeof f->weight);
    stencil_t s;
    for (int i = 0; i < f->n; i++) {
        const particle_t *p = &f->p[i];
        for (int a = 0; a < 3; a++) {
            face_stencil(a, p->pos, &s);
            const float v = axis(p->vel, a);
            for (int c = 0; c < 8; c++) {
                f->vel[a][s.idx[c]] += s.w[c] * v;
                f->weight[a][s.idx[c]] += s.w[c];
            }
        }
    }
    for (int a = 0; a < 3; a++) {
        for (int i = 0; i < FLUID_FACES; i++) {
            if (f->weight[a][i] > 0.0f) {
                f->vel[a][i] /= f->weight[a][i];
            }
        }
    }
    for (int j = 0; j < N; j++) {
        for (int k = 0; k < N; k++) {
            f->vel[0][face_index(0, 0, j, k)] = f->vel[0][face_index(0, N, j, k)] = 0.0f;
            f->vel[1][face_index(1, j, 0, k)] = f->vel[1][face_index(1, j, N, k)] = 0.0f;
            f->vel[2][face_index(2, j, k, 0)] = f->vel[2][face_index(2, j, k, N)] = 0.0f;
        }
    }
    memcpy(f->prev, f->vel, sizeof f->prev);
}

/* Make the flow out of every fluid cell zero (Gauss-Seidel with
 * over-relaxation), against closed walls and an open surface. Cells packed
 * above the rest density get a little outflow, which spreads them out
 * again. */
static void solve_pressure(fluid_t *f)
{
    const float rest = (float)FLUID_PARTICLES_PER_CELL;
    for (int it = 0; it < PRESSURE_ITERATIONS; it++) {
        for (int k = 0; k < N; k++) {
            for (int j = 0; j < N; j++) {
                for (int i = 0; i < N; i++) {
                    const int cell = cell_index(i, j, k);
                    if (f->count[cell] == 0) {
                        continue;
                    }
                    const int x0 = face_index(0, i, j, k), x1 = face_index(0, i + 1, j, k);
                    const int y0 = face_index(1, i, j, k), y1 = face_index(1, i, j + 1, k);
                    const int z0 = face_index(2, i, j, k), z1 = face_index(2, i, j, k + 1);
                    const float sx0 = i > 0, sx1 = i < N - 1;
                    const float sy0 = j > 0, sy1 = j < N - 1;
                    const float sz0 = k > 0, sz1 = k < N - 1;
                    const int open = (int)(sx0 + sx1 + sy0 + sy1 + sz0 + sz1);
                    float div = f->vel[0][x1] - f->vel[0][x0] + f->vel[1][y1] - f->vel[1][y0] +
                                f->vel[2][z1] - f->vel[2][z0];
                    const float compression = f->density[cell] - rest;
                    if (compression > 0.0f) {
                        div -= DRIFT_STIFFNESS * compression;
                    }
                    const float p = -div * INV_OPEN[open] * OVER_RELAXATION;
                    f->vel[0][x0] -= sx0 * p;
                    f->vel[0][x1] += sx1 * p;
                    f->vel[1][y0] -= sy0 * p;
                    f->vel[1][y1] += sy1 * p;
                    f->vel[2][z0] -= sz0 * p;
                    f->vel[2][z1] += sz1 * p;
                }
            }
        }
    }
}

/* Give the particles back the grid velocity: FLIP adds the change the
 * pressure solve made, PIC takes the grid velocity outright. Only faces
 * with particles nearby count. */
static void grid_to_particles(fluid_t *f)
{
    stencil_t s;
    for (int i = 0; i < f->n; i++) {
        particle_t *p = &f->p[i];
        for (int a = 0; a < 3; a++) {
            face_stencil(a, p->pos, &s);
            float sum = 0.0f, pic = 0.0f, change = 0.0f;
            for (int c = 0; c < 8; c++) {
                const int idx = s.idx[c];
                if (f->weight[a][idx] > 0.0f) {
                    sum += s.w[c];
                    pic += s.w[c] * f->vel[a][idx];
                    change += s.w[c] * (f->vel[a][idx] - f->prev[a][idx]);
                }
            }
            if (sum > 0.0f) {
                const float inv = 1.0f / sum;
                const float flip = axis(p->vel, a) + change * inv;
                set_axis(&p->vel, a, (1.0f - FLIP_RATIO) * pic * inv + FLIP_RATIO * flip);
            }
        }
    }
}

/* Particle density and tint at the cell centres, and which voxels the
 * density lights. The pressure solve of the next step uses the same
 * density: particles do not move in between. */
static void update_density(fluid_t *f)
{
    memset(f->density, 0, sizeof f->density);
    memset(f->tint, 0, sizeof f->tint);
    stencil_t s;
    for (int i = 0; i < f->n; i++) {
        const particle_t *p = &f->p[i];
        const float tint = p->tint + FLUID_SPEED_TINT * v3_len(p->vel);
        cell_stencil(p->pos, &s);
        for (int c = 0; c < 8; c++) {
            f->density[s.idx[c]] += s.w[c];
            f->tint[s.idx[c]] += s.w[c] * tint;
        }
    }
    for (int i = 0; i < FLUID_CELLS; i++) {
        f->lit[i] = f->density[i] >= (f->lit[i] ? LIGHT_OFF : LIGHT_ON);
        if (f->density[i] > 0.0f) {
            f->tint[i] /= f->density[i];
        }
    }
}

void fluid_init(fluid_t *f)
{
    memset(f, 0, sizeof *f);
}

void fluid_populate(fluid_t *f, int n)
{
    /* Two particles per voxel, on opposite corners of a smaller cube in
     * it: evenly spread and clear of the walls. */
    static const float OFFSETS[2] = {0.3f, 0.7f};
    if (n > FLUID_PARTICLES) {
        n = FLUID_PARTICLES;
    }
    f->n = 0;
    for (int z = 0; z < N && f->n < n; z++) {
        for (int y = 0; y < N && f->n < n; y++) {
            for (int x = 0; x < N && f->n < n; x++) {
                for (int k = 0; k < FLUID_PARTICLES_PER_CELL && f->n < n; k++) {
                    const float o = OFFSETS[k % 2];
                    const vec3_t pos = v3((float)x + o, (float)y + o, (float)z + o);
                    /* 0 in one bottom corner to 1 at the far top edge of a
                     * half-full cube. */
                    const float diagonal = (pos.x + pos.y + pos.z) / (2.5f * N);
                    f->p[f->n++] = (particle_t){
                        .pos = pos,
                        .vel = v3(0.0f, 0.0f, 0.0f),
                        .tint = FLUID_TINT_SPREAD * (2.0f * diagonal - 1.0f),
                    };
                }
            }
        }
    }
    memset(f->lit, 0, sizeof f->lit);
    update_density(f);
}

void fluid_set_rotation(fluid_t *f, vec3_t rotation)
{
    f->rotation = rotation;
}

void fluid_set_lift(fluid_t *f, const float lift[N * N])
{
    memcpy(f->lift, lift, sizeof f->lift);
}

void fluid_step(fluid_t *f, vec3_t accel)
{
    const vec3_t gravity = v3_scale(accel, GRAVITY);
    /* Columns for the lift run along the axis gravity mostly points along;
     * u and v are the other two. */
    const float gx = fabsf(accel.x), gy = fabsf(accel.y), gz = fabsf(accel.z);
    const int up_axis = gx >= gy && gx >= gz ? 0 : gy >= gz ? 1 : 2;
    const int u_axis = up_axis == 0 ? 1 : 0, v_axis = up_axis == 2 ? 1 : 2;
    const float up = (axis(accel, up_axis) > 0.0f ? -1.0f : 1.0f) * GRAVITY;
    const vec3_t center = v3(N / 2.0f, N / 2.0f, N / 2.0f);
    /* In the turning cube the fluid feels the Euler force (the spin
     * speeding up), Coriolis and centrifugal forces. */
    const vec3_t spin = v3_scale(f->rotation, ROTATION_GAIN);
    const vec3_t spin_up = v3_sub(spin, v3_scale(f->last_rotation, ROTATION_GAIN));
    f->last_rotation = f->rotation;

    for (int i = 0; i < f->n; i++) {
        particle_t *p = &f->p[i];
        const vec3_t r = v3_sub(p->pos, center);
        vec3_t a = gravity;
        a = v3_sub(a, v3_cross(spin_up, r));
        a = v3_sub(a, v3_scale(v3_cross(spin, p->vel), 2.0f));
        a = v3_sub(a, v3_cross(spin, v3_cross(spin, r)));
        const int column = cell_of(axis(p->pos, u_axis)) + N * cell_of(axis(p->pos, v_axis));
        set_axis(&a, up_axis, axis(a, up_axis) + up * f->lift[column]);
        p->vel = v3_add(p->vel, a);
    }

    /* Make the velocity divergence-free before moving, so a fluid at rest
     * stays at rest. */
    bin_particles(f);
    particles_to_grid(f);
    solve_pressure(f);
    grid_to_particles(f);

    for (int i = 0; i < f->n; i++) {
        particle_t *p = &f->p[i];
        p->vel.x = clampf(p->vel.x, -FLUID_MAX_VELOCITY, FLUID_MAX_VELOCITY);
        p->vel.y = clampf(p->vel.y, -FLUID_MAX_VELOCITY, FLUID_MAX_VELOCITY);
        p->vel.z = clampf(p->vel.z, -FLUID_MAX_VELOCITY, FLUID_MAX_VELOCITY);
        p->pos = v3_add(p->pos, p->vel);
        collide_walls(p);
    }
    for (int pass = 0; pass < SEPARATION_PASSES; pass++) {
        push_apart(f);
    }
    update_density(f);
}

bool fluid_lit(const fluid_t *f, cell_t c)
{
    return f->lit[cell_index(c.x, c.y, c.z)];
}

float fluid_tint(const fluid_t *f, cell_t c)
{
    return f->tint[cell_index(c.x, c.y, c.z)];
}
