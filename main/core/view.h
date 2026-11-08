/* The fluid on the cube's surface: each voxel the fluid fills lights the
 * LEDs over it. */
#pragma once

#include <stdint.h>

#include "fluid.h"
#include "frame.h"

/* Clear the frame and light every LED over a voxel the fluid fills, in
 * `hue` plus the fluid's tint there. */
void view_frame(const fluid_t *f, uint32_t hue, frame_t *frame);
