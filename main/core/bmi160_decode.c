#include "bmi160_decode.h"

static inline float le16(const uint8_t *p)
{
    return (float)(int16_t)((uint16_t)p[1] << 8 | p[0]);
}

static vec3_t to_cube(const float s[3])
{
    return v3(BMI160_AXIS_MAP[0].sign * s[BMI160_AXIS_MAP[0].axis],
              BMI160_AXIS_MAP[1].sign * s[BMI160_AXIS_MAP[1].axis],
              BMI160_AXIS_MAP[2].sign * s[BMI160_AXIS_MAP[2].axis]);
}

/* The accelerometer measures the push opposite to gravity, which the axis
 * map turns around, so the map is minus the sensor-to-cube transform T.
 * Rotation rate is an axial vector and maps as det(T) * T: with det(-T) =
 * -det(T) for 3x3, that is det(map) * map. */
static float map_det(void)
{
    float det = 1.0f;
    for (int i = 0; i < 3; i++) {
        det *= BMI160_AXIS_MAP[i].sign;
        for (int j = i + 1; j < 3; j++) {
            if (BMI160_AXIS_MAP[i].axis > BMI160_AXIS_MAP[j].axis) {
                det = -det;
            }
        }
    }
    return det;
}

imu_sample_t bmi160_decode(const uint8_t raw[BMI160_SAMPLE_BYTES])
{
    const float gyro[3] = {le16(raw), le16(raw + 2), le16(raw + 4)};
    const float accel[3] = {le16(raw + 6), le16(raw + 8), le16(raw + 10)};
    return (imu_sample_t){
        .accel = v3_scale(to_cube(accel), BMI160_ACCEL_SCALE),
        .gyro = v3_scale(to_cube(gyro), map_det() * BMI160_GYRO_SCALE),
    };
}
