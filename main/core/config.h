/* Cube geometry and the fluid's particle count and colour. */
#pragma once

#include <stdint.h>

/* 8x8x8 voxels; particle coordinates run from 0.0 to CUBE_SIZE. */
#define CUBE_SIZE 8

/* Six 8x8 panels, one per face, chained in one WS2812 string. */
#define LED_COUNT (6 * CUBE_SIZE * CUBE_SIZE)

typedef struct {
    uint8_t r, g, b;
} color_t;

/* Fluid: half the cube full at two particles per voxel, starting green. A
 * double tap picks a random hue; its channels always add up to
 * FLUID_LEVEL, so every colour draws the current of one channel at
 * 60/255, a fraction of white's. */
#define FLUID_PARTICLES_PER_CELL 2
#define FLUID_PARTICLES          (FLUID_PARTICLES_PER_CELL * CUBE_SIZE * CUBE_SIZE * CUBE_SIZE / 2)
#define FLUID_LEVEL              60
#define FLUID_HUE                120
