#include "can.h"

#include <math.h>
#include <string.h>

#include "color.h"

#define TWO_PI 6.2831853f

/* The LEDs look at the fluid half a cell inside the wall, where cells
 * along the wall have their centres. */
#define SAMPLE_INSET 0.5f

/* Density thresholds for lighting an LED and for turning it off again
 * (rest density is FLUID_PARTICLES_PER_CELL). */
#define LIGHT_ON  1.0f
#define LIGHT_OFF 0.4f

void frame_clear(frame_t *f)
{
    memset(f->rgb, 0, sizeof f->rgb);
}

void frame_set_led(frame_t *f, int led, color_t color)
{
    if (led < 0 || led >= LED_COUNT) {
        return;
    }
    f->rgb[led * 3] = color.r;
    f->rgb[led * 3 + 1] = color.g;
    f->rgb[led * 3 + 2] = color.b;
}

/* LEDs per turn where the strip starts, and the change in it per turn
 * on from there. */
#define START_LEDS  (CAN_START_AT_TOP ? CAN_LEDS_PER_TURN_TOP : CAN_LEDS_PER_TURN_BOTTOM)
#define END_LEDS    (CAN_START_AT_TOP ? CAN_LEDS_PER_TURN_BOTTOM : CAN_LEDS_PER_TURN_TOP)
#define LEDS_CHANGE ((END_LEDS - START_LEDS) / CAN_TURNS)

/* Turns from LED 0 to LED `led`. The LEDs up to t turns on number
 * START_LEDS * t + LEDS_CHANGE * t^2 / 2; this solves that for t (in the
 * form that stays exact when LEDS_CHANGE is 0). */
static float turns(int led)
{
    const float i = (float)led;
    return 2.0f * i / (START_LEDS + sqrtf(START_LEDS * START_LEDS + 2.0f * LEDS_CHANGE * i));
}

float can_led_at_turns(float t)
{
    return START_LEDS * t + 0.5f * LEDS_CHANGE * t * t;
}

float can_led_angle(int led)
{
    const float t = turns(led);
    const float frac = t - floorf(t);
    const float a = TWO_PI * (CAN_CLOCKWISE ? 1.0f - frac : frac);
    return a >= TWO_PI ? 0.0f : a;
}

float can_led_height(int led)
{
    const float up = (0.5f + turns(led)) * CAN_ROW_PITCH_CELLS;
    return CAN_START_AT_TOP ? CAN_HEIGHT - up : up;
}

vec3_t can_led_pos(int led)
{
    const float a = can_led_angle(led);
    const float z = can_led_height(led);
    const float r = can_radius_at(z);
    return v3(FLUID_NX / 2.0f + r * cosf(a), FLUID_NY / 2.0f + r * sinf(a), z);
}

void can_view_init(can_view_t *v)
{
    memset(v, 0, sizeof *v);
}

void can_view_frame(can_view_t *v, const fluid_t *f, uint32_t hue, float brightness, frame_t *frame)
{
    frame_clear(frame);
    for (int i = 0; i < LED_COUNT; i++) {
        const float a = can_led_angle(i);
        const float z = can_led_height(i);
        const float r = can_radius_at(z) - SAMPLE_INSET;
        const vec3_t at = v3(FLUID_NX / 2.0f + r * cosf(a), FLUID_NY / 2.0f + r * sinf(a), z);
        float tint;
        const float density = fluid_sample(f, at, &tint);
        v->lit[i] = density >= (v->lit[i] ? LIGHT_OFF : LIGHT_ON);
        if (v->lit[i]) {
            const int h = (int)hue + 360 + (int)lrintf(tint);
            frame_set_led(frame, i, color_scale(color_from_hue((uint32_t)h), brightness));
        }
    }
}

/* Calibration colours, at about the fluid's brightness. */
static const color_t CAL_LINES[4] = {{60, 0, 0}, {0, 60, 0}, {0, 0, 60}, {30, 30, 0}};
static const color_t CAL_DOWN = {20, 20, 20};
/* Tilt (horizontal g) below which the white band stays off, and its half
 * width. */
#define CAL_MIN_TILT 0.1f
#define CAL_BAND     (TWO_PI / 16.0f)

/* Angle from `a` to `b`, in [-pi, pi). */
static float angle_between(float a, float b)
{
    return fmodf(b - a + 1.5f * TWO_PI, TWO_PI) - 0.5f * TWO_PI;
}

void can_calibration_frame(frame_t *frame, vec3_t accel)
{
    frame_clear(frame);
    const bool tilted = accel.x * accel.x + accel.y * accel.y > CAL_MIN_TILT * CAL_MIN_TILT;
    const float down = atan2f(accel.y, accel.x);
    for (int i = 0; i < LED_COUNT; i++) {
        const float a = can_led_angle(i);
        if (tilted && fabsf(angle_between(down, a)) < CAL_BAND) {
            frame_set_led(frame, i, CAL_DOWN);
        }
        /* Half an LED's step round the turn at its height: one LED per
         * turn falls within it. */
        const float leds_per_turn = TWO_PI * CAN_CELL_LEDS * can_radius_at(can_led_height(i));
        const float half_step = 0.5f * TWO_PI / leds_per_turn;
        for (int q = 0; q < 4; q++) {
            if (fabsf(angle_between(0.25f * TWO_PI * (float)q, a)) <= half_step) {
                frame_set_led(frame, i, CAL_LINES[q]);
            }
        }
    }
}

void can_ruler_frame(frame_t *frame)
{
    static const color_t ZERO = {60, 0, 0}, HUNDRED = {0, 60, 0}, FIFTY = {0, 0, 60},
                         TEN = {20, 20, 20};
    frame_clear(frame);
    for (int i = 0; i < LED_COUNT; i += 10) {
        frame_set_led(frame, i, i == 0 ? ZERO : i % 100 == 0 ? HUNDRED : i % 50 == 0 ? FIFTY : TEN);
    }
}
