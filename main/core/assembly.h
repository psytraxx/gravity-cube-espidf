/* Assembly pattern: shows whether every panel sits on its face, the right
 * way round.
 *
 * Each panel has a colour of its own, which fills the 4x4 block in its
 * centre. Along each of its four edges it shows the colour of the panel
 * on the other side of that edge; its corners, where three panels meet,
 * stay dark. On a correctly built cube every edge strip matches the centre
 * of the panel next to it, so the two strips along a seam swap colours. A
 * panel in the wrong place shows the wrong centre colour; one turned or
 * flipped shows its edge colours on the wrong sides. */
#pragma once

#include "config.h"
#include "panel.h"

/* A panel's own colour (0-5, chain order): Left red, Back green, Right
 * blue, Front yellow, Bottom magenta, Top white. */
color_t assembly_color(int panel);

/* The colour's name, e.g. "red". */
const char *assembly_color_name(int panel);

/* Clear the frame and draw the pattern on every panel. */
void assembly_frame(frame_t *f);
