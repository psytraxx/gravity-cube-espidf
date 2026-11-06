#include "assembly.h"

#include <stdbool.h>

#define LAST (CUBE_SIZE - 1)

static const color_t COLORS[PANEL_COUNT] = {
    {60, 0, 0},   /* Left: red */
    {0, 60, 0},   /* Back: green */
    {0, 0, 60},   /* Right: blue */
    {60, 60, 0},  /* Front: yellow */
    {60, 0, 60},  /* Bottom: magenta */
    {40, 40, 40}, /* Top: white */
};

static const char *const NAMES[PANEL_COUNT] = {"red",    "green",   "blue",
                                               "yellow", "magenta", "white"};

color_t assembly_color(int panel)
{
    return COLORS[panel];
}

const char *assembly_color_name(int panel)
{
    return NAMES[panel];
}

void assembly_frame(frame_t *f)
{
    frame_clear(f);
    for (int p = 0; p < PANEL_COUNT; p++) {
        for (uint8_t row = 0; row < CUBE_SIZE; row++) {
            for (uint8_t col = 0; col < CUBE_SIZE; col++) {
                const bool edge_col = col == 0 || col == LAST;
                const bool edge_row = row == 0 || row == LAST;
                if (edge_col && edge_row) {
                    continue; /* corner */
                }
                if (edge_col || edge_row) {
                    /* The voxel under an edge LED touches this panel and
                     * the neighbour; its other LED is on the neighbour. */
                    uint16_t leds[3];
                    const int n = panel_leds(panel_cell(p, col, row), leds);
                    for (int i = 0; i < n; i++) {
                        const int other = leds[i] / PANEL_LEDS;
                        if (other != p) {
                            frame_set_panel_led(f, p, col, row, COLORS[other]);
                        }
                    }
                } else if (col >= 2 && col <= 5 && row >= 2 && row <= 5) {
                    frame_set_panel_led(f, p, col, row, COLORS[p]);
                }
            }
        }
    }
}
