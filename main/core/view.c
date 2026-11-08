#include "view.h"

#include <math.h>

#include "color.h"
#include "panel.h"

void view_frame(const fluid_t *f, uint32_t hue, frame_t *frame)
{
    frame_clear(frame);
    for (uint8_t z = 0; z < CUBE_SIZE; z++) {
        for (uint8_t y = 0; y < CUBE_SIZE; y++) {
            for (uint8_t x = 0; x < CUBE_SIZE; x++) {
                const cell_t c = {x, y, z};
                if (fluid_lit(f, c)) {
                    const int tint = (int)lrintf(fluid_tint(f, c));
                    frame_set(frame, c, color_from_hue((uint32_t)((int)hue + 360 + tint)));
                }
            }
        }
    }
}
