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

/* Turns from LED 0 to LED `led`. */
static float turns(int led)
{
    return (float)led / CAN_LEDS_PER_TURN;
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
    const float up = (0.5f + turns(led)) * CAN_ROW_PITCH;
    return CAN_START_AT_TOP ? CAN_HEIGHT - up : up;
}

vec3_t can_led_pos(int led)
{
    const float a = can_led_angle(led);
    return v3(FLUID_NX / 2.0f + CAN_RADIUS * cosf(a), FLUID_NY / 2.0f + CAN_RADIUS * sinf(a),
              can_led_height(led));
}

void can_view_init(can_view_t *v)
{
    memset(v, 0, sizeof *v);
}

void can_view_frame(can_view_t *v, const fluid_t *f, uint32_t hue, frame_t *frame)
{
    frame_clear(frame);
    const float r = CAN_RADIUS - SAMPLE_INSET;
    for (int i = 0; i < LED_COUNT; i++) {
        const float a = can_led_angle(i);
        const vec3_t at =
            v3(FLUID_NX / 2.0f + r * cosf(a), FLUID_NY / 2.0f + r * sinf(a), can_led_height(i));
        float tint;
        const float density = fluid_sample(f, at, &tint);
        v->lit[i] = density >= (v->lit[i] ? LIGHT_OFF : LIGHT_ON);
        if (v->lit[i]) {
            const int h = (int)hue + 360 + (int)lrintf(tint);
            frame_set_led(frame, i, color_from_hue((uint32_t)h));
        }
    }
}
