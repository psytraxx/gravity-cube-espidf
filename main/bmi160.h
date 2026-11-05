/* BMI160 accelerometer and gyroscope on SPI.
 *
 * Samples at 200 Hz into the chip's FIFO (accel ±4 g, gyro ±1000 °/s,
 * normal filter mode), so the main loop collects every sample since the
 * last frame and integrates the gyro over them. The chip's own engine
 * detects double taps. */
#pragma once

#include <stdbool.h>

#include "esp_err.h"
#include "bmi160_decode.h"

#define BMI160_SAMPLE_RATE_HZ 200
#define BMI160_SAMPLE_MS      (1000 / BMI160_SAMPLE_RATE_HZ)

/* Set up the bus, configure the chip and calibrate the gyro. The cube
 * should be still while this runs (about half a second). */
esp_err_t bmi160_init(void);

/* True once for each double tap since the last call. */
bool bmi160_double_tap(void);

/* Read up to `max` samples from the FIFO, oldest first, gyro bias removed.
 * Returns the number read, 0 if none are waiting or the FIFO was full (it
 * is then flushed), or -1 on a bus error. */
int bmi160_read(imu_sample_t *out, int max);
