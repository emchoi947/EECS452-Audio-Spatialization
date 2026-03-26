#include "common.h"
#include "i2s_sampling.h"
#include "ble_transmission.h"
void app_main(void) {
    // Initialize the I2S Hardware
    i2s_init_std_simplex();

    //Initialize the bluetooth communication
    ble_transmission_init();

    //Initialize a queue to hold frames of audio data
    audio_frame_queue = xQueueCreate(10, FRAME_SIZE_BYTES);

    // Begin reading and transmitting the audio data
    xTaskCreate(i2s_read_task, "i2s_cap", 4096, NULL, 10, NULL);
    xTaskCreate(ble_transmit_task, "ble_tx", 8192, NULL, 5, NULL);
}