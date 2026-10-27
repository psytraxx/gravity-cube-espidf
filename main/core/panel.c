#include "panel.h"

#include <stdbool.h>
#include <string.h>

#define N    CUBE_SIZE
#define LAST (CUBE_SIZE - 1)

typedef enum {
    FACE_EAST,  /* x == 0 */
    FACE_WEST,  /* x == 7 */
    FACE_SOUTH, /* y == 7 */
    FACE_NORTH, /* y == 0 */
    FACE_UP,    /* z == 0 */
    FACE_DOWN,  /* z == 7 */
    FACE_COUNT,
} face_t;

typedef struct {
    uint8_t chain;       /* position in the LED chain */
    uint16_t rotation;   /* 0, 90, 180 or 270 degrees */
    bool invert_x, invert_y;
} panel_t;

static const panel_t PANELS[FACE_COUNT] = {
    [FACE_EAST] = {.chain = 0, .rotation = 0, .invert_x = true, .invert_y = true},   /* Left */
    [FACE_NORTH] = {.chain = 1, .rotation = 0, .invert_x = false, .invert_y = true}, /* Front */
    [FACE_WEST] = {.chain = 2, .rotation = 0, .invert_x = false, .invert_y = true},  /* Right */
    [FACE_SOUTH] = {.chain = 3, .rotation = 0, .invert_x = true, .invert_y = true},  /* Back */
    [FACE_UP] = {.chain = 4, .rotation = 0, .invert_x = false, .invert_y = false},   /* Top */
    [FACE_DOWN] = {.chain = 5, .rotation = 90, .invert_x = false, .invert_y = true}, /* Bottom */
};

static uint16_t led_index(face_t face, uint8_t x, uint8_t y)
{
    const panel_t *p = &PANELS[face];
    uint8_t px = x, py = y;
    switch (p->rotation) {
    case 90:
        px = LAST - y;
        py = x;
        break;
    case 180:
        px = LAST - x;
        py = LAST - y;
        break;
    case 270:
        px = y;
        py = LAST - x;
        break;
    default:
        break;
    }
    if (p->invert_x) {
        px = LAST - px;
    }
    if (p->invert_y) {
        py = LAST - py;
    }
    return (uint16_t)(p->chain * N * N + px + py * N);
}

int panel_leds(cell_t c, uint16_t leds[3])
{
    int n = 0;
    if (c.x == 0) {
        leds[n++] = led_index(FACE_EAST, c.y, c.z);
    }
    if (c.x == LAST) {
        leds[n++] = led_index(FACE_WEST, c.y, c.z);
    }
    if (c.y == LAST) {
        leds[n++] = led_index(FACE_SOUTH, c.x, c.z);
    }
    if (c.y == 0) {
        leds[n++] = led_index(FACE_NORTH, c.x, c.z);
    }
    if (c.z == 0) {
        leds[n++] = led_index(FACE_UP, c.x, c.y);
    }
    if (c.z == LAST) {
        leds[n++] = led_index(FACE_DOWN, c.x, c.y);
    }
    return n;
}

void frame_clear(frame_t *f)
{
    memset(f->rgb, 0, sizeof f->rgb);
}

void frame_set(frame_t *f, cell_t cell, color_t color)
{
    uint16_t leds[3];
    const int n = panel_leds(cell, leds);
    for (int i = 0; i < n; i++) {
        uint8_t *px = &f->rgb[leds[i] * 3];
        px[0] = color.r;
        px[1] = color.g;
        px[2] = color.b;
    }
}
