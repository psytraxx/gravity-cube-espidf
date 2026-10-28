#include "color.h"

color_t color_dim(color_t c, float scale)
{
    return (color_t){
        (uint8_t)(c.r * scale + 0.5f),
        (uint8_t)(c.g * scale + 0.5f),
        (uint8_t)(c.b * scale + 0.5f),
    };
}

color_t color_from_hue(uint32_t hue)
{
    hue %= 360;
    const uint32_t t = hue % 120;
    const uint8_t a = (uint8_t)(FLUID_LEVEL * (120 - t) / 120);
    const uint8_t b = (uint8_t)(FLUID_LEVEL - a);
    switch (hue / 120) {
    case 0:
        return (color_t){a, b, 0};
    case 1:
        return (color_t){0, a, b};
    default:
        return (color_t){b, 0, a};
    }
}
