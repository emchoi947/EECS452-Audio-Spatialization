#ifndef BLE_RECEPTION_H
#define BLE_RECEPTION_H

/* BLE */
#include "nimble/ble.h"
#include "host/ble_hs.h"
/* ESP */
#include "modlog/modlog.h"
#include "esp_central.h"
/* UART */
#include "driver/uart.h"
#include "driver/gpio.h"
#ifdef __cplusplus
extern "C" {
#endif

/* UART */
#define UART_PORT       UART_NUM_1
#define UART_TX_PIN     GPIO_NUM_4
#define UART_RX_PIN     GPIO_NUM_5
#define UART_BAUD_RATE  115200

void uart_init(void);

/* BLE */
struct ble_hs_adv_fields;
struct ble_gap_conn_desc;
struct ble_hs_cfg;
union ble_store_value;
union ble_store_key;

static const ble_uuid128_t remote_svc_uuid =
    BLE_UUID128_INIT(0x12, 0x34, 0x56, 0x78, 0x9a, 0xbc, 0xde, 0xf0,
               0x12, 0x34, 0x56, 0x78, 0x9a, 0xbc, 0xde, 0xf0);

static const ble_uuid128_t remote_data_chr_uuid =
    BLE_UUID128_INIT(0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88,
               0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88);

void ble_reception_init();

#ifdef __cplusplus
}
#endif

#endif
