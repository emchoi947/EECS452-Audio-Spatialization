////#define AUDIO_SAMPLE_RATE_EXACT 16000.0  // MUST be before Audio.h


#define PLAY_THRESHOLD 384
#define PREBUFFER_SAMPLES 640  // wait for 2 frames before starting playback to allow for better scheduling


#include <Audio.h>
#include <Wire.h>
#include <SD.h>
#include <SPI.h>
#include <SerialFlash.h>
#include "low_pass.h"
#include "band_pass.h"
#include "adpcm_decode.h"  // add the decoder header we wrote



#define LED 13


static bool prebuffered = false;

// --- SAMPLING RATE CONVERSION SETUP ---
#define UPSAMPLE_IN_RATE  16000
#define UPSAMPLE_OUT_RATE 44100
float resample_phase = 0.0f;
const float resample_step = (float)UPSAMPLE_IN_RATE / UPSAMPLE_OUT_RATE; // ~0.3628
float dynamic_step = resample_step;  // start at nominal



// Removing CLicking with target stuff
const int TARGET_AVAIL = 640;  // where you want the buffer to sit

// ADPCM enum
enum AdpcmParseState { ADPCM_SYNC1, ADPCM_SYNC2, ADPCM_PAYLOAD };
static AdpcmParseState adpcmState = ADPCM_SYNC1;



// --- Swap AudioInputI2S for two queues (one per channel) ---
// Uncomment the line below and comment out the AudioInputI2S and AudioRecordQueue lines to switch from packet input to AUdioshield input
AudioInputI2S         audioInput;       // replaces sine wave
AudioRecordQueue      recordQueue;      // lets us pull samples in loop()

AudioPlayQueue        queueL;
AudioPlayQueue        queueR;
// AudioFilterFIR        myFilterL;
// AudioFilterFIR        myFilterR;
AudioOutputI2S        audioOutput;
AudioControlSGTL5000  audioShield;

// Feed queues into filters, filters into output
AudioConnection c0(audioInput,  0, recordQueue, 0);


AudioConnection c1(queueL,    0, audioOutput, 0);
AudioConnection c2(queueR,    0, audioOutput, 1);


// AudioConnection c3(myFilterL, 0, audioOutput, 0);
// AudioConnection c4(myFilterR, 0, audioOutput, 1);

// ---  Debug Varalbles ---

    static int framesSinceLastPrint = 0;
    static uint16_t lastBurstTime = 0;
    static uint16_t lastFrameTime = 0;
// --- End of Debug System Setup ---


// --- Your spatialization globals ---
// --- Spatialization globals (unchanged) ---
const float head_width     = 0.15;
const float speed_of_sound = 343.0;
const int   sample_rate    = 16000;

float left_ear_x  = -head_width / 2.0;
float right_ear_x =  head_width / 2.0;
float transmit_x  = 0.0;
float transmit_y  = 5.0;

const int MAX_DELAY_SAMPLES = 12;
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


// --- ADPCM globals ---
#define ADPCM_FRAME_BYTES   86   // 128 samples / 2 +4 header bytes
#define AUDIO_FRAME_SAMPLES 160

// Predicted(2)-> step(3) -> n/a (4) -> rest
adpcm_state_t adpcm_rx_state = {0, 0};  // persists across frames
uint8_t  adpcm_buf[ADPCM_FRAME_BYTES];
uint16_t adpcm_buf_idx = 0;
int16_t  pcm_16k[AUDIO_FRAME_SAMPLES];

#define PCM_RING_SIZE 4096
int16_t  pcm_ring[PCM_RING_SIZE];
volatile int pcm_write_pos = 0;
volatile int pcm_read_pos  = 0;

enum UloState { ULO_SYNC, ULO_TYPE, ULO_LEN1, ULO_LEN2, ULO_PAYLOAD };
UloState uloState  = ULO_SYNC;
uint8_t  uloType   = 0;
uint16_t uloLen    = 0;
uint16_t uloPayIdx = 0;
uint8_t  uloPayload[128];
float    uloAngle_averaging[5] = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
int      uloAngle_idx = 0;


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

void angle_gain(float angle_diff, float &gL, float &gR, boolean angle_flipped) {
  float rad_angle = angle_diff * PI / 180.0f;
  float panL = abs(0.5f * (1.0f + sinf(rad_angle)));
  float panR = abs(0.5f * (1.0f - sinf(rad_angle)));
  gL = powf(panL, 0.5f);
  gR = powf(panR, 0.5f);
  if (angle_flipped) {
    gL = powf(panR, 0.5f);
    gR = powf(panL, 0.5f);
  }
  if (gL< 0.02) gL = 0.02;
  if (gR< 0.02) gR = 0.02;
}

