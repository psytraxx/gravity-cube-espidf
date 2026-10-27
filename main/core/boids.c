#include "boids.h"

#include <string.h>

/* Distance boids try to keep from each other. */
#define SEPARATION_DISTANCE 1.5f

#define ALIGNMENT_WEIGHT  0.3f
#define COHESION_WEIGHT   0.1f
#define SEPARATION_WEIGHT 0.5f

/* How far a boid sees its neighbours. */
#define PERCEPTION_RADIUS 3.0f

/* Wall repulsion, starting BOUNDARY_MARGIN cells from each wall. */
#define BOUNDARY_FORCE  0.3f
#define BOUNDARY_MARGIN 1.5f

/* How much tilt acts as wind. Low keeps the flock mostly autonomous. */
#define WIND_SCALE 0.05f

/* How strongly spinning the cube sweeps the flock round. The flock is
 * carried against the spin, like fish in a turning tank, while it keeps
 * flocking. */
#define SPIN_CURRENT 0.5f

/* Multiplier on the centrifugal acceleration of a spinning cube (see the
 * fluid effect). */
#define CENTRIFUGAL_GAIN 20.0f

/* Velocity kept when a boid hits a wall. */
#define WALL_DAMPING 0.5f

/* Just under 1, so the speed limit does not oscillate at exactly
 * BOIDS_MAX_SPEED. */
#define SPEED_LIMIT_TOLERANCE 0.9999f

void boids_init(boids_t *b)
{
    memset(b, 0, sizeof *b);
}

void boids_populate(boids_t *b, int n)
{
    if (n > MAX_PARTICLES) {
        n = MAX_PARTICLES;
    }
    const float size = (float)CUBE_SIZE;
    b->n = n;
    for (int i = 0; i < n; i++) {
        /* Index-derived, pseudo-random but deterministic positions. */
        const float fi = (float)i;
        const float x = clampf(fmodf(fi * 7.131f, size), 0.5f, size - 0.5f);
        const float y = clampf(fmodf(fi * 11.243f, size), 0.5f, size - 0.5f);
        const float z = clampf(fmodf(fi * 13.579f, size), 0.5f, size - 0.5f);
        b->p[i] = particle_at(x, y, z);
        b->p[i].vel = v3((fmodf(x * 7.0f, 1.0f) - 0.5f) * 0.2f,
                         (fmodf(y * 11.0f, 1.0f) - 0.5f) * 0.2f,
                         (fmodf(z * 13.0f, 1.0f) - 0.5f) * 0.2f);
    }
}

void boids_set_rotation(boids_t *b, vec3_t rotation)
{
    b->rotation = rotation;
}

/* Push away from a wall within BOUNDARY_MARGIN of it. */
static inline float wall_push(float pos)
{
    float f = 0.0f;
    if (pos < BOUNDARY_MARGIN) {
        f += (BOUNDARY_MARGIN - pos) * BOUNDARY_FORCE;
    }
    if (pos > CUBE_BOUND - BOUNDARY_MARGIN) {
        f -= (pos - (CUBE_BOUND - BOUNDARY_MARGIN)) * BOUNDARY_FORCE;
    }
    return f;
}

/* Clamp to [0, CUBE_BOUND] and bounce off the wall with damping. */
static inline void wall_bounce(float *pos, float *vel)
{
    if (*pos < 0.0f) {
        *pos = 0.0f;
        *vel = fabsf(*vel) * WALL_DAMPING;
    } else if (*pos >= CUBE_BOUND) {
        *pos = CUBE_BOUND;
        *vel = -fabsf(*vel) * WALL_DAMPING;
    }
}

void boids_step(boids_t *b, vec3_t accel)
{
    const vec3_t center = v3(CUBE_BOUND / 2.0f, CUBE_BOUND / 2.0f, CUBE_BOUND / 2.0f);
    const vec3_t spin = b->rotation;
    const float spin2 = v3_dot(spin, spin);
    const vec3_t wind = v3_scale(accel, WIND_SCALE);

    for (int i = 0; i < b->n; i++) {
        vec3_t separation = v3(0.0f, 0.0f, 0.0f);
        vec3_t alignment = v3(0.0f, 0.0f, 0.0f);
        vec3_t cohesion = v3(0.0f, 0.0f, 0.0f);
        int neighbors = 0;

        for (int j = 0; j < b->n; j++) {
            if (i == j) {
                continue;
            }
            const vec3_t d = v3_sub(b->p[j].pos, b->p[i].pos);
            const float d2 = v3_dot(d, d);
            if (d2 < PERCEPTION_RADIUS * PERCEPTION_RADIUS && d2 > 0.0001f) {
                const float dist = sqrtf(d2);
                neighbors++;
                if (dist < SEPARATION_DISTANCE) {
                    const float repel = (SEPARATION_DISTANCE - dist) / SEPARATION_DISTANCE;
                    separation = v3_sub(separation, v3_scale(d, repel / dist));
                }
                alignment = v3_add(alignment, b->p[j].vel);
                cohesion = v3_add(cohesion, d);
            }
        }

        if (neighbors > 0) {
            alignment = v3_scale(alignment, ALIGNMENT_WEIGHT / (float)neighbors);
            cohesion = v3_scale(cohesion, COHESION_WEIGHT / (float)neighbors);
        }
        separation = v3_scale(separation, SEPARATION_WEIGHT);

        const vec3_t pos = b->p[i].pos;
        const vec3_t boundary = v3(wall_push(pos.x), wall_push(pos.y), wall_push(pos.z));

        /* Spinning sweeps the flock against the spin and out to the walls. */
        const vec3_t offset = v3_sub(pos, center);
        const vec3_t current = v3_scale(v3_cross(spin, offset), -SPIN_CURRENT);
        const vec3_t centrifugal = v3_scale(
            v3_sub(v3_scale(offset, spin2), v3_scale(spin, v3_dot(spin, offset))),
            CENTRIFUGAL_GAIN);

        vec3_t f = v3_add(v3_add(separation, alignment), v3_add(cohesion, boundary));
        f = v3_add(v3_add(f, wind), v3_add(current, centrifugal));
        b->forces[i] = f;
    }

    for (int i = 0; i < b->n; i++) {
        particle_t *p = &b->p[i];
        p->vel = v3_add(p->vel, b->forces[i]);

        const float limit = BOIDS_MAX_SPEED * SPEED_LIMIT_TOLERANCE;
        const float speed = v3_len(p->vel);
        if (speed > limit) {
            p->vel = v3_scale(p->vel, limit / speed);
        }

        p->pos = v3_add(p->pos, p->vel);
        wall_bounce(&p->pos.x, &p->vel.x);
        wall_bounce(&p->pos.y, &p->vel.y);
        wall_bounce(&p->pos.z, &p->vel.z);
    }
}
