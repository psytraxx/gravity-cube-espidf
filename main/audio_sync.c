#include "audio_sync.h"

#include <string.h>

#include "esp_log.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lwip/sockets.h"
#include "sdkconfig.h"

static const char *TAG = "audio";

/* Core 0 is busy with the physics. */
#define AUDIO_CORE 1

static portMUX_TYPE s_lock = portMUX_INITIALIZER_UNLOCKED;
static audio_frame_t s_latest;
static bool s_fresh;

static void audio_task(void *arg)
{
    const int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    const struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_port = htons(CONFIG_CUBE_AUDIO_PORT),
        .sin_addr.s_addr = htonl(INADDR_ANY),
    };
    if (sock < 0 || bind(sock, (const struct sockaddr *)&addr, sizeof addr) < 0) {
        ESP_LOGE(TAG, "Cannot listen on UDP port %d", CONFIG_CUBE_AUDIO_PORT);
        vTaskDelete(NULL);
    }
    ESP_LOGI(TAG, "Listening for WLED audio sync on UDP port %d", CONFIG_CUBE_AUDIO_PORT);

    uint8_t buf[64];
    audio_frame_t frame;
    for (;;) {
        const int len = recv(sock, buf, sizeof buf, 0);
        if (len > 0 && audio_decode(buf, (size_t)len, &frame)) {
            portENTER_CRITICAL(&s_lock);
            s_latest = frame;
            s_fresh = true;
            portEXIT_CRITICAL(&s_lock);
        }
    }
}

void audio_sync_start(void)
{
    esp_wifi_set_ps(WIFI_PS_NONE);
    xTaskCreatePinnedToCore(audio_task, "audio", 3072, NULL, 4, NULL, AUDIO_CORE);
}

bool audio_sync_take(audio_frame_t *frame)
{
    portENTER_CRITICAL(&s_lock);
    const bool fresh = s_fresh;
    if (fresh) {
        *frame = s_latest;
        s_fresh = false;
    }
    portEXIT_CRITICAL(&s_lock);
    return fresh;
}
