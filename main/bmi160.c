#include "bmi160.h"

#include <string.h>

#include "esp_check.h"
#include "esp_log.h"
#include "esp_rom_sys.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "driver/spi_master.h"
#include "sdkconfig.h"

static const char *TAG = "bmi160";

/* Registers */
#define REG_CHIP_ID      0x00
#define REG_ERR          0x02
#define REG_PMU_STATUS   0x03
#define REG_GYR_X_L      0x0C
#define REG_INT_STATUS0  0x1C
#define REG_FIFO_LENGTH  0x22
#define REG_FIFO_DATA    0x24
#define REG_ACC_CONF     0x40
#define REG_ACC_RANGE    0x41
#define REG_GYR_CONF     0x42
#define REG_GYR_RANGE    0x43
#define REG_FIFO_CONFIG1 0x47
#define REG_INT_EN0      0x50
#define REG_INT_OUT_CTRL 0x53
#define REG_INT_LATCH    0x54
#define REG_INT_MAP0     0x55
#define REG_INT_TAP0     0x63
#define REG_INT_TAP1     0x64
#define REG_CMD          0x7E
/* Any read switches the chip to SPI; this one has no side effects. */
#define REG_SPI_SELECT 0x7F

#define CHIP_ID_BMI160 0xD1
/* WHO_AM_I of the ST LSM6DS3 family, which comes on look-alike boards. */
#define REG_LSM6_WHO_AM_I 0x0F

#define CMD_SOFT_RESET  0xB6
#define CMD_ACC_NORMAL  0x11
#define CMD_GYR_NORMAL  0x15
#define CMD_FIFO_FLUSH  0xB0
#define CMD_INT_RESET   0xB1 /* clears latched interrupts */
#define PMU_BOTH_NORMAL 0x14 /* acc_pmu_status and gyr_pmu_status both normal */

/* ODR 9: 100 * 2^(9 - 8) = 200 Hz. BWP 2: normal filter mode, so a 3 dB
 * corner of about 80 Hz (accel) and 75 Hz (gyro). */
#define ACC_CONF_200HZ_NORMAL 0x29
#define GYR_CONF_200HZ_NORMAL 0x29
#define ACC_RANGE_4G          0x05
#define GYR_RANGE_1000DPS     0x01
/* Gyro and accel in the FIFO, no headers: fixed 12-byte frames. */
#define FIFO_GYR_ACC 0xC0

/* Double-tap engine. A tap is a jump in acceleration (the slope) above
 * the threshold; the 50 ms after it (shock) belong to the same tap, a tap
 * only counts after 30 ms of quiet, and the second must come within
 * 375 ms: INT_TAP_0 quiet 30 ms, shock 50 ms, duration 375 ms. */
#define TAP_QUIET30_SHOCK50_DUR375 0x05
/* At ±4 g the threshold counts in steps of 125 mg, up to 31 steps. */
#define TAP_TH_STEP_MG 125
#define TAP_TH_MAX     31
#define INT_D_TAP      0x10 /* INT_EN_0, INT_MAP_0 and INT_STATUS_0 */
#define INT_LATCHED    0x0F
/* INT1 output on, push-pull, active high; INT2 off. */
#define INT1_PUSH_PULL_HIGH 0x0A

#define FIFO_SIZE 1024

/* Gyro readings averaged at startup to measure its zero-rate offset. */
#define CALIBRATION_SAMPLES 50

static vec3_t s_gyro_bias;

/* The chip takes up to 10 MHz; 1 MHz is plenty and forgiving of long
 * wires. */
#define SPI_HZ 1000000
/* Writes need 450 us between them while a sensor is suspended, 2 us once
 * both run. */
#define WRITE_GAP_SUSPEND_US 450
#define WRITE_GAP_NORMAL_US  2
static uint32_t s_write_gap_us = WRITE_GAP_SUSPEND_US;

static spi_device_handle_t s_spi;
/* Command byte plus a FIFO's worth of data. */
static uint8_t s_tx[1 + FIFO_SIZE] __attribute__((aligned(4)));
static uint8_t s_rx[1 + FIFO_SIZE] __attribute__((aligned(4)));

