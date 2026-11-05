/* BMI160 sample decoding: raw FIFO bytes to acceleration and rotation rate
 * in cube axes. */
#pragma once

#include <stdint.h>

#include "vec3.h"

/* One headerless FIFO frame with gyro and accel enabled: gyro X, Y, Z then
 * accel X, Y, Z, each a little-endian int16. The data registers from
 * GYR_X (0x0C) to ACC_Z (0x17) hold the same 12 bytes in the same order. */
#define BMI160_SAMPLE_BYTES 12

/* ±4 g leaves headroom so shaking the cube does not clip:
 * 32768 / 4 = 8192 LSB per g. */
#define BMI160_ACCEL_SCALE (1.0f / 8192.0f)

/* ±1000 °/s covers spinning the cube by hand: 32.8 LSB per °/s, as rad/s. */
#define BMI160_GYRO_SCALE (3.14159265f / 180.0f / 32.8f)

typedef struct {
    vec3_t accel; /* gravity plus motion in g, pointing the way the fluid falls */
    vec3_t gyro;  /* rotation rate of the cube in rad/s */
} imu_sample_t;

/* Sensor-to-cube axis mapping: cube axis i = sign * sensor axis `axis`.
 *
 * The board is mounted flat on the top face with its Z axis pointing up,
 * so at rest it reads +1 g on Z, which has to pull towards cube -Z
 * (z == 0). With the Left panel (x == 0) on the floor it reads +1 g on X,
 * which has to pull towards cube -X, so X is turned round too. If tilting
 * moves the fluid the wrong way on an axis, flip that axis' sign here;
 * the gyro follows. */
typedef struct {
    uint8_t axis;
    int8_t sign;
} axis_map_t;

static const axis_map_t BMI160_AXIS_MAP[3] = {{0, -1}, {1, 1}, {2, -1}};

/* Decode one FIFO frame (or the 12 bytes read from GYR_X). */
imu_sample_t bmi160_decode(const uint8_t raw[BMI160_SAMPLE_BYTES]);
