/* Boids: a flock following Reynolds' three rules (separation, alignment,
 * cohesion). Tilting the cube blows the flock like wind; spinning it sweeps
 * the flock round against the spin and out to the walls. */
#pragma once

#include "config.h"
#include "particle.h"

/* Maximum boid speed, in cells per step. */
#define BOIDS_MAX_SPEED 0.4f

typedef struct {
    particle_t p[MAX_PARTICLES];
    int n;
    vec3_t forces[MAX_PARTICLES];
    /* Rotation of the cube per step (axis times radians), in cube axes. */
    vec3_t rotation;
} boids_t;

void boids_init(boids_t *b);

/* Spread `n` boids through the cube, deterministically, each with a small
 * starting velocity. */
void boids_populate(boids_t *b, int n);

/* Advance one step. `accel` acts as wind. */
void boids_step(boids_t *b, vec3_t accel);

void boids_set_rotation(boids_t *b, vec3_t rotation);

/* Voxel that boid i lights. */
static inline cell_t boids_cell(const boids_t *b, int i)
{
    return particle_cell(&b->p[i]);
}
