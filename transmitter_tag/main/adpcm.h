#pragma once
#include <stdint.h>

typedef struct {
    int32_t predicted;
    int     step_index;
} adpcm_state_t;

static const int adpcm_step_table[89] = {
    7, 8, 9, 10, 11, 12, 13, 14, 16, 17,
    19, 21, 23, 25, 28, 31, 34, 37, 41, 45,
    50, 55, 60, 66, 73, 80, 88, 97, 107, 118,
    130, 143, 157, 173, 190, 209, 230, 253, 279, 307,
    337, 371, 408, 449, 494, 544, 598, 658, 724, 796,
    876, 963, 1060, 1166, 1282, 1411, 1552, 1707, 1878, 2066,
    2272, 2499, 2749, 3024, 3327, 3660, 4026, 4428, 4871, 5358,
    5894, 6484, 7132, 7845, 8630, 9493, 10442, 11487, 12635, 13899,
    15289, 16818, 18500, 20350, 22385, 24623, 27086, 29794, 32767
};

static const int adpcm_index_table[16] = {
    -1, -1, -1, -1, 2, 4, 6, 8,
    -1, -1, -1, -1, 2, 4, 6, 8
};

/* Encodes a single int16_t sample to a 4-bit ADPCM nibble */
static inline uint8_t adpcm_encode_sample(int16_t sample, adpcm_state_t *state)
{
    int step = adpcm_step_table[state->step_index];
    int diff = sample - state->predicted;
    uint8_t nibble = 0;

    if (diff < 0) {
        nibble = 8;
        diff = -diff;
    }

    if (diff >= step)        { nibble |= 4; diff -= step; }
    if (diff >= step >> 1)   { nibble |= 2; diff -= step >> 1; }
    if (diff >= step >> 2)   { nibble |= 1; }

    /* Update predictor */
    int delta = step >> 3;
    if (nibble & 4) delta += step;
    if (nibble & 2) delta += step >> 1;
    if (nibble & 1) delta += step >> 2;
    if (nibble & 8) delta = -delta;

    state->predicted += delta;
    if (state->predicted > 32767)  state->predicted = 32767;
    if (state->predicted < -32768) state->predicted = -32768;

    /* Update step index */
    state->step_index += adpcm_index_table[nibble & 0x0F];
    if (state->step_index < 0)  state->step_index = 0;
    if (state->step_index > 88) state->step_index = 88;

    return nibble & 0x0F;
}

/* Encodes 160 int16_t samples into 80 bytes (two nibbles packed per byte) */
static inline void adpcm_encode_frame(const int16_t *samples, uint8_t *out, adpcm_state_t *state)
{
    for (int i = 0; i < 160; i += 2) {
        uint8_t lo = adpcm_encode_sample(samples[i],     state);
        uint8_t hi = adpcm_encode_sample(samples[i + 1], state);
        out[i / 2] = (hi << 4) | lo;
    }
}

static inline int32_t *upsample_32(const uint8_t *buf)
{
    return (int32_t *)buf;
}