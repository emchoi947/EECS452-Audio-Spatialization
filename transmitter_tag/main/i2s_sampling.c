/*
Name: 
i2s_sampling.c
* 
Authorship:
EECS 452 W26 - Audio Spatialization
Created on 03/14/26
Written by Thomas Oscar
*
Description:

Hardware - The USB-C (USB) port on the ESP32-H2 which will use the onboard Serial/JTAG 
controller to implement a USB Audio Class (UAC)

Software - i2s_sampling.c is intended to sample audio from a USB-C connection at a predetermined 
frequency and store it on a buffer in RAM (GDMA) for further processing. In the context of this
project, ble_transmission.c will use DMA to read from this buffer, compress the data into the 
LE Audio format (IC3) using ic3_encoder.c, and finally transmit the audio for receiving and 
calculating AoA information.  
*/

#include "common.h"
#include "i2s_sampling.h"
#include "adpcm.h"

#define I2S_SCK_IO1 //add GPIO pin
#define I2S_WS_IO1 //add GPIO pin
#define I2S_SD_IO1 //add GPIO pin

static i2s_chan_handle_t rx_handle;

//Initialize the I2S channel in Simplex mode as a receiver 
// Configuration for 16kHz, 10ms frame
void i2s_init_std_simplex(void){
    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_AUTO, I2S_ROLE_MASTER);
    ESP_ERROR_CHECK(i2s_new_channel(&chan_cfg, NULL, &rx_handle));
    i2s_std_config_t std_rx_cfg = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(16000),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_24BIT, I2S_SLOT_MODE_MONO),
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED,
            .bclk = I2S_SCK_IO1,
            .ws = I2S_WS_IO1,
            .dout = I2S_GPIO_UNUSED,
            .din = I2S_SD_IO1,
            .invert_flags = {
                .mclk_inv = false,
                .bclk_inv = false,
                .ws_inv = false,
            },
        },
    };
    ESP_ERROR_CHECK(i2s_channel_init_std_mode(rx_handle, &std_rx_cfg));
    ESP_ERROR_CHECK(i2s_channel_enable(rx_handle));
}

void i2s_read_task(void *args) {
    uint8_t *temp_buffer = (uint8_t *)malloc(FRAME_SIZE_BYTES);
    int16_t processing_buf[160];
    size_t bytes_read = 0;

    while(1){
        esp_err_t result = i2s_channel_read(rx_handle, temp_buffer, FRAME_SIZE_BYTES, &bytes_read, portMAX_DELAY);
        if(result == ESP_OK && bytes_read == FRAME_SIZE_BYTES){
            //Pass the sampled data to the FreeRTOS queue for processing
            int32_t *raw_samples = upsample_32(temp_buffer);
            for(int i=0; i<160; i++){
                processing_buf[i] = (int16_t)(raw_samples[i]>>8);
            }
            if(xQueueSend(audio_frame_queue, processing_buf, 0) != pdTRUE){
                ESP_LOGW("I2S", "Queue full! Dropping audio frame.")
            }
        }
        else if(result == ESP_ERR_TIMEOUT){
            // Reading timeout
            ESP_LOGW("I2S", "Read timeout");
        }
        else{
            // NULL ptr or the handle is not the true Rx handle OR
            // I2S not ready to ready
            ESP_LOGE("I2S", "I2S Read Error: %s", esp_err_to_name(result));
        }
    }
    free(temp_buffer);
}
