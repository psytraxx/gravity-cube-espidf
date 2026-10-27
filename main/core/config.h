/* Cube geometry and the per-effect settings: particle count and colour. */
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

/* Fluid: half the cube full. Dim green only: a single channel draws a
 * fraction of the current of white, and the eye is most sensitive to green,
 * so it stays visible at a low level. */
#define FLUID_PARTICLES 256
#define FLUID_COLOR     ((color_t){0, 60, 0})

/* A few dozen boids read as a flock; dim amber so a double tap visibly
 * changes the cube. */
#define BOIDS_PARTICLES 32
#define BOIDS_COLOR     ((color_t){40, 20, 0})

/* Largest particle count of any effect. */
#define MAX_PARTICLES FLUID_PARTICLES
