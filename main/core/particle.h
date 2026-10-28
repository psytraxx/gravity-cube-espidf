/* Fluid particles, and the voxel cells they fill. */
#pragma once

#include <stdint.h>

#include "config.h"
#include "vec3.h"

typedef struct {
    vec3_t pos; /* 0.0 .. CUBE_SIZE on each axis; voxel c spans [c, c + 1) */
    vec3_t vel; /* cells per step */
    float tint;     /* hue offset in degrees, carried along with the fluid */
    uint8_t liquid; /* LIQUID_OIL or LIQUID_WATER */
} particle_t;

typedef struct {
    uint8_t x, y, z;
} cell_t;
