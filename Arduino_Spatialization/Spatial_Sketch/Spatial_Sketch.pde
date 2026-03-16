#include <Arduino.h>
#include <math.h>
#include <vector>

const float head_width = 0.15; // meters
const float speed_of_sound = 343.0;
const int sample_rate = 44100;

float left_ear_x = -head_width / 2.0;
float right_ear_x = head_width / 2.0;

float transmit_x = 0.0;
float transmit_y = 5.0;

// 1 second stereo buffer
std::vector<std::vector<int>> audio_buffer(2, std::vector<int>(sample_rate/4, 0)); // 0.25 second buffer for testing

void setup() {

  Serial.begin(9600);

  // Generate 440 Hz tone
  for (int i = 0; i < sample_rate; i++) {
    int sample = (int)(32767 * sin(2 * M_PI * 440.0 * i / sample_rate));
    audio_buffer[0][i] = sample;
    audio_buffer[1][i] = sample;
  }
}

int clamp16(int v) {
  if (v > 32767) return 32767;
  if (v < -32768) return -32768;
  return v;
}

std::vector<std::vector<int>> update(
  float left_ear_distance,
  float right_ear_distance,
  const std::vector<std::vector<int>>& audio_buffer
) {

  float itd = (right_ear_distance - left_ear_distance) / speed_of_sound;

  // exaggerate effect
  itd *= 10;

  int itd_samples = round(itd * sample_rate);
  int delay = abs(itd_samples);

  float ild = 20.0 * log10(right_ear_distance / left_ear_distance);

  float gainL = pow(10, -ild / 20.0);
  float gainR = pow(10, ild / 20.0);

  int buf_size = audio_buffer[0].size();

  std::vector<std::vector<int>> processed_buffer(
      2, std::vector<int>(buf_size + delay, 0));

  if (itd_samples > 0) {

    // delay right ear
    for (int i = 0; i < buf_size; i++) {

      int idx = i + delay;

      if (idx < processed_buffer[1].size())
        processed_buffer[1][idx] = audio_buffer[1][i];

      processed_buffer[0][i] = audio_buffer[0][i];
    }

  } else {

    // delay left ear
    for (int i = 0; i < buf_size; i++) {

      int idx = i + delay;

      if (idx < processed_buffer[0].size())
        processed_buffer[0][idx] = audio_buffer[0][i];

      processed_buffer[1][i] = audio_buffer[1][i];
    }
  }

  // apply ILD gain and clamp
  for (int i = 0; i < processed_buffer[0].size(); i++) {

    int L = processed_buffer[0][i] * gainL;
    int R = processed_buffer[1][i] * gainR;

    processed_buffer[0][i] = clamp16(L);
    processed_buffer[1][i] = clamp16(R);
  }

  return processed_buffer;
}

void loop() {

  float left_ear_distance =
      sqrt(pow(transmit_x - left_ear_x, 2) + pow(transmit_y, 2));

  float right_ear_distance =
      sqrt(pow(transmit_x - right_ear_x, 2) + pow(transmit_y, 2));

  audio_buffer = update(left_ear_distance, right_ear_distance, audio_buffer);

  Serial.println("Audio updated");

  if (Serial.available() > 0) {
    String input = Serial.readStringUntil('\n');
    if (input.startsWith("pos")) {
      // Parse new transmitter position from input
      sscanf(input.c_str(), "pos %f %f", &transmit_x, &transmit_y);
      Serial.print("Updated transmitter position: ");
      Serial.print(transmit_x);
      Serial.print(", ");
      Serial.println(transmit_y);
    }
  }
  delay(10);


}

