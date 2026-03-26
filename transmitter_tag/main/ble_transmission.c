/*
Name: 
ble_transmission.c
* 
Authorship:
EECS 452 W26 - Audio Spatialization
Created on 03/16/26
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

#include "ble_transmission.h"
#include "lc3_encoder.h"

void ble_transmit_task(void *args) {
    uint8_t *processing_buf = (uint8_t *)malloc(FRAME_SIZE_BYTES);

    while(1) {
        if(xQueueReceive(audio_frame_queue, processing_buf, portMAX_DELAY == pdTRUE)) {
            //Compress the data into the LC3 format
            lc3_encode(processing_buf);
            //Send the compressed data via bluetooth
            ble_transmit(processing_buf);
        }
    }
    free(processing_buf);
}