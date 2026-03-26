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
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/i2s_std.h"
#include "driver/gpio.h"
#include "driver/spi_common.h"
#include "sdmmc_cmd.h"
#include "format_wav.h"
#include "esp_log.h"

#define FRAME_SIZE_BYTES 640 //160 samples that are each 4 bytes

extern QueueHandle_t audio_frame_queue; //Real declaration happens in main.c
void i2s_read_task(void *args);

#endif