static esp_err_t bus_init(void)
{
    const spi_bus_config_t bus = {
        .mosi_io_num = CONFIG_CUBE_IMU_SPI_MOSI,
        .miso_io_num = CONFIG_CUBE_IMU_SPI_MISO,
        .sclk_io_num = CONFIG_CUBE_IMU_SPI_SCLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = sizeof s_tx,
    };
    ESP_RETURN_ON_ERROR(spi_bus_initialize(SPI2_HOST, &bus, SPI_DMA_CH_AUTO), TAG, "spi bus");
    const spi_device_interface_config_t dev = {
        .clock_speed_hz = SPI_HZ,
        .mode = 3,
        .spics_io_num = CONFIG_CUBE_IMU_SPI_CS,
        .queue_size = 1,
    };
    return spi_bus_add_device(SPI2_HOST, &dev, &s_spi);
}

static esp_err_t read_regs(uint8_t reg, uint8_t *buf, size_t len)
{
    memset(s_tx, 0, len + 1);
    s_tx[0] = 0x80 | reg;
    spi_transaction_t t = {.length = (len + 1) * 8, .tx_buffer = s_tx, .rx_buffer = s_rx};
    const esp_err_t err = spi_device_polling_transmit(s_spi, &t);
    if (err == ESP_OK) {
        memcpy(buf, s_rx + 1, len);
    }
    return err;
}

static esp_err_t write_reg(uint8_t reg, uint8_t value)
{
    spi_transaction_t t = {
        .flags = SPI_TRANS_USE_TXDATA,
        .length = 16,
        .tx_data = {reg & 0x7F, value},
    };
    const esp_err_t err = spi_device_polling_transmit(s_spi, &t);
    esp_rom_delay_us(s_write_gap_us);
    return err;
}

/* A rising edge on CSB puts the chip on SPI until it is power cycled; a
 * soft reset puts it back on I2C. */
static esp_err_t select_spi(void)
{
    uint8_t dummy;
    return read_regs(REG_SPI_SELECT, &dummy, 1);
}

/* One gyro and accel reading straight from the data registers, laid out
 * as a FIFO frame. */
static esp_err_t read_direct(imu_sample_t *s)
{
    uint8_t raw[BMI160_SAMPLE_BYTES];
    ESP_RETURN_ON_ERROR(read_regs(REG_GYR_X_L, raw, sizeof raw), TAG, "read");
    *s = bmi160_decode(raw);
    return ESP_OK;
}

static esp_err_t fifo_flush(void)
{
    return write_reg(REG_CMD, CMD_FIFO_FLUSH);
}

