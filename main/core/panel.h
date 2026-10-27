/* Maps voxels on the cube's surface to LEDs in the chain, and builds
 * frames.
 *
 * Six 8x8 panels, chained OUT to IN in the WLED wiring: every panel has IN
 * at its bottom-left and scans row by row up to OUT at the top-right. Faces
 * are named as in the unfolded cross (Front = North):
 *
 *   0 Left (x == 0), 1 Front (y == 0), 2 Right (x == 7), 3 Back (y == 7),
 *   4 Top (z == 0), 5 Bottom (z == 7)
 *
 * A voxel on an edge or corner lights an LED on every face it touches;
 * inner voxels light nothing. */
#pragma once

#include <stdint.h>

#include "config.h"
#include "particle.h"

/* One frame: an RGB colour per LED, in chain order. */
typedef struct {
    uint8_t rgb[LED_COUNT * 3];
} frame_t;

/* LEDs showing a voxel, at most three (a corner). Returns how many. */
int panel_leds(cell_t cell, uint16_t leds[3]);

void frame_clear(frame_t *f);

/* Light every LED that shows the voxel. */
void frame_set(frame_t *f, cell_t cell, color_t color);
