#include "common.h"
#include "i2s_sampling.h"
#include "ble_transmission.h"
#include "aoa_ble.h"

QueueHandle_t audio_frame_queue;

void app_main(void) {
    // Initialize the I2S Hardware
    //i2s_init_std_simplex();

    //Initialize the bluetooth communications
    ble_transmission_init(); //audio data transmission through notifications
    

    //Initialize a queue to hold frames of audio data
    audio_frame_queue = xQueueCreate(10, AUDIO_FRAME_SAMPLES * sizeof(int16_t));
    assert(audio_frame_queue != NULL);

    // Begin reading and transmitting the audio data
    xTaskCreate(i2s_read_task, "i2s_cap", 4096, NULL, 10, NULL);
    xTaskCreate(ble_tx_audio, "ble_audio_tx", 4096, NULL, 5, NULL);
    nimble_port_freertos_init(ble_host_task);
    
    vTaskDelete(NULL);
}