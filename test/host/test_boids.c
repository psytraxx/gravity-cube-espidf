#include "boids.h"
#include "effect.h"
#include "unity.h"

static boids_t b;

void setUp(void)
{
    boids_init(&b);
}

void tearDown(void) {}

static void test_populate_spreads_moving_boids(void)
{
    boids_populate(&b, 10);
    TEST_ASSERT_EQUAL_INT(10, b.n);
    bool moving = false;
    for (int i = 0; i < b.n; i++) {
        TEST_ASSERT_TRUE(b.p[i].pos.x >= 0.0f && b.p[i].pos.x < 8.0f);
        TEST_ASSERT_TRUE(b.p[i].pos.y >= 0.0f && b.p[i].pos.y < 8.0f);
        TEST_ASSERT_TRUE(b.p[i].pos.z >= 0.0f && b.p[i].pos.z < 8.0f);
        moving |= particle_speed(&b.p[i]) > 0.01f;
    }
    TEST_ASSERT_TRUE(moving);
}

static void test_boids_move(void)
{
    boids_populate(&b, 5);
    vec3_t start[5];
    for (int i = 0; i < 5; i++) {
        start[i] = b.p[i].pos;
    }
    for (int s = 0; s < 10; s++) {
        boids_step(&b, v3(0.0f, 0.0f, 0.0f));
    }
    bool moved = false;
    for (int i = 0; i < 5; i++) {
        moved |= v3_len(v3_sub(b.p[i].pos, start[i])) > 0.1f;
    }
    TEST_ASSERT_TRUE(moved);
}

static void test_stay_in_bounds_and_under_speed_limit(void)
{
    boids_populate(&b, 10);
    for (int s = 0; s < 100; s++) {
        boids_step(&b, v3(2.0f, 2.0f, 2.0f));
    }
    for (int i = 0; i < b.n; i++) {
        const vec3_t p = b.p[i].pos;
        TEST_ASSERT_TRUE(p.x >= 0.0f && p.x <= CUBE_BOUND);
        TEST_ASSERT_TRUE(p.y >= 0.0f && p.y <= CUBE_BOUND);
        TEST_ASSERT_TRUE(p.z >= 0.0f && p.z <= CUBE_BOUND);
        TEST_ASSERT_TRUE(particle_speed(&b.p[i]) <= BOIDS_MAX_SPEED * 1.001f);
    }
}

static void test_spin_sweeps_boid_against_spin(void)
{
    b.p[0] = particle_at(5.0f, 3.5f, 3.5f);
    b.n = 1;
    boids_set_rotation(&b, v3(0.0f, 0.0f, 0.05f));
    boids_step(&b, v3(0.0f, 0.0f, 0.0f));
    TEST_ASSERT_TRUE(b.p[0].vel.y < 0.0f);
    TEST_ASSERT_TRUE(b.p[0].vel.x > 0.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, b.p[0].vel.z);
}

static effect_t e;

static void test_effect_switches_with_its_own_settings(void)
{
    effect_init(&e);
    TEST_ASSERT_EQUAL_STRING("fluid", effect_name(&e));
    TEST_ASSERT_EQUAL_INT(FLUID_PARTICLES, effect_count(&e));

    effect_next(&e);
    TEST_ASSERT_EQUAL_STRING("boids", effect_name(&e));
    TEST_ASSERT_EQUAL_INT(BOIDS_PARTICLES, effect_count(&e));
    TEST_ASSERT_EQUAL_UINT8(BOIDS_COLOR.r, effect_color(&e).r);

    effect_next(&e);
    TEST_ASSERT_EQUAL_STRING("fluid", effect_name(&e));
    TEST_ASSERT_EQUAL_INT(FLUID_PARTICLES, effect_count(&e));
    TEST_ASSERT_EQUAL_UINT8(FLUID_COLOR.g, effect_color(&e).g);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_populate_spreads_moving_boids);
    RUN_TEST(test_boids_move);
    RUN_TEST(test_stay_in_bounds_and_under_speed_limit);
    RUN_TEST(test_spin_sweeps_boid_against_spin);
    RUN_TEST(test_effect_switches_with_its_own_settings);
    return UNITY_END();
}
