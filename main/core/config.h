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

/* Fluid: the whole cube full at two particles per voxel, starting with
 * amber oil and blue water. A double tap picks a random hue for the oil;
 * the water is always LIQUID_HUE_GAP degrees round from it. A hue's
 * channels always add up to FLUID_LEVEL, so every colour draws the
 * current of one channel at 60/255, a fraction of white's. */
#define FLUID_PARTICLES_PER_CELL 2
#define FLUID_PARTICLES          (FLUID_PARTICLES_PER_CELL * CUBE_SIZE * CUBE_SIZE * CUBE_SIZE)
#define FLUID_LEVEL              60
#define FLUID_HUE                40

/* Two liquids that do not mix, half the cube each: light oil and heavy
 * water. */
#define LIQUID_OIL     0
#define LIQUID_WATER   1
#define LIQUIDS        2
#define LIQUID_HUE_GAP 180

/* Each particle carries a tint, a hue offset of up to this many degrees
 * either side of the fluid's hue, so the fluid is not one flat colour and
 * its shades swirl and mix as it flows. */
#define FLUID_TINT_SPREAD 40.0f
/* Extra hue, in degrees per cell per step of speed: moving fluid
 * shimmers. */
#define FLUID_SPEED_TINT 60.0f
