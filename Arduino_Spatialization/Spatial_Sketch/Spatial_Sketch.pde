#include <Audio.h>
#include <Wire.h>
#include <SD.h>
#include <SPI.h>
#include <SerialFlash.h>
#include "low_pass.h"
#include "band_pass.h"

#define LED 13

// --- Swap AudioInputI2S for two queues (one per channel) ---
AudioInputI2S         audioInput;       // replaces sine wave
AudioRecordQueue      recordQueue;      // lets us pull samples in loop()

AudioPlayQueue        queueL;
AudioPlayQueue        queueR;
AudioFilterFIR        myFilterL;
AudioFilterFIR        myFilterR;
AudioOutputI2S        audioOutput;
AudioControlSGTL5000  audioShield;

// Feed queues into filters, filters into output
AudioConnection c0(audioInput,  0, recordQueue, 0);


AudioConnection c1(queueL,    0, myFilterL, 0);
AudioConnection c2(queueR,    0, myFilterR, 0);
AudioConnection c3(myFilterL, 0, audioOutput, 0);
AudioConnection c4(myFilterR, 0, audioOutput, 1);

// --- Your spatialization globals ---
// --- Spatialization globals (unchanged) ---
const float head_width     = 0.15;
const float speed_of_sound = 343.0;
const int   sample_rate    = 44100;

float left_ear_x  = -head_width / 2.0;
float right_ear_x =  head_width / 2.0;
float transmit_x  = 0.0;
float transmit_y  = 5.0;

const int MAX_DELAY_SAMPLES = 100;
const int RING_SIZE = AUDIO_BLOCK_SAMPLES + MAX_DELAY_SAMPLES;

int16_t ringL[RING_SIZE];
int16_t ringR[RING_SIZE];

int write_pos      = 0;
int delayL_samples = 0;
int delayR_samples = 0;
float gainL_global = 1.0f;
float gainR_global = 1.0f;
float gainL        = 1.0f;
float gainR        = 1.0f;
float r_angle, l_angle;
float angle;

String serialBuf = "";
bool serialReady = false;

int start_idx = 1;
struct fir_filter {
  short *coeffs;
  short  num_coeffs;
};
struct fir_filter fir_list[] = {
  {LP,   100},
  {BP,   100},
  {NULL,   0}
};

// --- Helpers (unchanged) ---
int clamp16(int v) {
  if (v >  32767) return  32767;
  if (v < -32768) return -32768;
  return v;
}

int ringIndex(int pos) {
  pos = pos % RING_SIZE;
  if (pos < 0) pos += RING_SIZE;
  return pos;
}

void angle_gain(float angle_diff, float &gL, float &gR) {
  float rad_angle = angle_diff * PI / 180.0f;
  float panL = 0.5f * (1.0f + sinf(rad_angle));
  float panR = 0.5f * (1.0f - sinf(rad_angle));
  gL = powf(panL, 0.5f);
  gR = powf(panR, 0.5f);
}

void updateSpatialParams(float angle_diff) {
  float itd_max = (head_width) / speed_of_sound * 20.0f;

  if (angle_diff < -89.5f && angle_diff > -90.5f) angle_diff = -89.5f;
  if (angle_diff >  89.5f && angle_diff < 90.5f) angle_diff =  89.5f;
  
  if (abs(angle_diff) > 90.0f) {
    angle_diff = angle_diff + 90.0f * (angle_diff / abs(angle_diff)); // Wrap to [-90, 90]
  }

  float itd = itd_max * (angle_diff * 10 / 9) / 100;
  if (abs(itd) > itd_max) itd = itd / (itd/itd_max) ; // Clamp ITD to max by scaling it back proportionally
  int itd_samples = (int)roundf(itd * sample_rate);

  if (itd_samples > 0) {
    delayL_samples = itd_samples;
    delayR_samples = 0;
  } else {
    delayL_samples = 0;
    delayR_samples = abs(itd_samples);
  }

  angle_gain(angle_diff, gainL, gainR);

  float ild = 20.0f * log10f((angle_diff+90.0f) / 90.0f + 1e-3f); // Avoid log(0) with small offset
  gainL_global = pow(powf(10.0f,  ild / 20.0f), 0.5);
  gainR_global = pow(powf(10.0f, -ild / 20.0f), 0.5);

  if(gainL_global >2) gainL_global = 2;
  if(gainR_global >2) gainR_global = 2;

  Serial.print("ITD (samples): "); Serial.print(itd_samples);
  Serial.print(", ILD (dB): ");    Serial.println(ild);

  // Printing debug statemenets to serial
  Serial.print("Updated spatial params - ITD (samples): ");
  Serial.print(itd_samples);
  Serial.print(", ILD (dB): ");
  Serial.print(ild);
  Serial.print(", \n GainL: ");
  Serial.print(gainL_global);
  Serial.print(", GainL_Angle: ");
  Serial.print(gainL);
  Serial.print(", GainR: ");
  Serial.print(gainR_global);
  Serial.print(", GainR_Angle: ");
  Serial.println(gainR);


}


