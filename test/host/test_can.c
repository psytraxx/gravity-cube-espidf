#include <math.h>

#include "can.h"
#include "mpu6500_decode.h"
#include "unity.h"

void setUp(void) {}
void tearDown(void) {}

#define PI 3.14159265f

/* LED 0 sits at angle 0 on the bottom turn; each whole turn later the
 * strip is back at the same angle, a row higher. */
static void test_spiral_climbs_one_row_per_turn(void)
{
    TEST_ASSERT_FLOAT_WITHIN(1e-5f, 0.0f, can_led_angle(0));
    TEST_ASSERT_FLOAT_WITHIN(1e-5f, 0.5f * CAN_ROW_PITCH_CELLS, can_led_height(0));
    for (int t = 1; t <= (int)CAN_TURNS; t++) {
        const int led = (int)lrintf(can_led_at_turns((float)t));
        const float a = can_led_angle(led);
        TEST_ASSERT_TRUE(a < 0.1f || a > 2.0f * PI - 0.1f);
        TEST_ASSERT_FLOAT_WITHIN(0.02f, (0.5f + (float)t) * CAN_ROW_PITCH_CELLS,
                                 can_led_height(led));
    }
    /* Counter-clockwise from above: a quarter turn on is on the +y side. */
    const vec3_t q = can_led_pos((int)lrintf(can_led_at_turns(0.25f)));
    TEST_ASSERT_GREATER_THAN_FLOAT(FLUID_NY / 2.0f + 0.9f * can_radius_at(q.z), q.y);
}

/* The bottom turn holds CAN_LEDS_PER_TURN_BOTTOM LEDs, the top one
 * CAN_LEDS_PER_TURN_TOP, and the strip ends after CAN_TURNS turns. */
static void test_turns_shorten_as_the_can_narrows(void)
{
    TEST_ASSERT_FLOAT_WITHIN(0.5f, CAN_LEDS_PER_TURN_BOTTOM, can_led_at_turns(1.0f));
    TEST_ASSERT_FLOAT_WITHIN(0.5f, CAN_LEDS_PER_TURN_TOP,
                             can_led_at_turns(CAN_TURNS) - can_led_at_turns(CAN_TURNS - 1.0f));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, (float)LED_COUNT, can_led_at_turns(CAN_TURNS));
    TEST_ASSERT_GREATER_THAN_FLOAT(can_radius_at(CAN_HEIGHT), can_radius_at(0.0f));
}

/* Every LED sits on the can's wall at its height, inside the grid. */
static void test_every_led_on_the_wall(void)
{
    for (int i = 0; i < LED_COUNT; i++) {
        const vec3_t p = can_led_pos(i);
        TEST_ASSERT_FLOAT_WITHIN(1e-3f, can_radius_at(p.z),
                                 hypotf(p.x - FLUID_NX / 2.0f, p.y - FLUID_NY / 2.0f));
        TEST_ASSERT_TRUE(p.z > 0.0f && p.z < CAN_HEIGHT);
        TEST_ASSERT_TRUE(p.x >= 0.0f && p.x <= FLUID_NX);
        TEST_ASSERT_TRUE(p.y >= 0.0f && p.y <= FLUID_NY);
    }
    TEST_ASSERT_TRUE(can_led_height(LED_COUNT - 1) > can_led_height(LED_COUNT - 2));
}

static int lit_count(const can_view_t *v)
{
    int n = 0;
    for (int i = 0; i < LED_COUNT; i++) {
        n += v->lit[i];
    }
    return n;
}

/* A settled half-full can lights about the bottom half of the strip, every
 * lit LED in the hue's colour, and the picture holds still. */
static void test_half_full_can_lights_bottom_half(void)
{
    static fluid_t f;
    fluid_init(&f);
    fluid_populate(&f);
    const vec3_t down = {0.0f, 0.0f, -1.0f};
    for (int i = 0; i < 300; i++) {
        fluid_step(&f, down);
    }
    can_view_t v;
    can_view_init(&v);
    frame_t frame;
    can_view_frame(&v, &f, FLUID_HUE, &frame);
    const int lit = lit_count(&v);
    TEST_ASSERT_INT_WITHIN(LED_COUNT / 8, LED_COUNT / 2, lit);
    TEST_ASSERT_TRUE(v.lit[0]);
    TEST_ASSERT_FALSE(v.lit[LED_COUNT - 1]);
    /* Green fluid: lit LEDs have green, dark ones nothing. */
    TEST_ASSERT_GREATER_THAN_UINT8(0, frame.rgb[1]);
    const int last = (LED_COUNT - 1) * 3;
    TEST_ASSERT_EQUAL_UINT8(0, frame.rgb[last] + frame.rgb[last + 1] + frame.rgb[last + 2]);

    int changes = 0;
    for (int s = 0; s < 100; s++) {
        bool before[LED_COUNT];
        for (int i = 0; i < LED_COUNT; i++) {
            before[i] = v.lit[i];
        }
        fluid_step(&f, down);
        can_view_frame(&v, &f, FLUID_HUE, &frame);
        for (int i = 0; i < LED_COUNT; i++) {
            changes += v.lit[i] != before[i];
        }
    }
    /* Fewer than one LED change in five steps. */
    TEST_ASSERT_LESS_THAN_INT(20, changes);
}

