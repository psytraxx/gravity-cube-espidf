/* The can's geometry, and the fluid's particle count and colour.
 *
 * The can is an LED strip wound in a spiral round a cylinder: CAN_LED_COUNT
 * LEDs in CAN_TURNS turns, each turn the same way round. The fluid runs on
 * a grid of cells one LED spacing apart, FLUID_NX x FLUID_NY x FLUID_NZ,
 * with the can's axis up the middle. */
#pragma once

#include <stdint.h>

#define CAN_LED_COUNT 200
#define CAN_TURNS     13
/* Distance between two LEDs along the strip (60 LEDs per metre). */
#define CAN_LED_SPACING_MM 16.7f
/* Height of one turn, in LED spacings: the distance between two rows
 * divided by the distance between two LEDs along the strip. */
#define CAN_ROW_PITCH 1.0f
/* Where the data enters (1: the top turn, 0: the bottom one), and which
 * way the strip winds seen from above (1: clockwise). */
#define CAN_START_AT_TOP 0
#define CAN_CLOCKWISE    0

#define CAN_LEDS_PER_TURN ((float)CAN_LED_COUNT / (float)CAN_TURNS)
/* Radius of the LED ring, in cells: the fluid's cells lie within it. */
#define CAN_RADIUS (CAN_LEDS_PER_TURN / 6.2831853f)
/* The LEDs climb CAN_TURNS turns; the can reaches half a row beyond the
 * first and the last. */
#define CAN_HEIGHT ((float)(CAN_TURNS + 1) * CAN_ROW_PITCH)

/* The grid round the can: at least 2 * CAN_RADIUS across and CAN_HEIGHT
 * high (test_can checks). */
#define FLUID_NX 5
#define FLUID_NY 5
#define FLUID_NZ 14

#define LED_COUNT CAN_LED_COUNT

/* Two particles per cell of fluid. FLUID_PARTICLES is room for half the
 * grid's cells, more than half the can holds. */
#define FLUID_PARTICLES_PER_CELL 2
#define FLUID_PARTICLES          (FLUID_PARTICLES_PER_CELL * FLUID_NX * FLUID_NY * FLUID_NZ / 2)

/* Gravity in cells per step squared: at an 18 ms step, 1 g for cells one
 * LED spacing apart (9.81 m/s^2 * 0.018^2 s^2 / spacing). */
#define FLUID_GRAVITY (9.81f * 0.018f * 0.018f * 1000.0f / CAN_LED_SPACING_MM)

typedef struct {
    uint8_t r, g, b;
} color_t;

/* The fluid starts green. A double tap picks a random hue; its channels
 * always add up to FLUID_LEVEL, so every colour draws the current of one
 * channel at 60/255, a fraction of white's. */
#define FLUID_LEVEL 60
#define FLUID_HUE   120

/* Each particle carries a tint, a hue offset of up to this many degrees
 * either side of the fluid's hue, so the fluid is not one flat colour and
 * its shades swirl and mix as it flows. */
#define FLUID_TINT_SPREAD 40.0f
/* Extra hue, in degrees per cell per step of speed: moving fluid
 * shimmers. */
#define FLUID_SPEED_TINT 60.0f
