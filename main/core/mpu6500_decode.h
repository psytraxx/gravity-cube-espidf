/* MPU-6500 sample decoding: raw FIFO bytes to acceleration and rotation
 * rate in cube axes. */
#pragma once

#include <stdint.h>

#include "vec3.h"

/* One FIFO record: accel X, Y, Z then gyro X, Y, Z, each a big-endian
 * int16 (the FIFO keeps register order, and temperature is not stored). */
#define MPU6500_SAMPLE_BYTES 12

/* ±4 g leaves headroom so shaking the cube does not clip:
 * 32768 / 4 = 8192 LSB per g. */
#define MPU6500_ACCEL_SCALE (1.0f / 8192.0f)

/* ±1000 °/s covers spinning the cube by hand: 32.8 LSB per °/s, as rad/s. */
#define MPU6500_GYRO_SCALE (3.14159265f / 180.0f / 32.8f)

typedef struct {
    vec3_t accel; /* gravity plus motion in g, pointing the way the fluid falls */
    vec3_t gyro;  /* rotation rate of the cube in rad/s */
} imu_sample_t;

/* Sensor-to-cube axis mapping: cube axis i = sign * sensor axis `axis`.
 *
 * The board is mounted flat on the top face with its Z axis pointing up,
 * so at rest it reads +1 g on Z, which has to pull towards cube -Z
 * (z == 0). With the Left panel (x == 0) on the floor it reads -1 g on X,
 * so X maps straight through. If tilting moves the fluid the wrong way on
 * an axis, flip that axis' sign here. */
typedef struct {
    uint8_t axis;
    int8_t sign;
} axis_map_t;

static const axis_map_t MPU6500_AXIS_MAP[3] = {{0, 1}, {1, 1}, {2, -1}};

/* The accelerometer measures the push opposite to gravity, which the axis
 * map turns around; the gyro measures the real rotation and needs the
 * opposite signs. */
#define MPU6500_GYRO_SIGN (-1.0f)

/* Decode one FIFO record (or the same 12 bytes read from ACCEL_XOUT_H with
 * the 2 temperature bytes removed). */
imu_sample_t mpu6500_decode(const uint8_t raw[MPU6500_SAMPLE_BYTES]);
