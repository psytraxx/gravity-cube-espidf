/* Gravity cube: particles on an 8x8x8 LED cube that fall the way the cube
 * is tilted.
 *
 * Core 0 (this task) reads the IMU, runs the physics and builds frames;
 * core 1 sends them to the LEDs. The RMT send takes ~11.5 ms per changed
 * frame, so the two overlap: core 0 computes the next frame while core 1
 * sends the current one, and only waits if core 1 is still busy. Wi-Fi
 * and firmware updates run on core 1 too (ota.c). */
#include <math.h>

#include "color.h"
#include "esp_log.h"
#include "esp_random.h"
#include "esp_timer.h"
#include "fluid.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "motion.h"
#include "bmi160.h"
#include "ota.h"
#include "panel.h"
#include "sdkconfig.h"
#include "ws2812.h"

static const char *TAG = "cube";

/* Track gravity with the gyro and triple anything else, so shaking the
 * cube sloshes the fluid. */
#define MOTION_SMOOTHING 0.05f
#define MOTION_GAIN      3.0f

/* FIFO samples read per frame at most; the FIFO holds 42. */
#define MAX_SAMPLES 48

#define LOG_EVERY 100

/* One LED per step; a frame takes ~11.5 ms to send. */
#define PANEL_TEST_STEP_MS 12
#define PANEL_TEST_COLOR   ((color_t){0, 60, 0})

/* Frames from core 0 to core 1. One slot. */
static QueueHandle_t s_frames;
/* Duration of the last LED send, for the status log. */
static volatile int64_t s_led_send_us;

static void led_task(void *arg)
{
    /* The RMT interrupt runs on the core that sets the channel up. */
    ESP_ERROR_CHECK(ws2812_init(CONFIG_CUBE_LED_GPIO));
    ESP_LOGI(TAG, "%d LEDs on GPIO %d, sent from core %d", LED_COUNT, CONFIG_CUBE_LED_GPIO,
             xPortGetCoreID());
    static frame_t frame;
    for (;;) {
        xQueueReceive(s_frames, &frame, portMAX_DELAY);
        const int64_t start = esp_timer_get_time();
        const esp_err_t err = ws2812_show(&frame);
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "LED refresh failed: %s", esp_err_to_name(err));
        }
        s_led_send_us = esp_timer_get_time() - start;
    }
}

#if CONFIG_CUBE_PANEL_TEST
/* Each panel in turn, chain order, fills green LED by LED: from the
 * bottom left, row by row, to the top right. Then all off. */
static void panel_test(void)
{
    static frame_t frame;
    for (int p = 0; p < PANEL_COUNT; p++) {
        ESP_LOGI(TAG, "Panel test: %d %s", p, panel_name(p));
        frame_clear(&frame);
        for (uint8_t row = 0; row < CUBE_SIZE; row++) {
            for (uint8_t col = 0; col < CUBE_SIZE; col++) {
                frame_set_panel_led(&frame, p, col, row, PANEL_TEST_COLOR);
                xQueueSend(s_frames, &frame, portMAX_DELAY);
                vTaskDelay(pdMS_TO_TICKS(PANEL_TEST_STEP_MS));
            }
        }
    }
    frame_clear(&frame);
    xQueueSend(s_frames, &frame, portMAX_DELAY);
}
#endif

