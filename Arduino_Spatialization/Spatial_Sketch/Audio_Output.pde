// #include <Audio.h>
// #include <Wire.h>
// #include <SPI.h>
// #include <SD.h>
// #include <SerialFlash.h>


// // Sine wave for testing
// float phase = 0.0f;
// const float FREQ   = 440.0f;
// const float RATE   = 44100.0f;


// void buff_loop() {
//   // AudioPlayQueue works in 128-sample blocks at 44100 Hz
//   int16_t *buffer = queue1.getBuffer();  // request a block — returns NULL if none available

//   if (buffer != NULL) {
//     for (int i = 0; i < AUDIO_BLOCK_SAMPLES; i++) {  // AUDIO_BLOCK_SAMPLES = 128
//       buffer[i] = (int16_t)(sinf(phase) * 28000.0f); // 28000 = ~85% of int16 max
//       phase += TWO_PI * FREQ / RATE;
//       if (phase > TWO_PI) phase -= TWO_PI;
//     }
//     queue1.playBuffer();  // hand the block back to the audio library
//   }
// }