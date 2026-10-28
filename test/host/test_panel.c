#include <stdbool.h>

#include "mpu6500_decode.h"
#include "panel.h"
#include "unity.h"

void setUp(void) {}
void tearDown(void) {}

static bool lights(uint8_t x, uint8_t y, uint8_t z, uint16_t led)
{
    uint16_t leds[3];
    const int n = panel_leds((cell_t){x, y, z}, leds);
    for (int i = 0; i < n; i++) {
        if (leds[i] == led) {
            return true;
        }
    }
    return false;
}

static void test_every_led_is_hit_exactly_once(void)
{
    uint8_t hits[LED_COUNT] = {0};
    for (uint8_t x = 0; x < 8; x++) {
        for (uint8_t y = 0; y < 8; y++) {
            for (uint8_t z = 0; z < 8; z++) {
                uint16_t leds[3];
                const int n = panel_leds((cell_t){x, y, z}, leds);
                for (int i = 0; i < n; i++) {
                    TEST_ASSERT_TRUE(leds[i] < LED_COUNT);
                    hits[leds[i]]++;
                }
            }
        }
    }
    for (int i = 0; i < LED_COUNT; i++) {
        TEST_ASSERT_EQUAL_UINT8(1, hits[i]);
    }
}

static void test_chain_corners_match_wiring(void)
{
    /* Left OUT (LED 63) and Back IN (64) share the bottom of the Left/Back
     * edge. */
    TEST_ASSERT_TRUE(lights(0, 0, 0, 63));
    TEST_ASSERT_TRUE(lights(0, 0, 0, 64));
    /* Zigzag: Back runs up its first column (65 above IN) and down the
     * second (71 at the bottom, next to 64's column). */
    TEST_ASSERT_TRUE(lights(0, 0, 1, 65));
    TEST_ASSERT_TRUE(lights(1, 0, 7, 72));
    TEST_ASSERT_TRUE(lights(1, 0, 0, 79));
    /* Back OUT (127) and Right IN (128) on the Back/Right edge. */
    TEST_ASSERT_TRUE(lights(7, 0, 0, 127));
    TEST_ASSERT_TRUE(lights(7, 0, 0, 128));
    /* Front OUT (255) and Left IN (0) at the bottom of the Front/Left
     * edge. */
    TEST_ASSERT_TRUE(lights(0, 7, 0, 255));
    TEST_ASSERT_TRUE(lights(0, 7, 0, 0));
    /* Bottom IN (256) is in the Left/Back/Bottom corner. */
    TEST_ASSERT_TRUE(lights(0, 0, 0, 256));
}

static void test_inner_voxel_lights_nothing(void)
{
    uint16_t leds[3];
    TEST_ASSERT_EQUAL_INT(0, panel_leds((cell_t){3, 4, 5}, leds));
    TEST_ASSERT_EQUAL_INT(3, panel_leds((cell_t){7, 7, 7}, leds));
}

static void test_frame_set_lights_every_face(void)
{
    frame_t f;
    frame_clear(&f);
    frame_set(&f, (cell_t){0, 0, 0}, (color_t){1, 2, 3});
    /* Back IN (64), Bottom IN (256) and Left OUT (63). */
    TEST_ASSERT_EQUAL_UINT8(1, f.rgb[64 * 3]);
    TEST_ASSERT_EQUAL_UINT8(2, f.rgb[64 * 3 + 1]);
    TEST_ASSERT_EQUAL_UINT8(3, f.rgb[64 * 3 + 2]);
    TEST_ASSERT_EQUAL_UINT8(1, f.rgb[256 * 3]);
    TEST_ASSERT_EQUAL_UINT8(1, f.rgb[63 * 3]);
    TEST_ASSERT_EQUAL_UINT8(0, f.rgb[0]);
}

