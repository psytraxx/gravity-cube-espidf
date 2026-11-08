/* The can's geometry, and the fluid's particle count and colour.
 *
 * The can is an LED strip wound in a spiral round a cylinder, each turn
 * the same way round; a can that narrows has fewer LEDs in its upper
 * turns. The fluid runs on a grid of cells CAN_CELL_LEDS LED spacings
 * across, FLUID_NX x FLUID_NY x FLUID_NZ, with the can's axis up the
 * middle. Heights and radii below are in cells unless named otherwise. */
#pragma once

#include <math.h>
#include <stdint.h>

#define CAN_LED_COUNT 500
/* LEDs in one turn at the bottom and at the top. */
#define CAN_LEDS_PER_TURN_BOTTOM 39.0f
#define CAN_LEDS_PER_TURN_TOP    37.5f
/* Distance between two LEDs along the strip (100 LEDs per metre). */
#define CAN_LED_SPACING_MM 10.0f
/* Height of one turn, in LED spacings: the distance between two rows
 * divided by the distance between two LEDs along the strip. */
#define CAN_ROW_PITCH 0.65f
/* Where the data enters (1: the top turn, 0: the bottom one), and which
 * way the strip winds seen from above (1: clockwise). */
#define CAN_START_AT_TOP 0
#define CAN_CLOCKWISE    1

/* Size of a fluid cell, in LED spacings. The fluid's cost grows with the
 * cube of 1 / CAN_CELL_LEDS; 1.4 gives the can about 60 cells a layer
 * and a cell about every 2 rows, at about the cube firmware's cost per step. */
#define CAN_CELL_LEDS 1.4f

/* Turns of the spiral: the LEDs per turn change evenly from one end to
 * the other, so the mean of the two ends sets how many turns the strip
 * makes. */
#define CAN_TURNS           (2.0f * CAN_LED_COUNT / (CAN_LEDS_PER_TURN_BOTTOM + CAN_LEDS_PER_TURN_TOP))
#define CAN_ROW_PITCH_CELLS (CAN_ROW_PITCH / CAN_CELL_LEDS)
/* The can reaches half a row beyond the first and the last turn. */
#define CAN_HEIGHT ((CAN_TURNS + 1.0f) * CAN_ROW_PITCH_CELLS)
/* Radius of the widest turn. */
#define CAN_RADIUS_MAX                                                                             \
    (fmaxf(CAN_LEDS_PER_TURN_BOTTOM, CAN_LEDS_PER_TURN_TOP) / (6.2831853f * CAN_CELL_LEDS))

/* The grid round the can: at least 2 * CAN_RADIUS_MAX across and
 * CAN_HEIGHT high (test_fluid checks). */
#define FLUID_NX 9
#define FLUID_NY 9
#define FLUID_NZ 7

#define LED_COUNT CAN_LED_COUNT

/* Two particles per cell of fluid. FLUID_PARTICLES is room for half the
 * grid's cells, more than half the can holds. */
#define FLUID_PARTICLES_PER_CELL 2
#define FLUID_PARTICLES          (FLUID_PARTICLES_PER_CELL * FLUID_NX * FLUID_NY * FLUID_NZ / 2)

/* Gravity in cells per step squared: at an 18 ms step, 1 g for cells
 * CAN_CELL_LEDS LED spacings across (9.81 m/s^2 * 0.018^2 s^2 / cell). */
#define FLUID_GRAVITY (9.81f * 0.018f * 0.018f * 1000.0f / (CAN_CELL_LEDS * CAN_LED_SPACING_MM))

/* Turns from the bottom of the spiral at height z, 0 to CAN_TURNS. */
static inline float can_turns_at_height(float z)
{
    const float t = z / CAN_ROW_PITCH_CELLS - 0.5f;
    return t < 0.0f ? 0.0f : t > CAN_TURNS ? CAN_TURNS : t;
}

/* Radius of the can at height z: its LEDs per turn there, round a circle
 * of cells. */
static inline float can_radius_at(float z)
{
    const float n = CAN_LEDS_PER_TURN_BOTTOM + (CAN_LEDS_PER_TURN_TOP - CAN_LEDS_PER_TURN_BOTTOM) *
                                                   can_turns_at_height(z) / CAN_TURNS;
    return n / (6.2831853f * CAN_CELL_LEDS);
}

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
