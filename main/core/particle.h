/* Particles with continuous positions, and the voxel each one lights. */
#pragma once

#include <stdint.h>

#include "config.h"
#include "vec3.h"

typedef struct {
    vec3_t pos; /* 0.0 .. CUBE_BOUND on each axis */
    vec3_t vel; /* cells per step */
} particle_t;

typedef struct {
    uint8_t x, y, z;
} cell_t;

static inline particle_t particle_at(float x, float y, float z)
{
    return (particle_t){.pos = v3(x, y, z), .vel = v3(0.0f, 0.0f, 0.0f)};
}

/* Nearest voxel, clamped to the cube. Rounding keeps a particle resting on
 * a wall (exactly 0.0 or CUBE_BOUND) in the same cell when it jitters. */
static inline cell_t particle_cell(const particle_t *p)
{
    return (cell_t){
        (uint8_t)clampf(p->pos.x + 0.5f, 0.0f, CUBE_BOUND),
        (uint8_t)clampf(p->pos.y + 0.5f, 0.0f, CUBE_BOUND),
        (uint8_t)clampf(p->pos.z + 0.5f, 0.0f, CUBE_BOUND),
    };
}

static inline float particle_speed(const particle_t *p)
{
    return v3_len(p->vel);
}

/* Keep a position inside the cube on every axis. */
static inline void clamp_to_cube(vec3_t *pos)
{
    pos->x = clampf(pos->x, 0.0f, CUBE_BOUND);
    pos->y = clampf(pos->y, 0.0f, CUBE_BOUND);
    pos->z = clampf(pos->z, 0.0f, CUBE_BOUND);
}
