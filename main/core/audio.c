#include "audio.h"

#include <string.h>

#define N CUBE_SIZE

/* Band levels fall back by this factor per step after a peak. */
#define LEVEL_RELEASE 0.88f

/* Lift in g for a band at full level, on top of gravity. Its square is
 * used, so quiet bands barely stir the surface. */
#define LIFT_GAIN 1.5f

/* A beat throws the fluid off the floor for one step: this many g against
 * gravity, from KICK_MIN with no bass to KICK_MIN + KICK_BASS at full bass. */
#define KICK_MIN  0.6f
#define KICK_BASS 1.4f

/* Steps a beat must be apart: 150 ms at 18 ms a step. */
#define KICK_REFRACTORY 8

/* The beat flash fades by this factor per step. */
#define FLASH_DECAY 0.85f

/* Brightness between beats with music playing. */
#define DIM_LEVEL 0.35f

/* Steps without a packet before the music counts as stopped (~1 s), and
 * how fast the fluid fades between dancing and normal, per step. */
#define QUIET_STEPS 55
#define PRESENCE_RATE 0.04f

/* Band for each cell of the 4x4 grid of 2x2 columns: bass in the middle
 * four, the middle frequencies round the edges, treble in the corners. */
static const uint8_t BAND_AT[4][4] = {
    /* [v][u] */
    {12, 4, 5, 13},
    {11, 0, 1, 6},
    {10, 3, 2, 7},
    {15, 9, 8, 14},
};

static inline float f32_le(const uint8_t *p)
{
    float v;
    const uint32_t u = (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 |
                       (uint32_t)p[3] << 24;
    memcpy(&v, &u, sizeof v);
    return v;
}

bool audio_decode(const uint8_t *buf, size_t len, audio_frame_t *out)
{
    if (len < AUDIO_PACKET_SIZE || memcmp(buf, "00002", 6) != 0) {
        return false;
    }
    out->volume = f32_le(buf + 12);
    out->beat = buf[16] != 0;
    memcpy(out->bands, buf + 18, AUDIO_BANDS);
    out->major_peak = f32_le(buf + 40);
    return true;
}

void audio_react_init(audio_react_t *r)
{
    memset(r, 0, sizeof *r);
    r->quiet_steps = QUIET_STEPS;
    r->since_kick = KICK_REFRACTORY;
}

void audio_react_step(audio_react_t *r, const audio_frame_t *frame)
{
    r->kick = 0.0f;
    r->flash *= FLASH_DECAY;
    r->since_kick++;
    for (int b = 0; b < AUDIO_BANDS; b++) {
        r->level[b] *= LEVEL_RELEASE;
    }

    if (frame) {
        r->quiet_steps = 0;
        for (int b = 0; b < AUDIO_BANDS; b++) {
            const float level = frame->bands[b] / 255.0f;
            if (level > r->level[b]) {
                r->level[b] = level;
            }
        }
        if (frame->beat && !r->beat_held && r->since_kick >= KICK_REFRACTORY) {
            const float bass = (r->level[0] + r->level[1] + r->level[2]) / 3.0f;
            r->kick = r->presence * (KICK_MIN + KICK_BASS * bass);
            r->flash = 1.0f;
            r->since_kick = 0;
        }
        r->beat_held = frame->beat;
    } else if (r->quiet_steps < QUIET_STEPS) {
        r->quiet_steps++;
    }

    const float target = r->quiet_steps < QUIET_STEPS ? 1.0f : 0.0f;
    if (r->presence < target) {
        r->presence = r->presence + PRESENCE_RATE > target ? target : r->presence + PRESENCE_RATE;
    } else if (r->presence > target) {
        r->presence = r->presence - PRESENCE_RATE < target ? target : r->presence - PRESENCE_RATE;
    }
}

void audio_react_lift(const audio_react_t *r, float lift[N * N])
{
    float grid[4][4];
    float mean = 0.0f;
    for (int v = 0; v < 4; v++) {
        for (int u = 0; u < 4; u++) {
            const float level = r->level[BAND_AT[v][u]];
            grid[v][u] = r->presence * LIFT_GAIN * level * level;
            mean += grid[v][u];
        }
    }
    mean /= 16.0f;
    for (int v = 0; v < N; v++) {
        for (int u = 0; u < N; u++) {
            lift[u + N * v] = grid[v / 2][u / 2] - mean;
        }
    }
}

float audio_react_brightness(const audio_react_t *r)
{
    const float music = DIM_LEVEL + (1.0f - DIM_LEVEL) * r->flash;
    return 1.0f - r->presence * (1.0f - music);
}
