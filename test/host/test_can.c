#include <math.h>

#include "can.h"
#include "mpu6500_decode.h"
#include "unity.h"

void setUp(void) {}
void tearDown(void) {}

#define PI 3.14159265f

/* LED 0 sits at angle 0 on the bottom turn; one turn later the strip is
 * back at the same angle, a row higher. */
static void test_spiral_climbs_one_row_per_turn(void)
{
    TEST_ASSERT_FLOAT_WITHIN(1e-5f, 0.0f, can_led_angle(0));
    TEST_ASSERT_FLOAT_WITHIN(1e-5f, 0.5f * CAN_ROW_PITCH, can_led_height(0));
    /* 200 LEDs in 13 turns: LED 200 / 13 * 2 would be two turns on. */
    const int two_turns = (int)lrintf(2.0f * CAN_LEDS_PER_TURN);
    const float a = can_led_angle(two_turns);
    TEST_ASSERT_TRUE(a < 0.3f || a > 2.0f * PI - 0.3f);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 2.5f * CAN_ROW_PITCH, can_led_height(two_turns));
    /* Counter-clockwise from above: a quarter turn on is on the +y side. */
    const vec3_t q = can_led_pos((int)lrintf(CAN_LEDS_PER_TURN / 4.0f));
    TEST_ASSERT_GREATER_THAN_FLOAT(FLUID_NY / 2.0f + 0.8f * CAN_RADIUS, q.y);
}

/* Every LED sits on the ring, inside the grid and the can's height. */
static void test_every_led_on_the_ring(void)
{
    for (int i = 0; i < LED_COUNT; i++) {
        const vec3_t p = can_led_pos(i);
        TEST_ASSERT_FLOAT_WITHIN(1e-3f, CAN_RADIUS,
                                 hypotf(p.x - FLUID_NX / 2.0f, p.y - FLUID_NY / 2.0f));
        TEST_ASSERT_TRUE(p.z > 0.0f && p.z < CAN_HEIGHT);
        TEST_ASSERT_TRUE(p.x >= 0.0f && p.x <= FLUID_NX);
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
    TEST_ASSERT_GREATER_THAN_FLOAT(west + 2.0f, east);
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
    RUN_TEST(test_every_led_on_the_ring);
    RUN_TEST(test_half_full_can_lights_bottom_half);
    RUN_TEST(test_tilt_lights_the_low_side);
    RUN_TEST(test_decode_rest_and_spin);
    return UNITY_END();
}
