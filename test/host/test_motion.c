#include "motion.h"
#include "unity.h"

void setUp(void) {}
void tearDown(void) {}

static const vec3_t STILL = {0.0f, 0.0f, 0.0f};

static void test_steady_reading_passes_through(void)
{
    motion_filter_t m;
    motion_filter_init(&m, 0.05f, 3.0f);
    const vec3_t out = motion_filter_apply(&m, v3(0.0f, 0.0f, -1.0f), STILL);
    TEST_ASSERT_EQUAL_FLOAT(-1.0f, out.z);
    const vec3_t jolt = motion_filter_apply(&m, v3(0.5f, 0.0f, -1.0f), STILL);
    TEST_ASSERT_TRUE(jolt.x > 0.5f);
}

/* Tilted and held still: the amplified overshoot decays to the reading. */
static void test_settles_back_to_gravity(void)
{
    motion_filter_t m;
    motion_filter_init(&m, 0.05f, 3.0f);
    motion_filter_apply(&m, v3(0.0f, 0.0f, -1.0f), STILL);
    const vec3_t tilted = v3(1.0f, 0.0f, 0.0f);
    vec3_t out = motion_filter_apply(&m, tilted, STILL);
    TEST_ASSERT_TRUE(out.x > 1.0f);
    for (int i = 0; i < 300; i++) {
        out = motion_filter_apply(&m, tilted, STILL);
    }
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 1.0f, out.x);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.0f, out.z);
}

/* Tilt by 90 degrees about Y in 30 steps with the accelerometer agreeing
 * with the gyro: gravity follows the tilt and is never amplified. */
static void test_follows_gyro_tilt_without_overshoot(void)
{
    motion_filter_t m;
    motion_filter_init(&m, 0.05f, 3.0f);
    motion_filter_apply(&m, v3(0.0f, 0.0f, -1.0f), STILL);
    const int steps = 30;
    const float step = 1.5707963f / steps;
    for (int i = 1; i <= steps; i++) {
        const float a = step * i;
        const vec3_t reading = v3(sinf(a), 0.0f, -cosf(a));
        const vec3_t out = motion_filter_apply(&m, reading, v3(0.0f, step, 0.0f));
        TEST_ASSERT_FLOAT_WITHIN(0.02f, reading.x, out.x);
        TEST_ASSERT_FLOAT_WITHIN(0.02f, reading.z, out.z);
    }
}

static void test_rotate_quarter_turn(void)
{
    const vec3_t r = v3_rotate(v3(1.0f, 0.0f, 0.0f), v3(0.0f, 0.0f, 1.5707963f));
    TEST_ASSERT_FLOAT_WITHIN(1e-5f, 0.0f, r.x);
    TEST_ASSERT_FLOAT_WITHIN(1e-5f, 1.0f, r.y);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_steady_reading_passes_through);
    RUN_TEST(test_settles_back_to_gravity);
    RUN_TEST(test_follows_gyro_tilt_without_overshoot);
    RUN_TEST(test_rotate_quarter_turn);
    return UNITY_END();
}
