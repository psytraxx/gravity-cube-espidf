/* Maps voxels on the cube's surface to LEDs in the chain, and builds
 * frames.
 *
 * Six 8x8 zigzag panels, chained OUT to IN in the WLED wiring. The four
 * side panels run round the cube: each has IN at the bottom of the edge
 * it shares with the previous panel, runs up the first column, down the
 * next and so on, to OUT at the bottom of the edge with the next one. Faces are named
 * as in the unfolded cross (Front = North):
 *
 *   0 Left (x == 0), 1 Back (y == 0), 2 Right (x == 7), 3 Front (y == 7),
 *   4 Bottom (z == 0), 5 Top (z == 7)
 *
 * A voxel on an edge or corner lights an LED on every face it touches;
 * inner voxels light nothing. */
#pragma once

#include <stdint.h>

#include "config.h"
#include "particle.h"

#define PANEL_COUNT 6
#define PANEL_LEDS  (CUBE_SIZE * CUBE_SIZE)

/* One frame: an RGB colour per LED, in chain order. */
typedef struct {
    uint8_t rgb[LED_COUNT * 3];
} frame_t;

/* LEDs showing a voxel, at most three (a corner). Returns how many. */
int panel_leds(cell_t cell, uint16_t leds[3]);

void frame_clear(frame_t *f);

/* Light every LED that shows the voxel. */
void frame_set(frame_t *f, cell_t cell, color_t color);

/* Light one LED of a panel (0-5, chain order), by where it sits seen from
 * outside the cube: column 0 on the left, row 0 at the bottom. The side
 * panels are upright and run on from one to the next round the cube; Top
 * and Bottom lie as in the unfolded cross, Top above Front and Bottom
 * below it. */
void frame_set_panel_led(frame_t *f, int panel, uint8_t col, uint8_t row, color_t color);

/* The face a panel sits on, e.g. "Back". */
const char *panel_name(int panel);
