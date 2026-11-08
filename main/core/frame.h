/* One frame: an RGB colour per LED, in chain order. */
#pragma once

#include <stdint.h>

#include "color.h"
#include "config.h"

typedef struct {
    uint8_t rgb[LED_COUNT * 3];
} frame_t;

void frame_clear(frame_t *f);

/* Set LED `led` (chain order); LEDs outside the chain are left alone. */
void frame_set_led(frame_t *f, int led, color_t color);
