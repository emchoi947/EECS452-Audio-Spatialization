//ble_transmission.h
#ifndef BLE_TRANSMISSION_H
#define BLE_TRANSMISSION_H

#include "common.h"
#include "lc3_encoder.h"

void ble_transmit(int16_t &processing_buf);

void ble_transmit_task(void *args);


#endif