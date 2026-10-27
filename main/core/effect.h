/* Runs one effect at a time and switches to the next on a double tap.
 * Each effect has its own particle count and colour (config.h). */
#pragma once

#include "boids.h"
#include "config.h"
#include "fluid.h"

typedef enum {
    EFFECT_FLUID,
    EFFECT_BOIDS,
} effect_kind_t;

typedef struct {
    effect_kind_t kind;
    union {
        fluid_t fluid;
        boids_t boids;
    };
} effect_t;

/* Start with the fluid, populated. */
void effect_init(effect_t *e);

/* Replace the running effect with the next one, populated with its own
 * particle count. */
void effect_next(effect_t *e);

void effect_step(effect_t *e, vec3_t accel);
void effect_set_rotation(effect_t *e, vec3_t rotation);

/* Short name of the running effect, for logs. */
const char *effect_name(const effect_t *e);
color_t effect_color(const effect_t *e);
int effect_count(const effect_t *e);
cell_t effect_cell(const effect_t *e, int i);
