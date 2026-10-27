/* Cube geometry and the fluid's particle count and colour. */
#pragma once

#include <stdint.h>

/* 8x8x8 voxels; particle coordinates run from 0.0 to CUBE_BOUND. */
#define CUBE_SIZE  8
#define CUBE_BOUND ((float)(CUBE_SIZE - 1))

/* Six 8x8 panels, one per face, chained in one WS2812 string. */
#define LED_COUNT (6 * CUBE_SIZE * CUBE_SIZE)

typedef struct {
    uint8_t r, g, b;
} color_t;

/* Fluid: half the cube full, starting green. A double tap picks a random
 * hue; its channels always add up to FLUID_LEVEL, so every colour draws
 * the current of one channel at 60/255, a fraction of white's. */
#define FLUID_PARTICLES 256
#define FLUID_LEVEL     60
#define FLUID_HUE       120

#define MAX_PARTICLES FLUID_PARTICLES
