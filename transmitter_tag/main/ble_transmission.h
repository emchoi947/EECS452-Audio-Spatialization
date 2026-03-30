//ble_transmission.h
#ifndef BLE_TRANSMISSION_H
#define BLE_TRANSMISSION_H

#include "common.h"
#include "lc3_encoder.h"
#include "freertos/event_groups.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "esp_log.h"
#include "esp_nimble_hci.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"


char *TAG = "BLE-Microphone";
uint8_t ble_addr_type;
void ble_app_advertise(void);

void ble_app_on_sync(void);

void ble_transmission_init();

void ble_host_task(void *args);


#endif