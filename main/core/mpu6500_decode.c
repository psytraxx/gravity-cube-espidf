#include "mpu6500_decode.h"

static inline float be16(const uint8_t *p)
{
    return (float)(int16_t)((uint16_t)p[0] << 8 | p[1]);
}

static vec3_t to_cube(const float s[3])
{
    return v3(MPU6500_AXIS_MAP[0].sign * s[MPU6500_AXIS_MAP[0].axis],
              MPU6500_AXIS_MAP[1].sign * s[MPU6500_AXIS_MAP[1].axis],
              MPU6500_AXIS_MAP[2].sign * s[MPU6500_AXIS_MAP[2].axis]);
}

imu_sample_t mpu6500_decode(const uint8_t raw[MPU6500_SAMPLE_BYTES])
{
    const float accel[3] = {be16(raw), be16(raw + 2), be16(raw + 4)};
    const float gyro[3] = {be16(raw + 6), be16(raw + 8), be16(raw + 10)};
    return (imu_sample_t){
        .accel = v3_scale(to_cube(accel), MPU6500_ACCEL_SCALE),
        .gyro = v3_scale(to_cube(gyro), MPU6500_GYRO_SIGN * MPU6500_GYRO_SCALE),
    };
}