#if CONFIG_CUBE_IMU_I2C_SCAN
/* Log every address that acknowledges, on the SPI pins used as I2C. */
static void i2c_scan(void)
{
    /* High before it is driven: a rising edge on CSB would select SPI. */
    gpio_set_level(CONFIG_CUBE_IMU_SPI_CS, 1);
    gpio_set_direction(CONFIG_CUBE_IMU_SPI_CS, GPIO_MODE_OUTPUT);

    const i2c_master_bus_config_t cfg = {
        .i2c_port = -1,
        .sda_io_num = CONFIG_CUBE_IMU_SPI_MOSI,
        .scl_io_num = CONFIG_CUBE_IMU_SPI_SCLK,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    i2c_master_bus_handle_t bus;
    if (i2c_new_master_bus(&cfg, &bus) != ESP_OK) {
        ESP_LOGE(TAG, "I2C scan: bus setup failed");
        return;
    }
    int found = 0;
    for (uint16_t addr = 0x08; addr < 0x78; addr++) {
        if (i2c_master_probe(bus, addr, 20) == ESP_OK) {
            ESP_LOGI(TAG, "I2C scan: device at 0x%02x", addr);
            found++;
        }
    }
    ESP_LOGI(TAG, "I2C scan: %d device(s) on SDA %d, SCL %d", found, CONFIG_CUBE_IMU_SPI_MOSI,
             CONFIG_CUBE_IMU_SPI_SCLK);
    i2c_del_master_bus(bus);
}
#endif

esp_err_t bmi160_init(void)
{
#if CONFIG_CUBE_IMU_I2C_SCAN
    i2c_scan();
#endif
    ESP_RETURN_ON_ERROR(bus_init(), TAG, "bus");
    /* The pull-down holds INT1 low until the chip drives it. */
    const gpio_config_t int1 = {
        .pin_bit_mask = 1ULL << CONFIG_CUBE_IMU_INT1,
        .mode = GPIO_MODE_INPUT,
        .pull_down_en = GPIO_PULLDOWN_ENABLE,
    };
    ESP_RETURN_ON_ERROR(gpio_config(&int1), TAG, "int1 gpio");

    ESP_RETURN_ON_ERROR(select_spi(), TAG, "no answer");
    s_write_gap_us = WRITE_GAP_SUSPEND_US;
    ESP_RETURN_ON_ERROR(write_reg(REG_CMD, CMD_SOFT_RESET), TAG, "reset");
    vTaskDelay(pdMS_TO_TICKS(10));
    ESP_RETURN_ON_ERROR(select_spi(), TAG, "spi");

    uint8_t id = 0;
    ESP_RETURN_ON_ERROR(read_regs(REG_CHIP_ID, &id, 1), TAG, "chip id");
    if (id != CHIP_ID_BMI160) {
        uint8_t who = 0;
        read_regs(REG_LSM6_WHO_AM_I, &who, 1);
        if (who == 0x69 || who == 0x6A || who == 0x6C) {
            ESP_LOGE(TAG, "WHO_AM_I 0x%02X: this is an ST LSM6DS3-family chip, not a BMI160", who);
            return ESP_ERR_NOT_SUPPORTED;
        }
        ESP_LOGE(TAG, "CHIP_ID 0x%02X, expected 0x%02X: %s", id, CHIP_ID_BMI160,
                 id == 0x00 || id == 0xFF ? "nothing on the bus" : "not a BMI160");
        return ESP_ERR_NOT_FOUND;
    }

    ESP_RETURN_ON_ERROR(write_reg(REG_ACC_CONF, ACC_CONF_200HZ_NORMAL), TAG, "acc conf");
    ESP_RETURN_ON_ERROR(write_reg(REG_ACC_RANGE, ACC_RANGE_4G), TAG, "acc range");
    ESP_RETURN_ON_ERROR(write_reg(REG_GYR_CONF, GYR_CONF_200HZ_NORMAL), TAG, "gyr conf");
    ESP_RETURN_ON_ERROR(write_reg(REG_GYR_RANGE, GYR_RANGE_1000DPS), TAG, "gyr range");
    ESP_RETURN_ON_ERROR(write_reg(REG_CMD, CMD_ACC_NORMAL), TAG, "acc on");
    vTaskDelay(pdMS_TO_TICKS(5));
    ESP_RETURN_ON_ERROR(write_reg(REG_CMD, CMD_GYR_NORMAL), TAG, "gyr on");
    /* The gyro takes 80 ms to start up. */
    vTaskDelay(pdMS_TO_TICKS(100));

    uint8_t pmu = 0, err = 0;
    ESP_RETURN_ON_ERROR(read_regs(REG_PMU_STATUS, &pmu, 1), TAG, "pmu");
    ESP_RETURN_ON_ERROR(read_regs(REG_ERR, &err, 1), TAG, "err");
    if ((pmu & 0x3C) != PMU_BOTH_NORMAL || err != 0) {
        ESP_LOGE(TAG, "PMU_STATUS 0x%02X, ERR_REG 0x%02X: sensors did not start", pmu, err);
        return ESP_FAIL;
    }
    s_write_gap_us = WRITE_GAP_NORMAL_US;

    int tap_th = (CONFIG_CUBE_TAP_THRESHOLD_MG + TAP_TH_STEP_MG / 2) / TAP_TH_STEP_MG;
    tap_th = tap_th < 1 ? 1 : tap_th > TAP_TH_MAX ? TAP_TH_MAX : tap_th;
    ESP_RETURN_ON_ERROR(write_reg(REG_INT_TAP0, TAP_QUIET30_SHOCK50_DUR375), TAG, "tap0");
    ESP_RETURN_ON_ERROR(write_reg(REG_INT_TAP1, (uint8_t)tap_th), TAG, "tap1");
    /* Latched, so INT1 stays high after a double tap until INT_RESET. */
    ESP_RETURN_ON_ERROR(write_reg(REG_INT_OUT_CTRL, INT1_PUSH_PULL_HIGH), TAG, "int out");
    ESP_RETURN_ON_ERROR(write_reg(REG_INT_LATCH, INT_LATCHED), TAG, "latch");
    ESP_RETURN_ON_ERROR(write_reg(REG_INT_MAP0, INT_D_TAP), TAG, "int map");
    ESP_RETURN_ON_ERROR(write_reg(REG_INT_EN0, INT_D_TAP), TAG, "int en");
    ESP_RETURN_ON_ERROR(write_reg(REG_CMD, CMD_INT_RESET), TAG, "int reset");

    vec3_t sum = v3(0.0f, 0.0f, 0.0f);
    imu_sample_t s;
    for (int i = 0; i < CALIBRATION_SAMPLES; i++) {
        ESP_RETURN_ON_ERROR(read_direct(&s), TAG, "calibrate");
        sum = v3_add(sum, s.gyro);
        vTaskDelay(pdMS_TO_TICKS(BMI160_SAMPLE_MS));
    }
    s_gyro_bias = v3_scale(sum, 1.0f / CALIBRATION_SAMPLES);
    ESP_LOGI(TAG,
             "CHIP_ID 0x%02X, accel (%.2f, %.2f, %.2f) g, gyro offset (%.3f, %.3f, %.3f) rad/s", id,
             s.accel.x, s.accel.y, s.accel.z, s_gyro_bias.x, s_gyro_bias.y, s_gyro_bias.z);

    ESP_RETURN_ON_ERROR(write_reg(REG_FIFO_CONFIG1, FIFO_GYR_ACC), TAG, "fifo config");
    return fifo_flush();
}

bool bmi160_double_tap(void)
{
    /* INT1 high means a latched interrupt; only then touch the bus. Only
     * a rising edge counts, so a line that stays high (INT_RESET not
     * taking, a short to 3V3) changes the colour once, not every frame. */
    static bool s_was_high;
    if (!gpio_get_level(CONFIG_CUBE_IMU_INT1)) {
        s_was_high = false;
        return false;
    }
    if (s_was_high) {
        return false;
    }
    s_was_high = true;
    uint8_t status = 0;
    const esp_err_t err = read_regs(REG_INT_STATUS0, &status, 1);
    write_reg(REG_CMD, CMD_INT_RESET);
    return err == ESP_OK && (status & INT_D_TAP);
}

int bmi160_read(imu_sample_t *out, int max)
{
    static uint8_t buf[FIFO_SIZE];
    uint8_t len[2];
    if (read_regs(REG_FIFO_LENGTH, len, 2) != ESP_OK) {
        return -1;
    }
    const int count = (len[1] & 0x07) << 8 | len[0];
    /* A full FIFO drops the oldest frames; flush it and start over. */
    if (count > FIFO_SIZE - BMI160_SAMPLE_BYTES) {
        ESP_LOGW(TAG, "FIFO full, flush");
        return fifo_flush() == ESP_OK ? 0 : -1;
    }

    int n = count / BMI160_SAMPLE_BYTES;
    if (n > max) {
        n = max;
    }
    if (n == 0) {
        return 0;
    }
    if (read_regs(REG_FIFO_DATA, buf, (size_t)n * BMI160_SAMPLE_BYTES) != ESP_OK) {
        return -1;
    }
    for (int i = 0; i < n; i++) {
        out[i] = bmi160_decode(&buf[i * BMI160_SAMPLE_BYTES]);
        out[i].gyro = v3_sub(out[i].gyro, s_gyro_bias);
    }
    return n;
}
