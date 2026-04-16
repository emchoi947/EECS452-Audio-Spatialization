#pragma once
#include <stdint.h>

typedef struct {
    int16_t predicted;
    int8_t     step_index;
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

/* Decodes a 4-bit ADPCM nibble to an int16_t sample */
static inline int16_t adpcm_decode_sample(uint8_t nibble, adpcm_state_t *state)
{
    int step = adpcm_step_table[state->step_index];

    /* Reconstruct delta the same way the encoder built it */
    int delta = step >> 3;
    if (nibble & 4) delta += step;
    if (nibble & 2) delta += step >> 1;
    if (nibble & 1) delta += step >> 2;
    if (nibble & 8) delta = -delta;

    /* Update predictor */
    state->predicted += delta;
    if (state->predicted >  32767) state->predicted =  32767;
    if (state->predicted < -32768) state->predicted = -32768;

    /* Update step index */
    state->step_index += adpcm_index_table[nibble & 0x0F];
    if (state->step_index < 0)  state->step_index = 0;
    if (state->step_index > 88) state->step_index = 88;

    return (int16_t)state->predicted;
}

/* Decodes 80 bytes back into 160 int16_t samples */
static inline void adpcm_decode_frame(const uint8_t *in, int16_t *samples, adpcm_state_t *state)
{
    for (int i = 4; i < 84; i++) {
        uint8_t lo = in[i] & 0x0F;
        uint8_t hi = (in[i] >> 4) & 0x0F;
        samples[(i-4) * 2]     = adpcm_decode_sample(lo, state);
        samples[(i-4) * 2 + 1] = adpcm_decode_sample(hi, state);
    }
}

static inline adpcm_state_t adpcm_update_state(const uint8_t *frame) {
    adpcm_state_t state;
    state.predicted = (int16_t)((frame[1] << 8) | frame[0]);
    state.step_index = frame[2];
    if (state.step_index < 0)  state.step_index = 0;
    if (state.step_index > 88) state.step_index = 88;
    return state;
}