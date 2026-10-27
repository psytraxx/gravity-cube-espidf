#include <stdbool.h>
#include <string.h>

#include "fluid.h"
#include "unity.h"

static fluid_t f;

void setUp(void)
{
    fluid_init(&f);
}

void tearDown(void) {}

static const vec3_t NO_GRAVITY = {0.0f, 0.0f, 0.0f};
static const vec3_t DOWN = {0.0f, 1.0f, 0.0f};

static void assert_in_cube(vec3_t p)
{
    TEST_ASSERT_TRUE(p.x >= 0.0f && p.x <= CUBE_BOUND);
    TEST_ASSERT_TRUE(p.y >= 0.0f && p.y <= CUBE_BOUND);
    TEST_ASSERT_TRUE(p.z >= 0.0f && p.z <= CUBE_BOUND);
}

static void test_populate_fills_bottom_half(void)
{
    fluid_populate(&f, FLUID_PARTICLES);
    TEST_ASSERT_EQUAL_INT(256, f.n);
    for (int i = 0; i < f.n; i++) {
        TEST_ASSERT_TRUE(f.p[i].pos.y >= 4.0f && f.p[i].pos.y < 8.0f);
    }
    fluid_populate(&f, 4);
    TEST_ASSERT_EQUAL_INT(4, f.n);
}

static void test_gravity_moves_particle_down(void)
{
    fluid_add(&f, particle_at(3.5f, 3.5f, 3.5f));
    for (int i = 0; i < 10; i++) {
        fluid_step(&f, DOWN);
    }
    TEST_ASSERT_TRUE(f.p[0].pos.y > 3.5f);
}

static void test_stays_inside_cube_whichever_way_gravity_points(void)
{
    const vec3_t dirs[] = {{1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1}};
    fluid_add(&f, particle_at(3.5f, 3.5f, 3.5f));
    for (size_t d = 0; d < sizeof dirs / sizeof dirs[0]; d++) {
        f.p[0] = particle_at(3.5f, 3.5f, 3.5f);
        for (int i = 0; i < 100; i++) {
            fluid_step(&f, dirs[d]);
        }
        assert_in_cube(f.p[0].pos);
    }
}

static void test_velocity_is_clamped(void)
{
    particle_t p = particle_at(3.5f, 3.5f, 3.5f);
    p.vel = v3(10.0f, 10.0f, 10.0f);
    fluid_add(&f, p);
    fluid_step(&f, NO_GRAVITY);
    TEST_ASSERT_TRUE(fabsf(f.p[0].vel.x) <= FLUID_MAX_VELOCITY);
    TEST_ASSERT_TRUE(fabsf(f.p[0].vel.y) <= FLUID_MAX_VELOCITY);
    TEST_ASSERT_TRUE(fabsf(f.p[0].vel.z) <= FLUID_MAX_VELOCITY);
}

static void test_overlapping_particles_separate(void)
{
    fluid_add(&f, particle_at(3.5f, 3.5f, 3.5f));
    fluid_add(&f, particle_at(3.6f, 3.5f, 3.5f));
    for (int i = 0; i < 50; i++) {
        fluid_step(&f, NO_GRAVITY);
    }
    TEST_ASSERT_TRUE(f.p[1].pos.x - f.p[0].pos.x > 0.1f);
}

static void test_velocity_decays_without_gravity(void)
{
    particle_t p = particle_at(3.5f, 3.5f, 3.5f);
    p.vel = v3(0.5f, 0.5f, 0.5f);
    fluid_add(&f, p);
    const float start = particle_speed(&f.p[0]);
    for (int i = 0; i < 20; i++) {
        fluid_step(&f, NO_GRAVITY);
    }
    TEST_ASSERT_TRUE(particle_speed(&f.p[0]) < start);
}

static void test_particle_on_floor_comes_to_rest(void)
{
    fluid_add(&f, particle_at(3.0f, 3.0f, 3.0f));
    for (int i = 0; i < 200; i++) {
        fluid_step(&f, DOWN);
    }
    TEST_ASSERT_EQUAL_UINT8(7, particle_cell(&f.p[0]).y);
    TEST_ASSERT_TRUE(particle_speed(&f.p[0]) < 0.01f);
}

static void occupied(bool cells[512])
{
    memset(cells, 0, 512);
    for (int i = 0; i < f.n; i++) {
        const cell_t c = fluid_cell(&f, i);
        cells[c.x + c.y * 8 + c.z * 64] = true;
    }
}

