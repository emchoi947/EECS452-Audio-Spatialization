//lc3_encoder.h
#ifndef LC3_ENCODER_H
#define LC3_ENCODER_H

#include "common.h"

// Configuration for 16kHz, 10ms frame
#define LC3_SAMPLE_RATE 16000
#define LC3_FRAME_MS    10
#define LC3_SAMPLES_PER_FRAME 160
#define LC3_TARGET_BITRATE 32000 // 32kbps is plenty for 16kHz mono

// Initializes the encoder hardware/software state
void lc3_init(void);

// Takes 160 samples (int16_t) and outputs compressed bytes
// Returns the number of bytes written to 'out_bytes'
int lc3_encode_frame(int16_t *pcm_in, uint8_t *out_bytes);
int32_t upsample_32(uint8_t &temp);
int16_t downsample_16(int32_t &data);
void lc3_encode(int16_t &processing_buf);

#endif