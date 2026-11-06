#include "panel.h"

#include <stdbool.h>
#include <string.h>

#define N    CUBE_SIZE
#define LAST (CUBE_SIZE - 1)

typedef enum {
    FACE_EAST,  /* x == 0 */
    FACE_WEST,  /* x == 7 */
    FACE_NORTH, /* y == 7 */
    FACE_SOUTH, /* y == 0 */
    FACE_DOWN,  /* z == 0 */
    FACE_UP,    /* z == 7 */
    FACE_COUNT,
} face_t;

typedef struct {
    uint8_t chain;     /* position in the LED chain */
    uint16_t rotation; /* 0, 90, 180 or 270 degrees */
    bool invert_x, invert_y;
} panel_t;

static const panel_t PANELS[FACE_COUNT] = {
    [FACE_EAST] = {.chain = 0, .rotation = 270, .invert_x = false, .invert_y = false},  /* Left */
    [FACE_SOUTH] = {.chain = 1, .rotation = 270, .invert_x = false, .invert_y = true},  /* Back */
    [FACE_WEST] = {.chain = 2, .rotation = 270, .invert_x = false, .invert_y = true},   /* Right */
    [FACE_NORTH] = {.chain = 3, .rotation = 270, .invert_x = false, .invert_y = false}, /* Front */
    [FACE_DOWN] = {.chain = 4, .rotation = 0, .invert_x = false, .invert_y = false},    /* Bottom */
    [FACE_UP] = {.chain = 5, .rotation = 90, .invert_x = false, .invert_y = true},      /* Top */
};

static panel_wiring_t s_wiring = PANEL_WIRING_ZIGZAG;

void panel_set_wiring(panel_wiring_t wiring)
{
    s_wiring = wiring;
}

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
    /* px runs along the zigzag's lines, py across them. */
    uint16_t i;
    if (s_wiring == PANEL_WIRING_ROWS) {
        /* Lines along py, all the same way. */
        i = (uint16_t)(py + px * N);
    } else {
        /* Lines along px, every other one backwards. */
        if (py & 1) {
            px = LAST - px;
        }
        i = (uint16_t)(px + py * N);
    }
    return (uint16_t)(p->chain * N * N + i);
}

/* The LED of a face that shows the voxel; the voxel must be on that face. */
static uint16_t face_led(face_t face, cell_t c)
{
    switch (face) {
    case FACE_EAST:
    case FACE_WEST:
        return led_index(face, c.y, c.z);
    case FACE_NORTH:
    case FACE_SOUTH:
        return led_index(face, c.x, c.z);
    default:
        return led_index(face, c.x, c.y);
    }
}

int panel_leds(cell_t c, uint16_t leds[3])
{
    int n = 0;
    if (c.x == 0) {
        leds[n++] = face_led(FACE_EAST, c);
    }
    if (c.x == LAST) {
        leds[n++] = face_led(FACE_WEST, c);
    }
    if (c.y == LAST) {
        leds[n++] = face_led(FACE_NORTH, c);
    }
    if (c.y == 0) {
        leds[n++] = face_led(FACE_SOUTH, c);
    }
    if (c.z == 0) {
        leds[n++] = face_led(FACE_DOWN, c);
    }
    if (c.z == LAST) {
        leds[n++] = face_led(FACE_UP, c);
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

cell_t panel_cell(int panel, uint8_t col, uint8_t row)
{
    switch (panel) {
    case 0: /* Left, seen from -x */
        return (cell_t){0, (uint8_t)(LAST - col), row};
    case 1: /* Back, seen from -y */
        return (cell_t){col, 0, row};
    case 2: /* Right, seen from +x */
        return (cell_t){LAST, col, row};
    case 3: /* Front, seen from +y */
        return (cell_t){(uint8_t)(LAST - col), LAST, row};
    case 4: /* Bottom, Front edge at the top */
        return (cell_t){(uint8_t)(LAST - col), row, 0};
    default: /* Top, Front edge at the bottom */
        return (cell_t){(uint8_t)(LAST - col), (uint8_t)(LAST - row), LAST};
    }
}

void frame_set_panel_led(frame_t *f, int panel, uint8_t col, uint8_t row, color_t color)
{
    /* Panels in chain order, faces in enum order. */
    static const face_t faces[PANEL_COUNT] = {FACE_EAST,  FACE_SOUTH, FACE_WEST,
                                              FACE_NORTH, FACE_DOWN,  FACE_UP};
    const uint16_t led = face_led(faces[panel], panel_cell(panel, col, row));
    uint8_t *px = &f->rgb[led * 3];
    px[0] = color.r;
    px[1] = color.g;
    px[2] = color.b;
}

const char *panel_name(int panel)
{
    static const char *const names[PANEL_COUNT] = {"Left",  "Back",   "Right",
                                                   "Front", "Bottom", "Top"};
    return names[panel];
}
