#include <stdbool.h>
#include <string.h>

#include "assembly.h"
#include "panel.h"
#include "unity.h"

static frame_t s_frame;

void setUp(void)
{
    panel_set_wiring(PANEL_WIRING_ZIGZAG);
}
void tearDown(void) {}

/* The colour of one LED, found through frame_set_panel_led. */
static color_t shown(int panel, uint8_t col, uint8_t row)
{
    static frame_t probe;
    frame_clear(&probe);
    frame_set_panel_led(&probe, panel, col, row, (color_t){1, 1, 1});
    for (int i = 0; i < LED_COUNT; i++) {
        if (probe.rgb[i * 3]) {
            return (color_t){s_frame.rgb[i * 3], s_frame.rgb[i * 3 + 1], s_frame.rgb[i * 3 + 2]};
        }
    }
    TEST_FAIL_MESSAGE("no LED");
    return (color_t){0, 0, 0};
}

static void assert_color(color_t want, color_t got)
{
    TEST_ASSERT_EQUAL_UINT8(want.r, got.r);
    TEST_ASSERT_EQUAL_UINT8(want.g, got.g);
    TEST_ASSERT_EQUAL_UINT8(want.b, got.b);
}

static void check_layout(void)
{
    assembly_frame(&s_frame);
    const color_t dark = {0, 0, 0};
    for (int p = 0; p < PANEL_COUNT; p++) {
        /* Corners dark, the ring inside the edges dark, the centre in the
         * panel's own colour. */
        assert_color(dark, shown(p, 0, 0));
        assert_color(dark, shown(p, 7, 0));
        assert_color(dark, shown(p, 0, 7));
        assert_color(dark, shown(p, 7, 7));
        for (uint8_t i = 1; i < 7; i++) {
            assert_color(dark, shown(p, i, 1));
            assert_color(dark, shown(p, i, 6));
            assert_color(dark, shown(p, 1, i));
            assert_color(dark, shown(p, 6, i));
        }
        for (uint8_t row = 2; row <= 5; row++) {
            for (uint8_t col = 2; col <= 5; col++) {
                assert_color(assembly_color(p), shown(p, col, row));
            }
        }
    }
}

static void test_layout(void)
{
    check_layout();
    panel_set_wiring(PANEL_WIRING_ROWS);
    check_layout();
}

static void check_edges_show_the_neighbour(void)
{
    assembly_frame(&s_frame);
    for (uint8_t i = 1; i < 7; i++) {
        /* Sides round the cube: Left, Back, Right, Front. Each side's
         * right edge shows the next side, its left edge the previous one,
         * its top Top and its bottom Bottom. */
        for (int p = 0; p < 4; p++) {
            assert_color(assembly_color((p + 1) % 4), shown(p, 7, i));
            assert_color(assembly_color((p + 3) % 4), shown(p, 0, i));
            assert_color(assembly_color(5), shown(p, i, 7));
            assert_color(assembly_color(4), shown(p, i, 0));
        }
        /* Top, Front edge at the bottom: Back at the top, Right on the
         * left, Left on the right (seen from above). */
        assert_color(assembly_color(3), shown(5, i, 0));
        assert_color(assembly_color(1), shown(5, i, 7));
        assert_color(assembly_color(2), shown(5, 0, i));
        assert_color(assembly_color(0), shown(5, 7, i));
        /* Bottom, Front edge at the top: Back at the bottom, Right on the
         * left, Left on the right (seen from below). */
        assert_color(assembly_color(3), shown(4, i, 7));
        assert_color(assembly_color(1), shown(4, i, 0));
        assert_color(assembly_color(2), shown(4, 0, i));
        assert_color(assembly_color(0), shown(4, 7, i));
    }
}

static void test_edges_show_the_neighbour(void)
{
    check_edges_show_the_neighbour();
    panel_set_wiring(PANEL_WIRING_ROWS);
    check_edges_show_the_neighbour();
}

static void test_colors_differ(void)
{
    for (int a = 0; a < PANEL_COUNT; a++) {
        for (int b = a + 1; b < PANEL_COUNT; b++) {
            const color_t ca = assembly_color(a), cb = assembly_color(b);
            TEST_ASSERT_FALSE(ca.r == cb.r && ca.g == cb.g && ca.b == cb.b);
            TEST_ASSERT_NOT_EQUAL(0, strcmp(assembly_color_name(a), assembly_color_name(b)));
        }
    }
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_layout);
    RUN_TEST(test_edges_show_the_neighbour);
    RUN_TEST(test_colors_differ);
    return UNITY_END();
}