/* At rest, board flat with Z up: +1 g on sensor Z pulls towards cube -Z. */
static void test_decode_rest_and_spin(void)
{
    /* accel (0, 0, +8192) = +1 g on Z; gyro X = +328 = +10 °/s. */
    const uint8_t raw[MPU6500_SAMPLE_BYTES] = {0, 0, 0, 0, 0x20, 0x00, 0x01, 0x48, 0, 0, 0, 0};
    const imu_sample_t s = mpu6500_decode(raw);
    TEST_ASSERT_FLOAT_WITHIN(1e-5f, 0.0f, s.accel.x);
    TEST_ASSERT_FLOAT_WITHIN(1e-5f, -1.0f, s.accel.z);
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, -10.0f * 3.14159265f / 180.0f, s.gyro.x);
    /* Negative values are two's complement; -1 g on sensor X (Right panel
     * down) pulls towards cube +X. */
    const uint8_t neg[MPU6500_SAMPLE_BYTES] = {0xE0, 0x00, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    TEST_ASSERT_FLOAT_WITHIN(1e-5f, 1.0f, mpu6500_decode(neg).accel.x);
}

/* The one LED frame_set_panel_led lights. */
static uint16_t panel_led(int panel, uint8_t col, uint8_t row)
{
    static frame_t f;
    frame_clear(&f);
    frame_set_panel_led(&f, panel, col, row, (color_t){0, 60, 0});
    int lit = -1;
    for (int i = 0; i < LED_COUNT; i++) {
        if (f.rgb[i * 3 + 1] != 0) {
            TEST_ASSERT_EQUAL_INT(-1, lit);
            lit = i;
        }
    }
    TEST_ASSERT_NOT_EQUAL_INT(-1, lit);
    return (uint16_t)lit;
}

static void test_panel_led_covers_its_panel(void)
{
    for (int p = 0; p < PANEL_COUNT; p++) {
        uint8_t hits[PANEL_LEDS] = {0};
        for (uint8_t row = 0; row < 8; row++) {
            for (uint8_t col = 0; col < 8; col++) {
                const uint16_t led = panel_led(p, col, row);
                TEST_ASSERT_TRUE(led >= p * PANEL_LEDS && led < (p + 1) * PANEL_LEDS);
                hits[led - p * PANEL_LEDS]++;
            }
        }
        for (int i = 0; i < PANEL_LEDS; i++) {
            TEST_ASSERT_EQUAL_UINT8(1, hits[i]);
        }
    }
}

static void test_panel_led_is_seen_from_outside(void)
{
    /* Bottom left of each side is at the bottom of the edge it shares with
     * the previous side, round Left, Back, Right, Front. */
    TEST_ASSERT_TRUE(lights(0, 7, 0, panel_led(0, 0, 0)));
    TEST_ASSERT_TRUE(lights(0, 0, 0, panel_led(1, 0, 0)));
    TEST_ASSERT_TRUE(lights(7, 0, 0, panel_led(2, 0, 0)));
    TEST_ASSERT_TRUE(lights(7, 7, 0, panel_led(3, 0, 0)));
    for (uint8_t i = 0; i < 8; i++) {
        /* Each side's right column meets the next side's left column. */
        TEST_ASSERT_TRUE(lights(0, 0, i, panel_led(0, 7, i)));
        TEST_ASSERT_TRUE(lights(7, 0, i, panel_led(1, 7, i)));
        TEST_ASSERT_TRUE(lights(7, 7, i, panel_led(2, 7, i)));
        TEST_ASSERT_TRUE(lights(0, 7, i, panel_led(3, 7, i)));
        /* Unfolded cross: Front's top row meets Top's bottom row, Front's
         * bottom row meets Bottom's top row, column for column. */
        const uint8_t x = (uint8_t)(7 - i);
        TEST_ASSERT_TRUE(lights(x, 7, 7, panel_led(3, i, 7)));
        TEST_ASSERT_TRUE(lights(x, 7, 7, panel_led(5, i, 0)));
        TEST_ASSERT_TRUE(lights(x, 7, 0, panel_led(3, i, 0)));
        TEST_ASSERT_TRUE(lights(x, 7, 0, panel_led(4, i, 7)));
    }
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_every_led_is_hit_exactly_once);
    RUN_TEST(test_chain_corners_match_wiring);
    RUN_TEST(test_inner_voxel_lights_nothing);
    RUN_TEST(test_frame_set_lights_every_face);
    RUN_TEST(test_panel_led_covers_its_panel);
    RUN_TEST(test_panel_led_is_seen_from_outside);
    RUN_TEST(test_decode_rest_and_spin);
    return UNITY_END();
}
