#include "color.h"
#include "unity.h"

void setUp(void) {}
void tearDown(void) {}

static void test_primary_hues(void)
{
    const color_t red = color_from_hue(0), green = color_from_hue(120), blue = color_from_hue(240);
    TEST_ASSERT_EQUAL_UINT8(FLUID_LEVEL, red.r);
    TEST_ASSERT_EQUAL_UINT8(0, red.g + red.b);
    TEST_ASSERT_EQUAL_UINT8(FLUID_LEVEL, green.g);
    TEST_ASSERT_EQUAL_UINT8(0, green.r + green.b);
    TEST_ASSERT_EQUAL_UINT8(FLUID_LEVEL, blue.b);
    TEST_ASSERT_EQUAL_UINT8(0, blue.r + blue.g);
}

static void test_every_hue_draws_the_same(void)
{
    for (uint32_t hue = 0; hue < 720; hue++) {
        const color_t c = color_from_hue(hue);
        TEST_ASSERT_EQUAL_INT(FLUID_LEVEL, c.r + c.g + c.b);
    }
}

static void test_dim_scales_and_rounds(void)
{
    const color_t c = {60, 30, 1};
    const color_t half = color_dim(c, 0.5f);
    TEST_ASSERT_EQUAL_UINT8(30, half.r);
    TEST_ASSERT_EQUAL_UINT8(15, half.g);
    TEST_ASSERT_EQUAL_UINT8(1, half.b);
    TEST_ASSERT_EQUAL_UINT8(60, color_dim(c, 1.0f).r);
    TEST_ASSERT_EQUAL_UINT8(0, color_dim(c, 0.0f).r);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_primary_hues);
    RUN_TEST(test_every_hue_draws_the_same);
    RUN_TEST(test_dim_scales_and_rounds);
    return UNITY_END();
}
