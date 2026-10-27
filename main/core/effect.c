#include "effect.h"

static void start(effect_t *e, effect_kind_t kind)
{
    e->kind = kind;
    switch (kind) {
    case EFFECT_FLUID:
        fluid_init(&e->fluid);
        fluid_populate(&e->fluid, FLUID_PARTICLES);
        break;
    case EFFECT_BOIDS:
        boids_init(&e->boids);
        boids_populate(&e->boids, BOIDS_PARTICLES);
        break;
    }
}

void effect_init(effect_t *e)
{
    start(e, EFFECT_FLUID);
}

void effect_next(effect_t *e)
{
    start(e, e->kind == EFFECT_FLUID ? EFFECT_BOIDS : EFFECT_FLUID);
}

void effect_step(effect_t *e, vec3_t accel)
{
    switch (e->kind) {
    case EFFECT_FLUID:
        fluid_step(&e->fluid, accel);
        break;
    case EFFECT_BOIDS:
        boids_step(&e->boids, accel);
        break;
    }
}

void effect_set_rotation(effect_t *e, vec3_t rotation)
{
    switch (e->kind) {
    case EFFECT_FLUID:
        fluid_set_rotation(&e->fluid, rotation);
        break;
    case EFFECT_BOIDS:
        boids_set_rotation(&e->boids, rotation);
        break;
    }
}

const char *effect_name(const effect_t *e)
{
    return e->kind == EFFECT_FLUID ? "fluid" : "boids";
}

color_t effect_color(const effect_t *e)
{
    return e->kind == EFFECT_FLUID ? FLUID_COLOR : BOIDS_COLOR;
}

int effect_count(const effect_t *e)
{
    return e->kind == EFFECT_FLUID ? e->fluid.n : e->boids.n;
}

cell_t effect_cell(const effect_t *e, int i)
{
    return e->kind == EFFECT_FLUID ? fluid_cell(&e->fluid, i) : boids_cell(&e->boids, i);
}
