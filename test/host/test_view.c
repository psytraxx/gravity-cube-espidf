#include "panel.h"
#include "unity.h"
#include "view.h"

void setUp(void) {}
void tearDown(void) {}

static bool led_on(const frame_t *f, uint16_t led)
{
    return f->rgb[led * 3] + f->rgb[led * 3 + 1] + f->rgb[led * 3 + 2] > 0;
}

/* Half a cube of fluid lights every LED over the bottom half and none
 * over the top. */
static void test_half_full_cube_lights_bottom_half(void)
{
    static fluid_t fluid;
    fluid_init(&fluid);
    fluid_populate(&fluid, FLUID_PARTICLES);
    static frame_t frame;
    view_frame(&fluid, FLUID_HUE, &frame);
    int lit = 0;
    for (uint8_t z = 0; z < CUBE_SIZE; z++) {
        for (uint8_t y = 0; y < CUBE_SIZE; y++) {
            for (uint8_t x = 0; x < CUBE_SIZE; x++) {
                uint16_t leds[3];
                const int n = panel_leds((cell_t){x, y, z}, leds);
                for (int i = 0; i < n; i++) {
                    TEST_ASSERT_EQUAL(z < CUBE_SIZE / 2, led_on(&frame, leds[i]));
                    lit += led_on(&frame, leds[i]);
                }
            }
        }
    }
    /* The bottom panel and the lower half of the four sides. */
    TEST_ASSERT_EQUAL_INT(PANEL_LEDS + 4 * PANEL_LEDS / 2, lit);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_half_full_cube_lights_bottom_half);
    return UNITY_END();
}
