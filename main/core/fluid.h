/* Fluid: an incompressible FLIP fluid (inviscid Navier-Stokes) on the
 * cube's 8x8x8 voxels.
 *
 * Particles carry the fluid's velocity. Each step they are accelerated by
 * gravity and by the cube's rotation, move, and are pushed apart so they
 * stay evenly spread. Their velocity is then spread onto a staggered (MAC)
 * grid with one cell per voxel, where a Gauss-Seidel pressure solve makes
 * the flow divergence-free against the cube's walls. The particles take
 * back the change in grid velocity (FLIP, blended with a little of the
 * grid velocity itself to damp noise). A voxel lights while particles
 * fill it.
 *
 * The fluid is two liquids that do not mix, oil and water. Water is
 * heavier: gravity and the cube's turning push it harder than the oil, so
 * it sinks under the oil and is flung out past it. A voxel shows the
 * liquid that fills most of it, so the two never blend.
 *
 * Units are cells and steps: positions run from 0.0 to CUBE_SIZE, voxel c
 * spans [c, c + 1), velocities are cells per step. */
#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "config.h"
#include "particle.h"

#define FLUID_CELLS (CUBE_SIZE * CUBE_SIZE * CUBE_SIZE)
/* Faces carrying one velocity component: one more than the cells along
 * that component's axis. */
#define FLUID_FACES ((CUBE_SIZE + 1) * CUBE_SIZE * CUBE_SIZE)

/* Maximum velocity per axis, in cells per step: a particle never skips a
 * cell. */
#define FLUID_MAX_VELOCITY 1.0f

typedef struct {
    particle_t p[FLUID_PARTICLES];
    int n;
    /* Face velocities per axis after the pressure solve, before it, and
     * the particle weight behind each (0: no particle nearby). */
    float vel[3][FLUID_FACES];
    float prev[3][FLUID_FACES];
    float weight[3][FLUID_FACES];
    /* Particle density at each cell's centre, in all and per liquid, the
     * density-weighted mean tint of each liquid there (speed included),
     * and particles per cell. */
    float density[FLUID_CELLS];
    float liquid_density[LIQUIDS][FLUID_CELLS];
    float tint[LIQUIDS][FLUID_CELLS];
    uint8_t count[FLUID_CELLS];
    bool lit[FLUID_CELLS];
    /* Particles per cell as linked lists, for pushing them apart: first
     * particle per cell, next in the same cell; UINT16_MAX ends a list. */
    uint16_t head[FLUID_CELLS];
    uint16_t next[FLUID_PARTICLES];
    /* Rotation of the cube per step (axis times radians), in cube axes,
     * this step and the last. */
    vec3_t rotation;
    vec3_t last_rotation;
} fluid_t;

void fluid_init(fluid_t *f);

/* Fill the bottom half of the cube (low z), FLUID_PARTICLES_PER_CELL
 * particles per voxel, with up to `n` particles at rest: oil and water in
 * a 3D checkerboard of voxels, which sort themselves out as soon as the
 * fluid runs. Tints run diagonally across the fluid from
 * -FLUID_TINT_SPREAD to +FLUID_TINT_SPREAD. */
void fluid_populate(fluid_t *f, int n);

/* Advance one step. `accel` is gravity plus motion in g, in cube axes,
 * pointing the way the fluid falls. */
void fluid_step(fluid_t *f, vec3_t accel);

/* How far the cube turned since the last step, as a rotation vector in
 * cube axes. Applies to the following steps until set again. */
void fluid_set_rotation(fluid_t *f, vec3_t rotation);

/* Whether the fluid fills voxel c. */
bool fluid_lit(const fluid_t *f, cell_t c);

/* The liquid that fills most of voxel c: LIQUID_OIL or LIQUID_WATER. */
int fluid_liquid(const fluid_t *f, cell_t c);

/* Hue offset in degrees for voxel c: the tints of its particles of
 * fluid_liquid(), plus FLUID_SPEED_TINT for their speed. */
float fluid_tint(const fluid_t *f, cell_t c);