/* Tilted gravity so the pile has to flow before it settles; once settled,
 * no LED may change. */
static void test_full_cube_settles_without_flicker(void)
{
    const vec3_t tilted = {0.3f, 1.0f, 0.2f};
    fluid_populate(&f, FLUID_PARTICLES);
    for (int i = 0; i < 600; i++) {
        fluid_step(&f, tilted);
    }
    bool prev[512], cur[512];
    occupied(prev);
    int changed = 0;
    for (int i = 0; i < 100; i++) {
        fluid_step(&f, tilted);
        occupied(cur);
        for (int c = 0; c < 512; c++) {
            changed += prev[c] != cur[c];
        }
        memcpy(prev, cur, sizeof prev);
    }
    TEST_ASSERT_EQUAL_INT(0, changed);
}

static void test_spin_lags_and_flings_outwards(void)
{
    /* One particle on the +X side of the centre (3.5), no gravity. Spin
     * about +Z: a point on +X moves towards +Y with the cube. */
    fluid_add(&f, particle_at(5.0f, 3.5f, 3.5f));
    fluid_set_rotation(&f, v3(0.0f, 0.0f, 0.05f));
    fluid_step(&f, NO_GRAVITY);
    TEST_ASSERT_TRUE(f.p[0].pos.y < 3.5f);
    TEST_ASSERT_TRUE(f.p[0].pos.x > 5.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 3.5f, f.p[0].pos.z);
}

static void test_particle_on_spin_axis_stays_put(void)
{
    fluid_add(&f, particle_at(3.5f, 3.5f, 1.0f));
    fluid_set_rotation(&f, v3(0.0f, 0.0f, 0.05f));
    for (int i = 0; i < 10; i++) {
        fluid_step(&f, NO_GRAVITY);
    }
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 3.5f, f.p[0].pos.x);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 3.5f, f.p[0].pos.y);
}

static void test_cell_rounds_and_clamps(void)
{
    const particle_t a = particle_at(2.7f, 5.3f, 7.9f);
    const cell_t ca = particle_cell(&a);
    TEST_ASSERT_EQUAL_UINT8(3, ca.x);
    TEST_ASSERT_EQUAL_UINT8(5, ca.y);
    TEST_ASSERT_EQUAL_UINT8(7, ca.z);
    const particle_t b = particle_at(-1.0f, 10.0f, 3.2f);
    const cell_t cb = particle_cell(&b);
    TEST_ASSERT_EQUAL_UINT8(0, cb.x);
    TEST_ASSERT_EQUAL_UINT8(7, cb.y);
    TEST_ASSERT_EQUAL_UINT8(3, cb.z);
}

/* The displayed cell only moves once the particle is clearly past the
 * boundary. */
static void test_display_hysteresis(void)
{
    fluid_add(&f, particle_at(3.0f, 3.0f, 3.0f));
    fluid_step(&f, NO_GRAVITY);
    f.p[0].pos.x = 3.6f; /* nearest cell is 4, but within hysteresis of 3 */
    f.p[0].vel = v3(0.0f, 0.0f, 0.0f);
    fluid_step(&f, NO_GRAVITY);
    TEST_ASSERT_EQUAL_UINT8(3, fluid_cell(&f, 0).x);
    f.p[0].pos.x = 3.7f;
    f.p[0].vel = v3(0.0f, 0.0f, 0.0f);
    fluid_step(&f, NO_GRAVITY);
    TEST_ASSERT_EQUAL_UINT8(4, fluid_cell(&f, 0).x);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_populate_fills_bottom_half);
    RUN_TEST(test_gravity_moves_particle_down);
    RUN_TEST(test_stays_inside_cube_whichever_way_gravity_points);
    RUN_TEST(test_velocity_is_clamped);
    RUN_TEST(test_overlapping_particles_separate);
    RUN_TEST(test_velocity_decays_without_gravity);
    RUN_TEST(test_particle_on_floor_comes_to_rest);
    RUN_TEST(test_full_cube_settles_without_flicker);
    RUN_TEST(test_spin_lags_and_flings_outwards);
    RUN_TEST(test_particle_on_spin_axis_stays_put);
    RUN_TEST(test_cell_rounds_and_clamps);
    RUN_TEST(test_display_hysteresis);
    return UNITY_END();
}
