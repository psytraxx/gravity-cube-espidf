#pragma once

/* Music over Wi-Fi: WLED AudioReactive sync packets (V2), as sent by a WLED
 * audio sync server or another WLED, received on UDP port
 * CONFIG_CUBE_AUDIO_PORT on core 1. */

#include <stdbool.h>

#include "audio.h"

/* Starts listening. Call once Wi-Fi is started. Turns Wi-Fi power saving
 * off, which would otherwise hold broadcast packets back for up to a
 * beacon interval (~100 ms) and smear the beats. */
void audio_sync_start(void);

/* The newest packet since the last call, if one came. */
bool audio_sync_take(audio_frame_t *frame);
