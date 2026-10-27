/* 3D float vector: positions, velocities, accelerations and rotation
 * vectors (axis times radians). */
#pragma once

#include <math.h>

typedef struct {
    float x, y, z;
} vec3_t;

static inline vec3_t v3(float x, float y, float z)
{
    return (vec3_t){x, y, z};
}

static inline vec3_t v3_add(vec3_t a, vec3_t b)
{
    return v3(a.x + b.x, a.y + b.y, a.z + b.z);
}

static inline vec3_t v3_sub(vec3_t a, vec3_t b)
{
    return v3(a.x - b.x, a.y - b.y, a.z - b.z);
}

static inline vec3_t v3_scale(vec3_t a, float s)
{
    return v3(a.x * s, a.y * s, a.z * s);
}

static inline float v3_dot(vec3_t a, vec3_t b)
{
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

static inline vec3_t v3_cross(vec3_t a, vec3_t b)
{
    return v3(a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x);
}

static inline float v3_len(vec3_t a)
{
    return sqrtf(v3_dot(a, a));
}

static inline float clampf(float v, float lo, float hi)
{
    return v < lo ? lo : (v > hi ? hi : v);
}

/* Rotate v by a rotation vector (axis times angle in radians), using
 * Rodrigues' formula. */
static inline vec3_t v3_rotate(vec3_t v, vec3_t rotation)
{
    const float angle = v3_len(rotation);
    if (angle < 1e-6f) {
        return v;
    }
    const vec3_t axis = v3_scale(rotation, 1.0f / angle);
    const float s = sinf(angle), c = cosf(angle);
    return v3_add(v3_add(v3_scale(v, c), v3_scale(v3_cross(axis, v), s)),
                  v3_scale(axis, v3_dot(axis, v) * (1.0f - c)));
}