/*
void applySpatialisation(float left_dist, float right_dist) {
  float itd = (right_dist - left_dist) / speed_of_sound * 10.0f;
  int   itd_samples = (int)roundf(itd * sample_rate);
  int   delay = abs(itd_samples);

  float ild   = 20.0f * log10f(right_dist / left_dist);
  float gainL = powf(10.0f, -ild / 20.0f);
  float gainR = powf(10.0f,  ild / 20.0f);

  if (itd_samples > 0) {
    for (int i = pending_buf_size - 1; i >= 0; i--) {
      int dst = i + delay;
      writeR[dst] = (dst < pending_buf_size + MAX_DELAY) ? writeR[i] : 0;
    }
    for (int i = 0; i < delay; i++) writeR[i] = 0;
  } else if (itd_samples < 0) {
    for (int i = pending_buf_size - 1; i >= 0; i--) {
      int dst = i + delay;
      writeL[dst] = (dst < pending_buf_size + MAX_DELAY) ? writeL[i] : 0;
    }
    for (int i = 0; i < delay; i++) writeL[i] = 0;
  }

  for (int i = 0; i < pending_buf_size + delay; i++) {
    writeL[i] = clamp16((int)(writeL[i] * gainL));
    writeR[i] = clamp16((int)(writeR[i] * gainR));
  }



  pending_buf_size += delay;

  // Signal the audio feed to swap at the next clean loop boundary
  swap_requested = true;
}
*/


void setup() {
  Serial.begin(9600);
  delay(300);
  pinMode(LED, OUTPUT);

  AudioMemory(32);
  audioShield.enable();
  audioShield.volume(0.55);

  // Enable the line input — use AUDIO_INPUT_LINEIN or MIC as needed
  audioShield.inputSelect(AUDIO_INPUT_LINEIN);
  audioShield.lineInLevel(7);   // 0–15, adjust to taste


  myFilterL.begin(fir_list[start_idx].coeffs, fir_list[start_idx].num_coeffs);
  myFilterR.begin(fir_list[start_idx].coeffs, fir_list[start_idx].num_coeffs);


  // Generate initial tone and apply starting position
  // float ld = sqrtf(powf(transmit_x - left_ear_x,  2) + powf(transmit_y, 2));
  // float rd = sqrtf(powf(transmit_x - right_ear_x, 2) + powf(transmit_y, 2));
  // applySpatialisation(ld, rd);
    recordQueue.begin();  // start capturing input blocks

}


void loop() {
  // Redundant outer getBuffer() calls removed (were causing unused variable warnings)
  
  if (recordQueue.available() >= 1) {
    int16_t *input = recordQueue.readBuffer();

    int16_t *blockL = queueL.getBuffer();
    int16_t *blockR = queueR.getBuffer();

    if (blockL != NULL && blockR != NULL) {
      for (int i = 0; i < AUDIO_BLOCK_SAMPLES; i++) {

        int16_t dry = input[i];  // <-- real audio input, sine wave removed

        ringL[write_pos] = dry;
        ringR[write_pos] = dry;

        int readL = ringIndex(write_pos - delayL_samples);
        int readR = ringIndex(write_pos - delayR_samples);

        blockL[i] = clamp16((int)(ringL[readL] * gainL_global * gainL));
        blockR[i] = clamp16((int)(ringR[readR] * gainR_global * gainR));

        write_pos = ringIndex(write_pos + 1);
      }
      queueL.playBuffer();
      queueR.playBuffer();
    }

    recordQueue.freeBuffer();
  }

  // Non-blocking serial read
  while (Serial.available() > 0) {
    char c = Serial.read();
    if (c == '\n') {
      serialReady = true;
    } else {
      serialBuf += c;
    }
  }

  // Only process the command once the full line has arrived
  if (serialReady) {
    if (serialBuf.startsWith("angle")) {
      sscanf(serialBuf.c_str(), "angle %f", &angle);
      Serial.print("Updated angle: ");
      Serial.println(angle);
      updateSpatialParams(angle);
    }
    serialBuf  = "";
    serialReady = false;
  }

}