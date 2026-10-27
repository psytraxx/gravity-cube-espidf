/* The cube's WS2812 chain, through Espressif's led_strip component on
 * one RMT channel. */
#pragma once

#include "esp_err.h"
#include "panel.h"

/* Set up the RMT channel. Its interrupt runs on the calling core. */
esp_err_t ws2812_init(int gpio);

/* Send a frame and wait until it is out (about 11.5 ms for 384 LEDs).
 * An unchanged frame is skipped, so a settled cube keeps the data line
 * quiet. */
esp_err_t ws2812_show(const frame_t *frame);
