/* Music from a WLED AudioReactive sender (V2 AudioSync packets over UDP),
 * and how the fluid dances to it.
 *
 * The 16 frequency bands drive fountains: the floor of the cube (whichever
 * face gravity points at) is split into a 4x4 grid of 2x2 columns, bass in
 * the middle and treble round the edges, and each band lifts the fluid in
 * its column. Every beat throws the fluid off the floor, harder the more
 * bass there is, and flashes it to full brightness; between beats it
 * glows dimmer. When the packets stop the fluid fades back to normal. */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "config.h"

#define AUDIO_BANDS 16

/* Size of a V2 AudioSync packet. */
#define AUDIO_PACKET_SIZE 44

typedef struct {
    float volume;               /* smoothed level, 0..255 */
    bool beat;                  /* the sender heard a beat */
    uint8_t bands[AUDIO_BANDS]; /* 16 log-spaced bands, low to high, 0..255 */
    float major_peak;           /* loudest frequency, Hz */
} audio_frame_t;

/* Decode a V2 AudioSync packet ("00002" header, little-endian). False for
 * anything else. */
bool audio_decode(const uint8_t *buf, size_t len, audio_frame_t *out);

typedef struct {
    float level[AUDIO_BANDS]; /* band levels, 0..1: jump up, fall back slowly */
    float flash;              /* beat flash, 1 on a beat, fading */
    float presence;           /* 0 without music, 1 while packets arrive */
    float kick;               /* this step's throw off the floor, in g */
    int quiet_steps;          /* steps since the last packet */
    int since_kick;           /* steps since the last throw */
    bool beat_held;           /* the last packet's beat flag */
} audio_react_t;

void audio_react_init(audio_react_t *r);

/* Advance one physics step, with the packet that arrived since the last
 * step, or NULL if none did. */
void audio_react_step(audio_react_t *r, const audio_frame_t *frame);

/* Lift per column of the floor, in g against gravity, for fluid_set_lift:
 * column (u, v) at lift[u + CUBE_SIZE * v]. Averages zero, so the fountains
 * move fluid about rather than lifting it all. */
void audio_react_lift(const audio_react_t *r, float lift[CUBE_SIZE * CUBE_SIZE]);

/* Brightness for this step, 0..1: 1 without music; with it, dim between
 * beats and full on a beat. */
float audio_react_brightness(const audio_react_t *r);
