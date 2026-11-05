#include <string.h>

#include "audio.h"
#include "unity.h"

#define N CUBE_SIZE

static audio_react_t r;

void setUp(void)
{
    audio_react_init(&r);
}

void tearDown(void) {}

static void put_f32(uint8_t *p, float v)
{
    uint32_t u;
    memcpy(&u, &v, sizeof u);
    for (int i = 0; i < 4; i++) {
        p[i] = (uint8_t)(u >> (8 * i));
    }
}

/* A V2 packet as the WLED audio sync server sends it. */
static void make_packet(uint8_t buf[AUDIO_PACKET_SIZE], bool beat, uint8_t band_level)
{
    memset(buf, 0, AUDIO_PACKET_SIZE);
    memcpy(buf, "00002", 6);
    put_f32(buf + 8, 100.0f);
    put_f32(buf + 12, 80.0f);
    buf[16] = beat;
    buf[17] = 7;
    for (int b = 0; b < AUDIO_BANDS; b++) {
        buf[18 + b] = (uint8_t)(band_level + b);
    }
    put_f32(buf + 36, 1234.0f);
    put_f32(buf + 40, 440.0f);
}

static audio_frame_t frame(bool beat, uint8_t bass, uint8_t rest)
{
    audio_frame_t f = {.volume = 80.0f, .beat = beat};
    for (int b = 0; b < AUDIO_BANDS; b++) {
        f.bands[b] = b < 4 ? bass : rest;
    }
    return f;
}

/* Packets arrive for a while, so the music counts as playing. */
static void play(int steps, audio_frame_t f)
{
    for (int i = 0; i < steps; i++) {
        audio_react_step(&r, &f);
    }
}

static void test_decodes_v2_packet(void)
{
    uint8_t buf[AUDIO_PACKET_SIZE];
    make_packet(buf, true, 10);
    audio_frame_t f;
    TEST_ASSERT_TRUE(audio_decode(buf, sizeof buf, &f));
    TEST_ASSERT_EQUAL_FLOAT(80.0f, f.volume);
    TEST_ASSERT_TRUE(f.beat);
    TEST_ASSERT_EQUAL_UINT8(10, f.bands[0]);
    TEST_ASSERT_EQUAL_UINT8(25, f.bands[15]);
    TEST_ASSERT_EQUAL_FLOAT(440.0f, f.major_peak);
}

static void test_rejects_other_packets(void)
{
    uint8_t buf[AUDIO_PACKET_SIZE];
    audio_frame_t f;
    make_packet(buf, false, 0);
    TEST_ASSERT_FALSE(audio_decode(buf, sizeof buf - 1, &f));
    buf[4] = '1'; /* V1 */
    TEST_ASSERT_FALSE(audio_decode(buf, sizeof buf, &f));
}

/* Without packets nothing moves and the brightness is normal. */
static void test_silent_without_packets(void)
{
    for (int i = 0; i < 100; i++) {
        audio_react_step(&r, NULL);
    }
    float lift[N * N];
    audio_react_lift(&r, lift);
    for (int i = 0; i < N * N; i++) {
        TEST_ASSERT_EQUAL_FLOAT(0.0f, lift[i]);
    }
    TEST_ASSERT_EQUAL_FLOAT(0.0f, r.kick);
    TEST_ASSERT_EQUAL_FLOAT(1.0f, audio_react_brightness(&r));
}

/* A beat throws once, however many packets keep the flag up, harder with
 * more bass, and flashes; the next beat throws again. */
static void test_beat_kicks_once(void)
{
    play(40, frame(false, 0, 0));
    TEST_ASSERT_LESS_THAN_FLOAT(0.5f, audio_react_brightness(&r));

    const audio_frame_t quiet_beat = frame(true, 0, 0);
    audio_react_step(&r, &quiet_beat);
    const float soft = r.kick;
    TEST_ASSERT_GREATER_THAN_FLOAT(0.0f, soft);
    TEST_ASSERT_EQUAL_FLOAT(1.0f, audio_react_brightness(&r));
    play(3, quiet_beat);
    TEST_ASSERT_EQUAL_FLOAT(0.0f, r.kick);

    play(20, frame(false, 0, 0));
    const audio_frame_t loud_beat = frame(true, 255, 0);
    audio_react_step(&r, &loud_beat);
    TEST_ASSERT_GREATER_THAN_FLOAT(soft * 2.0f, r.kick);
}

/* Beats closer than 150 ms count once. */
static void test_beats_too_close_count_once(void)
{
    play(40, frame(false, 0, 0));
    const audio_frame_t beat = frame(true, 100, 0), off = frame(false, 100, 0);
    audio_react_step(&r, &beat);
    TEST_ASSERT_GREATER_THAN_FLOAT(0.0f, r.kick);
    audio_react_step(&r, &off);
    audio_react_step(&r, &beat);
    TEST_ASSERT_EQUAL_FLOAT(0.0f, r.kick);
}

/* Bass lifts the middle columns, the treble corners sink; the lift
 * averages zero. Once the packets stop, it all fades out. */
static void test_bass_lifts_middle(void)
{
    play(40, frame(false, 255, 0));
    float lift[N * N];
    audio_react_lift(&r, lift);
    TEST_ASSERT_GREATER_THAN_FLOAT(0.5f, lift[3 + N * 3]);
    TEST_ASSERT_GREATER_THAN_FLOAT(0.5f, lift[4 + N * 4]);
    TEST_ASSERT_LESS_THAN_FLOAT(0.0f, lift[0]);
    TEST_ASSERT_LESS_THAN_FLOAT(0.0f, lift[7 + N * 7]);
    float sum = 0.0f;
    for (int i = 0; i < N * N; i++) {
        sum += lift[i];
    }
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, 0.0f, sum);

    for (int i = 0; i < 120; i++) {
        audio_react_step(&r, NULL);
    }
    audio_react_lift(&r, lift);
    TEST_ASSERT_EQUAL_FLOAT(0.0f, lift[3 + N * 3]);
    TEST_ASSERT_EQUAL_FLOAT(1.0f, audio_react_brightness(&r));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_decodes_v2_packet);
    RUN_TEST(test_rejects_other_packets);
    RUN_TEST(test_silent_without_packets);
    RUN_TEST(test_beat_kicks_once);
    RUN_TEST(test_beats_too_close_count_once);
    RUN_TEST(test_bass_lifts_middle);
    return UNITY_END();
}
