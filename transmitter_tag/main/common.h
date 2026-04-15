//common.h
#ifndef COMMON_H
#define COMMON_H

//Standard C libraries
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <sys/unistd.h>
#include <sys/stat.h>
//Configuration files from ESP-IDF
#include "sdkconfig.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_system.h"
#include "esp_vfs_fat.h"
#include "freertos/FreeRTOS.h"
#include "freertos/FreeRTOSConfig.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/i2s_std.h"
#include "driver/gpio.h"
#include "driver/spi_common.h"
#include "sdmmc_cmd.h"
//#include "format_wav.h" espressif audio codec
#include "esp_log.h"



#define ADPCM_PACKET_BYTES (sizeof(adpcm_state_t) + ADPCM_FRAME_BYTES) //used for prepending the adpcm data to the audio packet
#define FRAME_SIZE_BYTES 640 //160 samples per frame that are each 4 bytes
#define AUDIO_FRAME_SAMPLES 160 //16kHz sampling rate at 10ms frame lengths
#define ADPCM_FRAME_BYTES 80

extern QueueHandle_t audio_frame_queue; //Real declaration happens in main.c
void i2s_read_task(void *args);

#endif