#include <math.h>
#include <stdbool.h>
#include <string.h>

#include "fluid.h"
#include "unity.h"

#define N CUBE_SIZE

static fluid_t f;

void setUp(void)
{
    fluid_init(&f);
    fluid_populate(&f, FLUID_PARTICLES);
}

void tearDown(void) {}

static const vec3_t NO_GRAVITY = {0.0f, 0.0f, 0.0f};
static const vec3_t DOWN = {0.0f, 0.0f, -1.0f};

/* Half the cube's voxels. */
#define HALF_FULL (N * N * N / 2)

static void run(int steps, vec3_t accel)
{
    for (int i = 0; i < steps; i++) {
        fluid_step(&f, accel);
    }
}

static int lit_count(void)
{
    int n = 0;
    for (uint8_t z = 0; z < N; z++) {
        for (uint8_t y = 0; y < N; y++) {
            for (uint8_t x = 0; x < N; x++) {
                n += fluid_lit(&f, (cell_t){x, y, z});
            }
        }
    }
    return n;
}

static vec3_t mean_position(void)
{
    vec3_t sum = {0.0f, 0.0f, 0.0f};
    for (int i = 0; i < f.n; i++) {
        sum = v3_add(sum, f.p[i].pos);
    }
    return v3_scale(sum, 1.0f / (float)f.n);
}

static void test_populate_fills_bottom_half(void)
{
    TEST_ASSERT_EQUAL_INT(FLUID_PARTICLES, f.n);
    TEST_ASSERT_EQUAL_INT(HALF_FULL, lit_count());
    TEST_ASSERT_TRUE(fluid_lit(&f, (cell_t){0, 0, 0}));
    TEST_ASSERT_TRUE(fluid_lit(&f, (cell_t){7, 7, 3}));
    TEST_ASSERT_FALSE(fluid_lit(&f, (cell_t){0, 0, 4}));
    TEST_ASSERT_FALSE(fluid_lit(&f, (cell_t){7, 7, 7}));
}

/* At rest the fluid stays in the bottom half, keeps its volume, and the
 * lit voxels hold still. */
static void test_settles_keeping_volume_without_flicker(void)
{
    run(300, DOWN);
    const int lit = lit_count();
    TEST_ASSERT_INT_WITHIN(HALF_FULL / 10, HALF_FULL, lit);
    TEST_ASSERT_FLOAT_WITHIN(0.3f, N / 4.0f, mean_position().z);

    bool before[FLUID_CELLS];
    memcpy(before, f.lit, sizeof before);
    int changes = 0;
    for (int i = 0; i < 100; i++) {
        fluid_step(&f, DOWN);
        for (int c = 0; c < FLUID_CELLS; c++) {
            changes += f.lit[c] != before[c];
        }
        memcpy(before, f.lit, sizeof before);
    }
    /* Fewer than one voxel change in five steps. */
    TEST_ASSERT_LESS_THAN_INT(20, changes);
}

/* Gravity along +x: the fluid flows to the x == 7 side and keeps its
 * volume there. */
static void test_tilt_flows_downhill(void)
{
    run(200, (vec3_t){1.0f, 0.0f, 0.0f});
    TEST_ASSERT_FLOAT_WITHIN(0.4f, N * 0.75f, mean_position().x);
    TEST_ASSERT_TRUE(fluid_lit(&f, (cell_t){7, 0, 7}));
    TEST_ASSERT_FALSE(fluid_lit(&f, (cell_t){0, 0, 0}));
    TEST_ASSERT_INT_WITHIN(HALF_FULL / 10, HALF_FULL, lit_count());
}

static void test_stays_inside_cube_whichever_way_gravity_points(void)
{
    const vec3_t dirs[] = {
        {1, 0, 0}, {-1, 0, 0}, {0, 1, 0},       {0, -1, 0},
        {0, 0, 1}, {0, 0, -1}, {0.6f, -0.6f, 0.5f}, {-3, 2, 1},
    };
    for (size_t d = 0; d < sizeof dirs / sizeof dirs[0]; d++) {
        run(40, dirs[d]);
        for (int i = 0; i < f.n; i++) {
            const vec3_t p = f.p[i].pos;
            TEST_ASSERT_TRUE(p.x >= 0.0f && p.x <= N);
            TEST_ASSERT_TRUE(p.y >= 0.0f && p.y <= N);
            TEST_ASSERT_TRUE(p.z >= 0.0f && p.z <= N);
        }
    }
}

/* After a step, flow into and out of every cell that held fluid when the
 * step started balances. */