void updateSpatialParams(float angle_diff) {
  float itd_max = (head_width) / speed_of_sound * 20.0f;

  if (angle_diff < -89.5f && angle_diff > -90.5f) angle_diff = -89.5f;
  if (angle_diff >  89.5f && angle_diff < 90.5f) angle_diff =  89.5f;
  
  boolean angle_flipped = false;

  if( angle_diff < -90.0f){

   angle_diff = angle_diff +180.0f;
    angle_flipped = true;
  }
  if( angle_diff > 90.0f){

   angle_diff = angle_diff -180.0f;
    angle_flipped = true;
  }

  float itd = itd_max * (angle_diff * 10 / 9) / 100;
  if (abs(itd) > itd_max) itd = itd / (itd/itd_max) ; // Clamp ITD to max by scaling it back proportionally
  int itd_samples = (int)roundf(itd * sample_rate);
  if( angle_flipped) itd_samples = -itd_samples; // Flip ITD if angle was flipped to maintain correct delay direction
  if (itd_samples > 0) {
    delayL_samples = itd_samples;
    delayR_samples = 0;
  } else {
    delayL_samples = 0;
    delayR_samples = abs(itd_samples);
  }

  angle_gain(angle_diff, gainL, gainR, angle_flipped);

  float ild = 20.0f * log10f((angle_diff+90.0f) / 90.0f + 1e-3f); // Avoid log(0) with small offset
  gainL_global = pow(powf(10.0f,  ild / 20.0f), 0.5);
  gainR_global = pow(powf(10.0f, -ild / 20.0f), 0.5);
  if(angle_flipped){
    gainL_global = pow(powf(10.0f, -ild / 20.0f), 0.5);
    gainR_global = pow(powf(10.0f,  ild / 20.0f), 0.5);
  }

  if(gainL_global >3) gainL_global = 3;
  if(gainR_global >3) gainR_global = 3;

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

// --- New Upsampling Function ---
int16_t upsample_next_cubic(void) {
    float frac = resample_phase;  // always in [0,1)

    // 4-point Hermite cubic
    int16_t s_1 = pcm_ring[(pcm_read_pos - 1 + PCM_RING_SIZE) % PCM_RING_SIZE];
    int16_t s0  = pcm_ring[(pcm_read_pos)                      % PCM_RING_SIZE];
    int16_t s1  = pcm_ring[(pcm_read_pos + 1)                  % PCM_RING_SIZE];
    int16_t s2  = pcm_ring[(pcm_read_pos + 2)                  % PCM_RING_SIZE];

    float a = -0.5f*s_1 + 1.5f*s0 - 1.5f*s1 + 0.5f*s2;
    float b =       s_1 - 2.5f*s0 + 2.0f*s1 - 0.5f*s2;
    float c = -0.5f*s_1            + 0.5f*s1;
    float d =                  s0;

    int32_t out = (int32_t)(a*frac*frac*frac + b*frac*frac + c*frac + d);

    resample_phase += dynamic_step;
    int consumed = (int)resample_phase;
    pcm_read_pos += consumed;
    resample_phase -= consumed;

    return (int16_t)(out > 32767 ? 32767 : out < -32768 ? -32768 : out);
}

// Sending AT commands to the U-locate module so it will go into the proper format we desire

void sendATCommand(const char* cmd) {
    Serial8.print(cmd);
    Serial8.print("\r");   // AT commands need CR+LF
    Serial.print("Sent: ");  // echo to USB debug
    Serial.println(cmd);
}

// In setup() after Serial8.begin() — configure the module
void configureUloModule() {
    delay(2000);  // wait for NINA-B4 to boot
    while (Serial8.available()) Serial8.read();  // flush

    // Set report interval to 100ms
    sendATCommand("AT+UDFCFG=9,100");
    delay(500);
    while (Serial8.available()) Serial8.read();

    // Set binary output mode (already confirmed working)
    sendATCommand("AT+UDFCFG=7,2");
    delay(500);
    while (Serial8.available()) Serial8.read();

    // Enable direction finding — this starts the data stream
    sendATCommand("AT+UDFENABLE=1");
    delay(500);
    while (Serial8.available()) Serial8.read();
}

void setup() {
  Serial.begin(115200);
  delay(300);
  pinMode(LED, OUTPUT);

  // Hardware Serial
  while (!Serial) { delay(1); }
  Serial8.begin(115200);
  delay(300);
  Serial7.begin(115200);
  //Audio shield


  AudioMemory(500);
  audioShield.enable();
  audioShield.volume(0.55);

  // Sampling Rate Printer
  Serial.print("Sample rate: ");
  Serial.println(AUDIO_SAMPLE_RATE_EXACT);
  delay(500);


  //Memory set
  memset(ringL, 0, sizeof(ringL));
  memset(ringR, 0, sizeof(ringR));
  memset(pcm_ring, 0, sizeof(pcm_ring));

  // Enable the line input — use AUDIO_INPUT_LINEIN or MIC as needed
  audioShield.inputSelect(AUDIO_INPUT_LINEIN);
  audioShield.lineInLevel(7);   // 0–15, adjust to taste


  // myFilterL.begin(fir_list[start_idx].coeffs, fir_list[start_idx].num_coeffs);
  // myFilterR.begin(fir_list[start_idx].coeffs, fir_list[start_idx].num_coeffs);


  //Initalizign U-locate module

  


  configureUloModule();


  // Generate initial tone and apply starting position
  // float ld = sqrtf(powf(transmit_x - left_ear_x,  2) + powf(transmit_y, 2));
  // float rd = sqrtf(powf(transmit_x - right_ear_x, 2) + powf(transmit_y, 2));
  // applySpatialisation(ld, rd);
    recordQueue.begin();  // start capturing input blocks

}


// Setting Up the code reading in the UUDF message from the sending thing.

// --- u-locateEmbed parser globals (add at top) ---


void processUloMessage(uint8_t type, uint8_t *p, uint16_t len) {
    if (type == 0x01 && len >= 7) {
        uloAngle_averaging[uloAngle_idx] = (float)(int8_t)p[6];  // cast to signed first!
        uloAngle_idx = (uloAngle_idx + 1) % 5;
        // Calculate average azimuth
        float azimuth = 0.0f;
        for (int i = 0; i < 5; i++) {
            azimuth += uloAngle_averaging[i];
        }
        azimuth /= 5.0f;

        Serial.print("Azimuth: ");
        Serial.println(azimuth);
        updateSpatialParams(azimuth);
    }
}


void fadeBlock(int16_t *block, bool fadeIn) {
    for (int i = 0; i < AUDIO_BLOCK_SAMPLES; i++) {
        float gain = (float)i / AUDIO_BLOCK_SAMPLES;
        if (!fadeIn) gain = 1.0f - gain;
        block[i] = (int16_t)(block[i] * gain);
    }
}

static bool wasStarved = false;


void loop() {
  // Redundant outer getBuffer() calls removed (were causing unused variable warnings)
    static int frameCount = 0;
    static int dropCount = 0;
  
    while (Serial7.available() > 0) {
    uint8_t b = Serial7.read();

    switch (adpcmState) {

        case ADPCM_SYNC1:
            if (b == 0xAA) adpcmState = ADPCM_SYNC2;
            break;

        case ADPCM_SYNC2:
            if (b == 0xFF) {
                adpcm_buf_idx = 0;   // fresh frame
                adpcmState = ADPCM_PAYLOAD;
            } else if (b == 0xAA) {
                adpcmState = ADPCM_SYNC2;  // handle 0xAA 0xAA 0xFF
            } else {
                adpcmState = ADPCM_SYNC1;  // bad byte, resync
            }
            break;

        case ADPCM_PAYLOAD:
            adpcm_buf[adpcm_buf_idx++] = b;
            if (adpcm_buf_idx >= ADPCM_FRAME_BYTES) {
                adpcmState = ADPCM_SYNC1;  // done, wait for next sync
                // decode block goes here:
                uint8_t frame_copy[ADPCM_FRAME_BYTES];
                memcpy(frame_copy, adpcm_buf, ADPCM_FRAME_BYTES);
                frameCount++;

                memcpy(&adpcm_rx_state.predicted, frame_copy, 2);
                memcpy(&adpcm_rx_state.step_index, frame_copy + 2, 1);
                adpcm_decode_frame(frame_copy, pcm_16k, &adpcm_rx_state);

                for (int i = 0; i < AUDIO_FRAME_SAMPLES; i++) {
                    if ((pcm_write_pos - pcm_read_pos) < PCM_RING_SIZE - 1) {
                        pcm_ring[pcm_write_pos % PCM_RING_SIZE] = pcm_16k[i];
                        pcm_write_pos++;
                    }
                }
            }
            break;
    }
}




  // if (adpcm_buf_idx >= ADPCM_FRAME_BYTES) {

  //     // DECODING


  //   if (frameCount % 50 == 0) {
  //       Serial.print("Frame end:   ");
  //       Serial.print(pcm_16k[125]); Serial.print(", ");
  //       Serial.print(pcm_16k[126]); Serial.print(", ");
  //       Serial.println(pcm_16k[127]);
  //   }
  //   if (frameCount % 50 == 1) {
  //       Serial.print("Frame start: ");
  //       Serial.print(pcm_16k[0]); Serial.print(", ");
  //       Serial.print(pcm_16k[1]); Serial.print(", ");
  //       Serial.println(pcm_16k[2]);
  //   }

  //   // Copy frame data before resetting index to avoid overwriting during decoding



  //     uint8_t frame_copy[ADPCM_FRAME_BYTES];
  //     memcpy(&frame_copy, adpcm_buf, ADPCM_FRAME_BYTES);
  //     adpcm_buf_idx = 0;  // now safe to reset
  //     frameCount++;


  //   memcpy(&adpcm_rx_state.predicted, frame_copy, 2); // Initialize predicted sample with last sample of previous frame for continuity
  //   memcpy(&adpcm_rx_state.step_index, frame_copy + 2, 1); // Initialize step index with last frame's value

  //   adpcm_decode_frame(frame_copy, pcm_16k, &adpcm_rx_state);


  //    // Check for silence/garbage frames
  //   int32_t energy = 0;
  //   for (int i = 0; i < AUDIO_FRAME_SAMPLES; i++) energy += abs(pcm_16k[i]);
  //   if (energy == 0) dropCount++;
    
  //   // if (frameCount % 100 == 0) {
  //   //     Serial.print("Frames: "); Serial.print(frameCount);
  //   //     Serial.print(" Drops: "); Serial.println(dropCount);
  //   // }

  //   // Push 160 decoded samples into pcm_ring
  //   for (int i = 0; i < AUDIO_FRAME_SAMPLES; i++) {
  //     pcm_ring[pcm_write_pos % PCM_RING_SIZE] = pcm_16k[i];
  //     pcm_write_pos++;
  //   }

  // }


  
  

   int available = pcm_write_pos - pcm_read_pos;
    //Serial.printf("avaliable = %d write %d Read %d Upsampled %d\n", available, pcm_write_pos, pcm_read_pos, (int)(available / resample_step));
    if (!prebuffered) {
    if (available >= PREBUFFER_SAMPLES) prebuffered = true;
    return;  // don't play yet
    }


   if (available >= PLAY_THRESHOLD) {
    int16_t *blockL = queueL.getBuffer();
    int16_t *blockR = queueR.getBuffer();
    // Guard against runaway buffer before processing



    if (blockL == NULL || blockR == NULL) {
      Serial.println("AUDIO BUFFER NULL");
    }


    
    if (blockL != NULL && blockR != NULL) {


      // Bit Wise step adjustment to account for slight differeences in the actual rate we are recieveing
      static float integral_error = 0.0f;

      // Inside your playback block:
      int avail = pcm_write_pos - pcm_read_pos;
      float error = (avail - TARGET_AVAIL) / (float)TARGET_AVAIL;

      integral_error += error * 0.0001f;                          // accumulates slowly
      integral_error = constrain(integral_error, -0.05f, 0.05f); // prevent windup

      float target_step = resample_step * (1.0f + 0.05f * error + integral_error);
      dynamic_step += 0.1f * (target_step - dynamic_step);
      dynamic_step = constrain(dynamic_step, resample_step * 0.9f, resample_step * 1.1f);
     


      for (int i = 0; i < AUDIO_BLOCK_SAMPLES; i++) {


       int16_t dry = upsample_next_cubic();
        ringL[write_pos] = dry;
        ringR[write_pos] = dry;

        int readL = ringIndex(write_pos - delayL_samples);
        int readR = ringIndex(write_pos - delayR_samples);

        blockL[i] = clamp16((int)(ringL[readL] * gainL_global * gainL));
        blockR[i] = clamp16((int)(ringR[readR] * gainR_global * gainR));
        // blockL[i] = dry; // BYPASSING SPATIALIZATION FOR TESTING
        // blockR[i] = dry; // BYPASSING SPATIALIZATION FOR TESTING

        write_pos = ringIndex(write_pos + 1);
      }
      if (wasStarved) {
          fadeBlock(blockL, true);   // fade in after silence
          fadeBlock(blockR, true);
          wasStarved = false;
      }

      queueL.playBuffer();
      queueR.playBuffer();
    }
  }
  else if (available == 0){
     prebuffered = false;  // reset if we fully drain
  }
  else {
    int16_t *blockL = queueL.getBuffer();
    int16_t *blockR = queueR.getBuffer();
    if (blockL != NULL && blockR != NULL) {

        memset(blockL, 0, AUDIO_BLOCK_SAMPLES * sizeof(int16_t));
        memset(blockR, 0, AUDIO_BLOCK_SAMPLES * sizeof(int16_t));
        if (!wasStarved) {
            // last real block — fade it out
            fadeBlock(blockL, false);
            fadeBlock(blockR, false);
            wasStarved = true;
        }
        queueL.playBuffer();
        queueR.playBuffer();
    }
}

  // Non-blocking serial read
  while (Serial.available() > 0) {
    char c = Serial.read();
    if (c == '\n') serialReady = true;
    else serialBuf += c;
  }
  if (serialReady) {
    if (serialBuf.startsWith("angle")) {
      sscanf(serialBuf.c_str(), "angle %f", &angle);
      Serial.print("Manual angle: "); Serial.println(angle);
      updateSpatialParams(angle);
    }
    serialBuf = "";
    serialReady = false;
  }

// --- Drop into loop() replacing your Serial8 block ---
  while (Serial8.available() > 0) {
    
      uint8_t b = Serial8.read();
    
      switch (uloState) {

        case ULO_SYNC:
          
            if (b == 0xFE) uloState = ULO_TYPE;
            break;

        case ULO_TYPE:
            Serial.println(); // Add a newline for better readability in the logs
            Serial.print("Type byte: 0x");
            Serial.println(b, HEX);  // ADD THIS to see what type is actually coming
            if (b == 0x01 || b == 0x02) {
                uloType  = b;
                uloState = ULO_LEN1;
            } else {
                // Serial.print("Unexpected type, resyncing: 0x");
                // Serial.println(b, HEX);
                uloState = ULO_SYNC;
            }
            break;
          case ULO_LEN1:
              uloLen   = b;              // low byte
              uloState = ULO_LEN2;
              break;

          case ULO_LEN2:
              uloLen  |= ((uint16_t)b << 8);   // high byte
              uloPayIdx = 0;
              uloState  = (uloLen > 0) ? ULO_PAYLOAD : ULO_SYNC;
              break;

          case ULO_PAYLOAD:
              if (uloPayIdx < sizeof(uloPayload))
                  uloPayload[uloPayIdx] = b;
              uloPayIdx++;
              if (uloPayIdx >= uloLen) {
                  processUloMessage(uloType, uloPayload, uloLen);
                  uloState = ULO_SYNC;
              }
              break;
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
    static uint32_t lastPrint = 0;
    if (millis() - lastPrint > 500) {
        lastPrint = millis();
        Serial.print("write="); Serial.print(pcm_write_pos);
        Serial.print(" read="); Serial.print(pcm_read_pos);
        Serial.print(" avail="); Serial.print(pcm_write_pos - pcm_read_pos);
        Serial.print(" audioMem="); Serial.println(AudioMemoryUsageMax());
        Serial.print(" PCM Ring[0]: "); Serial.print(pcm_ring[pcm_read_pos % PCM_RING_SIZE]);
    }
    // when you successfully decode a frame:
    uint32_t now = millis();


    uint32_t gap = now - lastFrameTime;
    framesSinceLastPrint++;

// Print every 500ms regardless
if (now - lastBurstTime > 500) {
    lastFrameTime = now;

    // Serial.print("Frames in last 500ms: ");
    // Serial.print(framesSinceLastPrint);
    // Serial.print(", last gap ms: ");
    // Serial.println(gap);
    framesSinceLastPrint = 0;
    lastBurstTime = now;
    }

  static uint32_t lastRateCheck = 0;
static int32_t lastWritePos = 0;

  if (millis() - lastRateCheck > 5000) {
      float actualRate = (pcm_write_pos - lastWritePos) / 5.0f;
      Serial.print("Actual write rate (samples/sec): ");
      Serial.println(actualRate);
      lastWritePos = pcm_write_pos;
      lastRateCheck = millis();
  }

}



  /* 
  void loop() {
      while (Serial.available())  Serial8.write(Serial.read());
      while (Serial8.available()) Serial.write(Serial8.read());
  }
