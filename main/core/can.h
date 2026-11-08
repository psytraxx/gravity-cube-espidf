/* The can's LEDs: where each sits on the spiral, and frames that show the
 * fluid behind them.
 *
 * LED 0 sits on the +x side of the can (angle 0), on the bottom turn, or
 * the top one with CAN_START_AT_TOP. Each LED after it is a step of
 * 1 / CAN_LEDS_PER_TURN of a turn further round, counter-clockwise seen
 * from above (from +x towards +y), or clockwise with CAN_CLOCKWISE, and
 * climbs (or drops) CAN_ROW_PITCH per turn. */
#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "config.h"
#include "fluid.h"
#include "vec3.h"

/* One frame: an RGB colour per LED, in strip order. */
typedef struct {
    uint8_t rgb[LED_COUNT * 3];
} frame_t;

void frame_clear(frame_t *f);
void frame_set_led(frame_t *f, int led, color_t color);

/* Angle of LED `led` round the can in radians, counter-clockwise from +x
 * seen from above, in [0, 2 pi). */
float can_led_angle(int led);

/* Height of LED `led` above the floor, in cells. */
float can_led_height(int led);

/* Where LED `led` sits on the LED ring, in grid coordinates. */
vec3_t can_led_pos(int led);

/* Which LEDs show fluid. Each LED lights once the fluid density just
 * inside the wall in front of it reaches a threshold and goes dark again
 * below a lower one, so a still surface does not flicker. */
typedef struct {
    bool lit[LED_COUNT];
} can_view_t;

void can_view_init(can_view_t *v);

/* Light every LED with fluid behind it in `hue` plus the fluid's tint
 * there; the rest stay dark. */
void can_view_frame(can_view_t *v, const fluid_t *f, uint32_t hue, frame_t *frame);