/* Tilted towards +x the fluid lights the +x side higher up than the -x
 * side. */
static void test_tilt_lights_the_low_side(void)
{
    static fluid_t f;
    fluid_init(&f);
    fluid_populate(&f);
    const vec3_t tilt = {0.7f, 0.0f, -0.7f};
    for (int i = 0; i < 300; i++) {
        fluid_step(&f, tilt);
    }
    can_view_t v;
    can_view_init(&v);
    frame_t frame;
    can_view_frame(&v, &f, FLUID_HUE, &frame);
    float east = 0.0f, west = 0.0f;
    for (int i = 0; i < LED_COUNT; i++) {
        if (!v.lit[i]) {
            continue;
        }
        const float a = can_led_angle(i);
        const float z = can_led_height(i);
        if (a < PI / 4.0f || a > 7.0f * PI / 4.0f) {
            east = fmaxf(east, z);
        } else if (a > 3.0f * PI / 4.0f && a < 5.0f * PI / 4.0f) {
            west = fmaxf(west, z);
        }
    }
    TEST_ASSERT_GREATER_THAN_FLOAT(west + 1.5f, east);
}

static bool is(const frame_t *f, int led, uint8_t r, uint8_t g, uint8_t b)
{
    return f->rgb[led * 3] == r && f->rgb[led * 3 + 1] == g && f->rgb[led * 3 + 2] == b;
}

/* Level: one red, green, blue and yellow LED per turn, a quarter turn
 * apart, nothing else. */
static void test_calibration_lines(void)
{
    static frame_t f;
    can_calibration_frame(&f, (vec3_t){0.0f, 0.0f, -1.0f});
    int count[4] = {0, 0, 0, 0}, other = 0;
    for (int i = 0; i < LED_COUNT; i++) {
        if (is(&f, i, 60, 0, 0)) {
            count[0]++;
        } else if (is(&f, i, 0, 60, 0)) {
            count[1]++;
        } else if (is(&f, i, 0, 0, 60)) {
            count[2]++;
        } else if (is(&f, i, 30, 30, 0)) {
            count[3]++;
        } else if (!is(&f, i, 0, 0, 0)) {
            other++;
        }
    }
    for (int q = 0; q < 4; q++) {
        TEST_ASSERT_INT_WITHIN(1, (int)CAN_TURNS, count[q]);
    }
    TEST_ASSERT_EQUAL_INT(0, other);
    TEST_ASSERT_TRUE(is(&f, 0, 60, 0, 0));
    TEST_ASSERT_TRUE(is(&f, (int)lrintf(can_led_at_turns(0.25f)), 0, 60, 0));
    TEST_ASSERT_TRUE(is(&f, (int)lrintf(can_led_at_turns(3.5f)), 0, 0, 60));
}

/* Tilted so the fluid falls towards +y: the white band lights LEDs on the
 * +y side, and none on the -y side. */
static void test_calibration_band_follows_tilt(void)
{
    static frame_t f;
    can_calibration_frame(&f, (vec3_t){0.0f, 0.5f, -0.85f});
    int plus_y = 0, minus_y = 0;
    for (int i = 0; i < LED_COUNT; i++) {
        if (!is(&f, i, 20, 20, 20)) {
            continue;
        }
        const float a = can_led_angle(i);
        plus_y += a > PI / 4.0f && a < 3.0f * PI / 4.0f;
        minus_y += a > 5.0f * PI / 4.0f && a < 7.0f * PI / 4.0f;
    }
    TEST_ASSERT_GREATER_THAN_INT(3 * (int)CAN_TURNS, plus_y);
    TEST_ASSERT_EQUAL_INT(0, minus_y);
}

/* At rest, board flat with Z up: +1 g on sensor Z pulls towards the floor,
 * +1 g on sensor X (X pointing up) towards -x, and the gyro keeps its
 * axes. */
static void test_decode_rest_and_spin(void)
{
    /* accel (0, 0, +8192) = +1 g on Z; gyro Z = +328 = +10 °/s. */
    const uint8_t raw[MPU6500_SAMPLE_BYTES] = {0, 0, 0, 0, 0x20, 0x00, 0, 0, 0, 0, 0x01, 0x48};
    const imu_sample_t s = mpu6500_decode(raw);
    TEST_ASSERT_FLOAT_WITHIN(1e-5f, 0.0f, s.accel.x);
    TEST_ASSERT_FLOAT_WITHIN(1e-5f, -1.0f, s.accel.z);
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, 10.0f * PI / 180.0f, s.gyro.z);
    const uint8_t up_x[MPU6500_SAMPLE_BYTES] = {0x20, 0x00, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    TEST_ASSERT_FLOAT_WITHIN(1e-5f, -1.0f, mpu6500_decode(up_x).accel.x);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_spiral_climbs_one_row_per_turn);
    RUN_TEST(test_turns_shorten_as_the_can_narrows);
    RUN_TEST(test_every_led_on_the_wall);
    RUN_TEST(test_half_full_can_lights_bottom_half);
    RUN_TEST(test_tilt_lights_the_low_side);
    RUN_TEST(test_calibration_lines);
    RUN_TEST(test_calibration_band_follows_tilt);
    RUN_TEST(test_decode_rest_and_spin);
    return UNITY_END();
}
