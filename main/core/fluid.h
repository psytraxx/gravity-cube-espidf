/* Fluid: gravity-driven particles that pile up and slosh.
 *
 * Each step integrates velocity, moves every particle to its predicted
 * position, then pushes overlapping particles apart in a few solver passes
 * (position-based collisions on a spatial grid). Velocity is then the
 * distance actually moved, so a particle blocked by a wall or its
 * neighbours comes to rest instead of pressing on forever. */
#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "config.h"
#include "particle.h"

/* One cell per voxel plus one, so a particle on the far wall has its own. */
#define FLUID_GRID       (CUBE_SIZE + 1)
#define FLUID_GRID_CELLS (FLUID_GRID * FLUID_GRID * FLUID_GRID)

/* Maximum velocity per axis, in cells per step. */
#define FLUID_MAX_VELOCITY 0.9f

typedef struct {
    particle_t p[MAX_PARTICLES];
    int n;
    /* Displayed cell per particle, moved with hysteresis; the first
     * `ncells` are valid. */
    cell_t cells[MAX_PARTICLES];
    int ncells;
    /* Positions at the start of the current step. */
    vec3_t previous[MAX_PARTICLES];
    /* Spatial grid: first particle per cell, next particle in the same
     * cell; UINT16_MAX ends a list. */
    uint16_t head[FLUID_GRID_CELLS];
    uint16_t next[MAX_PARTICLES];
    /* Rotation of the cube per step (axis times radians), in cube axes. */
    vec3_t rotation;
} fluid_t;

void fluid_init(fluid_t *f);

/* Fill the bottom half of the cube (high y), row by row, with up to `n`
 * particles. */
void fluid_populate(fluid_t *f, int n);

/* Add one particle at rest; returns false when full. */
bool fluid_add(fluid_t *f, particle_t p);

/* Advance one step. `accel` is gravity plus motion in g, in cube axes. */
void fluid_step(fluid_t *f, vec3_t accel);

/* How far the cube turned since the last step, as a rotation vector in
 * cube axes. Applies to the following steps until set again. */
void fluid_set_rotation(fluid_t *f, vec3_t rotation);

/* Voxel that particle i lights. */
cell_t fluid_cell(const fluid_t *f, int i);