static void test_pressure_solve_removes_divergence(void)
{
    const vec3_t tilted = {0.7f, 0.0f, -0.7f};
    run(20, tilted);
    /* The solve sees the cells and density as they are before it moves the
     * particles. */
    bool fluid[FLUID_CELLS] = {false};
    for (int i = 0; i < f.n; i++) {
        const vec3_t p = f.p[i].pos;
        fluid[(int)p.x + N * ((int)p.y + N * (int)p.z)] = true;
    }
    float density[FLUID_CELLS];
    memcpy(density, f.density, sizeof density);
    fluid_step(&f, tilted);

    float worst = 0.0f;
    for (int k = 0; k < N; k++) {
        for (int j = 0; j < N; j++) {
            for (int i = 0; i < N; i++) {
                const int cell = i + N * (j + N * k);
                /* Cells packed above rest density get outflow on purpose. */
                if (!fluid[cell] || density[cell] > FLUID_PARTICLES_PER_CELL) {
                    continue;
                }
                const float div = f.vel[0][(i + 1) + (N + 1) * (j + N * k)] -
                                  f.vel[0][i + (N + 1) * (j + N * k)] +
                                  f.vel[1][i + N * ((j + 1) + (N + 1) * k)] -
                                  f.vel[1][i + N * (j + (N + 1) * k)] +
                                  f.vel[2][i + N * (j + N * (k + 1))] - f.vel[2][i + N * (j + N * k)];
                worst = fmaxf(worst, fabsf(div));
            }
        }
    }
    TEST_ASSERT_LESS_THAN_FLOAT(0.05f, worst);
}

/* The cube starts spinning about z: the fluid lags behind (on the +x side
 * it moves towards -y, on the +y side towards +x), then is flung out. */
static void test_spin_lags_and_flings_outwards(void)
{
    float radius_before = 0.0f;
    for (int i = 0; i < f.n; i++) {
        radius_before += hypotf(f.p[i].pos.x - N / 2.0f, f.p[i].pos.y - N / 2.0f);
    }

    fluid_set_rotation(&f, (vec3_t){0.0f, 0.0f, 0.15f});
    fluid_step(&f, NO_GRAVITY);
    float vy_east = 0.0f, vx_north = 0.0f;
    for (int i = 0; i < f.n; i++) {
        const vec3_t p = f.p[i].pos;
        if (p.x > 6.0f && p.y > 2.0f && p.y < 6.0f) {
            vy_east += f.p[i].vel.y;
        }
        if (p.y > 6.0f && p.x > 2.0f && p.x < 6.0f) {
            vx_north += f.p[i].vel.x;
        }
    }
    TEST_ASSERT_LESS_THAN_FLOAT(0.0f, vy_east);
    TEST_ASSERT_GREATER_THAN_FLOAT(0.0f, vx_north);

    run(200, NO_GRAVITY);
    float radius_after = 0.0f;
    for (int i = 0; i < f.n; i++) {
        radius_after += hypotf(f.p[i].pos.x - N / 2.0f, f.p[i].pos.y - N / 2.0f);
    }
    TEST_ASSERT_GREATER_THAN_FLOAT(radius_before * 1.05f, radius_after);
}

/* Tints run diagonally across the fluid, inside the spread, and moving
 * fluid shifts hue. */
static void test_tint_varies_across_fluid_and_with_speed(void)
{
    const float corner = fluid_tint(&f, (cell_t){0, 0, 0});
    const float far = fluid_tint(&f, (cell_t){7, 7, 3});
    TEST_ASSERT_LESS_THAN_FLOAT(far - 30.0f, corner);
    TEST_ASSERT_GREATER_OR_EQUAL_FLOAT(-FLUID_TINT_SPREAD, corner);
    TEST_ASSERT_LESS_OR_EQUAL_FLOAT(FLUID_TINT_SPREAD, far);

    const float still = fluid_tint(&f, (cell_t){3, 3, 1});
    for (int i = 0; i < f.n; i++) {
        f.p[i].vel = (vec3_t){0.0f, 0.0f, 0.0f};
    }
    fluid_step(&f, (vec3_t){3.0f, 0.0f, 0.0f});
    TEST_ASSERT_GREATER_THAN_FLOAT(still + 5.0f, fluid_tint(&f, (cell_t){3, 3, 1}));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_populate_fills_bottom_half);
    RUN_TEST(test_settles_keeping_volume_without_flicker);
    RUN_TEST(test_tilt_flows_downhill);
    RUN_TEST(test_stays_inside_cube_whichever_way_gravity_points);
    RUN_TEST(test_pressure_solve_removes_divergence);
    RUN_TEST(test_spin_lags_and_flings_outwards);
    RUN_TEST(test_tint_varies_across_fluid_and_with_speed);
    return UNITY_END();
}
