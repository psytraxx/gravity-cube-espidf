#include "ws2812.h"

#include <stdbool.h>
#include <string.h>

#include "driver/rmt_tx.h"
#include "esp_check.h"
#include "esp_rom_sys.h"

static const char *TAG = "ws2812";

/* 10 MHz RMT tick: 0.1 us per tick. */
#define RMT_RESOLUTION_HZ 10000000

/* Four of the ESP32's eight 64-symbol RMT blocks: the driver refills one
 * half while the other goes out, so the refill interrupt has ~150 us of
 * slack instead of ~40. */
#define RMT_MEM_SYMBOLS 256

/* Low time that latches a frame. The WS2812B datasheet asks for 280 us;
 * older parts need 50. */
#define RESET_US 300

static rmt_channel_handle_t s_chan;
static rmt_encoder_handle_t s_encoder;
static frame_t s_sent;
static bool s_have_sent;

esp_err_t ws2812_init(int gpio)
{
    const rmt_tx_channel_config_t chan_cfg = {
        .gpio_num = gpio,
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = RMT_RESOLUTION_HZ,
        .mem_block_symbols = RMT_MEM_SYMBOLS,
        .trans_queue_depth = 1,
    };
    ESP_RETURN_ON_ERROR(rmt_new_tx_channel(&chan_cfg, &s_chan), TAG, "channel");

    /* WS2812 bit timing: 0 = 0.3 us high, 0.9 us low; 1 = 0.9 us high,
     * 0.3 us low. Bytes go out MSB first. */
    const rmt_bytes_encoder_config_t enc_cfg = {
        .bit0 = {.level0 = 1, .duration0 = 3, .level1 = 0, .duration1 = 9},
        .bit1 = {.level0 = 1, .duration0 = 9, .level1 = 0, .duration1 = 3},
        .flags.msb_first = 1,
    };
    ESP_RETURN_ON_ERROR(rmt_new_bytes_encoder(&enc_cfg, &s_encoder), TAG, "encoder");
    return rmt_enable(s_chan);
}

esp_err_t ws2812_show(const frame_t *frame)
{
    if (s_have_sent && memcmp(frame->grb, s_sent.grb, sizeof s_sent.grb) == 0) {
        return ESP_OK;
    }
    const rmt_transmit_config_t tx_cfg = {.loop_count = 0};
    ESP_RETURN_ON_ERROR(rmt_transmit(s_chan, s_encoder, frame->grb, sizeof frame->grb, &tx_cfg),
                        TAG, "transmit");
    ESP_RETURN_ON_ERROR(rmt_tx_wait_all_done(s_chan, 100), TAG, "wait");
    esp_rom_delay_us(RESET_US);
    s_sent = *frame;
    s_have_sent = true;
    return ESP_OK;
}