void app_main(void)
{
    ESP_LOGI(TAG, "Gravity cube, %dx%dx%d", CUBE_SIZE, CUBE_SIZE, CUBE_SIZE);

    /* Joins Wi-Fi during the panel test. */
    ota_start();

    s_frames = xQueueCreate(1, sizeof(frame_t));
    xTaskCreatePinnedToCore(led_task, "leds", 4096, NULL, 5, NULL, 1);

#if CONFIG_CUBE_PANEL_TEST
    panel_test();
#endif

    /* Calibrates the gyro, so the cube should be still. */
    const bool have_imu = bmi160_init() == ESP_OK;
    if (!have_imu) {
        ESP_LOGW(TAG, "No BMI160: check the wiring (README.md#wiring). "
                      "Running with gravity towards z == 0, no spin or taps.");
    }

    static fluid_t fluid;
    fluid_init(&fluid);
    fluid_populate(&fluid, FLUID_PARTICLES);
    ESP_LOGI(TAG, "%d fluid particles; double-tap the cube to change colour", FLUID_PARTICLES);
    uint32_t hue = FLUID_HUE;

    motion_filter_t motion;
    motion_filter_init(&motion, MOTION_SMOOTHING, MOTION_GAIN);

    static imu_sample_t samples[MAX_SAMPLES];
    static frame_t frame;
    /* No IMU (or no new sample): gravity towards z == 0. */
    vec3_t accel = v3(0.0f, 0.0f, -1.0f);
    vec3_t gyro_rate = v3(0.0f, 0.0f, 0.0f);
    uint32_t loops = 0;
    TickType_t wake = xTaskGetTickCount();

    for (;;) {
        if (have_imu && bmi160_double_tap()) {
            /* At least 60 degrees from the last hue, so the change shows. */
            hue = (hue + 60 + esp_random() % 241) % 360;
            ESP_LOGI(TAG, "Double tap: hue %lu", (unsigned long)hue);
        }

        /* Every sample since the last frame: the gyro is integrated over
         * all of them, and their mean is this frame's acceleration. */
        vec3_t rotation = v3(0.0f, 0.0f, 0.0f);
        const int n = have_imu ? bmi160_read(samples, MAX_SAMPLES) : 0;
        if (n < 0) {
            ESP_LOGW(TAG, "IMU read failed");
        }
        if (n > 0) {
            vec3_t sum = v3(0.0f, 0.0f, 0.0f);
            for (int i = 0; i < n; i++) {
                sum = v3_add(sum, samples[i].accel);
                rotation = v3_add(rotation, v3_scale(samples[i].gyro, BMI160_SAMPLE_MS / 1000.0f));
            }
            accel = v3_scale(sum, 1.0f / (float)n);
            gyro_rate = samples[n - 1].gyro;
        }

        const int64_t start = esp_timer_get_time();
        fluid_set_rotation(&fluid, rotation);
        fluid_step(&fluid, motion_filter_apply(&motion, accel, rotation));
        const int64_t physics_us = esp_timer_get_time() - start;

        frame_clear(&frame);
        for (uint8_t z = 0; z < CUBE_SIZE; z++) {
            for (uint8_t y = 0; y < CUBE_SIZE; y++) {
                for (uint8_t x = 0; x < CUBE_SIZE; x++) {
                    const cell_t c = {x, y, z};
                    if (fluid_lit(&fluid, c)) {
                        const int tint = (int)lrintf(fluid_tint(&fluid, c));
                        frame_set(&frame, c, color_from_hue((uint32_t)((int)hue + 360 + tint)));
                    }
                }
            }
        }
        xQueueSend(s_frames, &frame, portMAX_DELAY);
        const int64_t frame_us = esp_timer_get_time() - start;

        if (++loops % LOG_EVERY == 0) {
            ESP_LOGI(TAG,
                     "Loop %lu: accel (%.2f, %.2f, %.2f) |%.2f| g, gyro (%.2f, %.2f, %.2f) rad/s, "
                     "physics %lld us, frame %lld us, LED send %lld us",
                     (unsigned long)loops, accel.x, accel.y, accel.z,
                     v3_len(accel), gyro_rate.x, gyro_rate.y, gyro_rate.z, physics_us, frame_us,
                     s_led_send_us);
        }
        if (loops == LOG_EVERY) {
            ota_app_ok();
        }

        /* Fixed step rate: the physics constants are per step. If a frame
         * ran over, still give the idle task a tick. */
        if (xTaskDelayUntil(&wake, pdMS_TO_TICKS(CONFIG_CUBE_FRAME_MS)) == pdFALSE) {
            vTaskDelay(1);
            wake = xTaskGetTickCount();
        }
    }
}
