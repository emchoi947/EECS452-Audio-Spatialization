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

static int ble_gap_event(struct ble_gap_event *event, void *arg) {
    
}

void ble_add_advertise(void){
    //GAP - device name definition
    struct ble_hs_adv_fields fields;
    const char *device_name;
    memset(&fields, 0, sizeof(fields));
    device_name = ble_svc_gap_device_name(); //reads the device name which was set before this gets called
    fields.name = (uint8_t *)device_name;
    fields.name_len = strlen(device_name);
    fields.name_is_complete = 1;
    ble_gap_adv_set_fields(&fields); //configure GAP

    //GAP - device connectivity definition
    struct ble_gap_adv_params adv_params;
    memset(&adv_params, 0, sizeof(adv_params));
    adv_params.conn_mode = BLE_GAP_CONN_MODE_UND; //undirected-connectable
    adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN; //general-discoverable
    ble_gap_adv_start(ble_addr_type, NULL, BLE_HS_FOREVER, &adv_params, ble_gap_event, NULL);
}

void ble_app_on_sync(void) {
    ble_hs_id_infer_auto(0, &ble_addr_type); //determines the best address type automatically
    ble_app_advertise(); //define the ble connection (see function)
}

void ble_transmission_init() {
    //initialize the NVS flash for configuration data
    nvs_flash_init();
    //initialize the host stack using NimBLE
    nimble_port_init();
    //begin setting up NimBLE configuration
    //set device name
    ble_srv_gap_device_name_set("BLE-Microphone");
    //initialize the generic access profile (GAP)
    ble_srv_gap_init();
    //initialize the generic attribute profile (GATT) (see ble_transmission.h)
    ble_gatts_count_cfg(gatt_svcs);
    //initialize the 
    ble_gatts_add_svcs(gatt_svcs);
    ble_hs_cfg.sync_cb = ble_app_on_sync;
}

void ble_host_task(void *args) {
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