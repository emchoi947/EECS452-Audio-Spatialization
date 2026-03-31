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

static void ble_tx_audio(void *pvParameters){
    int16_t audio_frame[AUDIO_FRAME_SAMPLES];
    uint8_t compressed[ADPCM_FRAME_BYTES];
    adpcm_state_t adpcm_state = {0, 0};
    struct os_mbuf *om;
    int rc;

    while(1) {
        if (xQueueReceive(audio_frame_queue, audio_frame, portMAX_DELAY) != pdTRUE) {
            continue;
        }
        if (!notify_state) {
            ble_tx_audio_stop();
            continue;
        }

        adpcm_encode_frame(audio_frame, compressed, &adpcm_state);

        /* Chunk and send */
        uint16_t mtu        = ble_att_mtu(conn_handle) - 3;
        uint16_t offset     = 0;

        while (offset < ADPCM_FRAME_BYTES) {
            uint16_t chunk_size = MIN(mtu, ADPCM_FRAME_BYTES - offset);

            om = ble_hs_mbuf_from_flat(compressed + offset, chunk_size);
            if (om == NULL) {
                ESP_LOGE("BLE", "mbuf alloc failed at offset %d", offset);
                break;
            }

            rc = ble_gatts_notify_custom(conn_handle, hrs_hrm_handle, om);
            if (rc != 0) {
                ESP_LOGE("BLE", "Notify failed at offset %d: %d", offset, rc);
                break;
            }

            offset += chunk_size;
        }
    }
}
//notes:
//modlog is NimBLE specific and so better to use than ESP_LOGI
static int bleaudio_gap_event(struct ble_gap_event *event, void *arg) {
    switch (event->type) {
    case BLE_GAP_EVENT_CONNECT:
        //a new connection was established or an attempt failed
        MODLOG_DFLT(INFO, "connection %s; status=%d\n", 
                    event->connect.status == 0 ? "established" : "failed",
                    event->connect.status);
        if (event->connect.status != 0) {
            //connection failed; resume advertising
            ble_audio_advertise();
        }
        conn_handle = event->connect.conn_handle;
        break;

    case BLE_GAP_EVENT_DISCONNECT:
        MODLOG_DFLT(INFO, "disconnect; reason=%d\n", event->disconnect.reason);
        notify_state = false;
        ble_tx_audio_stop();
        conn_handle = BLE_HS_CONN_HANDLE_NONE;
        //Connection terminated; resume advertising 
        ble_audio_advertise();
        break;
    case BLE_GAP_EVENT_ADV_COMPLETE:
        MODLOG_DFLT(INFO, "adv complete\n");
        ble_audio_advertise();
        break;
    case BLE_GAP_EVENT_SUBSCRIBE:
        MODLOG_DFLT(INFO, "subscribe event; cur_notify=%d\n value handle; "
                    "val_handle=%d\n",
                    event->subscribe.cur_notify, hrs_hrm_handle);
        if (event->subscribe.attr_handle == hrs_hrm_handle) {
            notify_state = event->subscribe.cur_notify;
            ble_tx_audio_reset();
        } else if (event->subscribe.attr_handle != hrs_hrm_handle) {
            notify_state = event->subscribe.cur_notify;
            ble_tx_audio_stop();
        }
        ESP_LOGI("BLE_GAP_SUBSCRIBE_EVENT", "conn_handle from subscribe=%d", conn_handle);
        break;
    case BLE_GAP_EVENT_MTU:
        MODLOG_DFLT(INFO, "mtu update event; conn_handle=%d mtu=%d\n",
                    event->mtu.conn_handle,
                    event->mtu.value);
        break;
    }
    return 0;
}

void ble_audio_advertise(void){
    //GAP - device name definition
    struct ble_hs_adv_fields fields;
    const char *device_name;
    int rc;

    memset(&fields, 0, sizeof(fields));
    fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;
    fields.tx_pwr_lvl_is_present = 1;
    fields.tx_pwr_lvl = BLE_HS_ADV_TX_PWR_LVL_AUTO;
    device_name = ble_svc_gap_device_name(); //reads the device name which was set before this gets called
    fields.name = (uint8_t *)device_name;
    fields.name_len = strlen(device_name);
    fields.name_is_complete = 1;
    rc = ble_gap_adv_set_fields(&fields); //configure GAP
    if (rc != 0) {
        MODLOG_DFLT(ERROR, "error setting advertisement data; rc=%d\n", rc);
        return;
    }

    //GAP - device connectivity definition
    struct ble_gap_adv_params adv_params;
    memset(&adv_params, 0, sizeof(adv_params));
    adv_params.conn_mode = BLE_GAP_CONN_MODE_UND; //undirected-connectable
    adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN; //general-discoverable
    rc = ble_gap_adv_start(ble_addr_type, NULL, BLE_HS_FOREVER, &adv_params, ble_gap_event, NULL);
    if (rc != 0) {
        MODLOG_DFLT(ERROR, "error enabling advertisement; rc=%d\n", rc);
        return;
    }
}

void ble_app_on_sync(void) {
    int rc;

    rc = ble_hs_id_infer_auto(0, &ble_addr_type); //determines the best address type automatically
    assert(rc == 0);

    uint8_t addr_val[6] = {0};
    rc = ble_hs_id_copy_addr(blehr_addr_type, addr_val, NULL);

    MODLOG_DFLT(INFO, "Device Address: ");
    print_addr(addr_val);
    MODLOG_DFLT(INFO, "\n");
    ble_audio_advertise(); //define the ble connection (see function)
}

void ble_app_on_reset(int reason) {
    MODLOG_DFLT(ERROR, "Resetting state; reason=%d\n", reason);
}

void ble_transmission_init() {
    //initialize the NVS flash for configuration data
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    //initialize the host stack using NimBLE
    ret = nimble_port_init();
    if (ret != ESP_OK) {
        MODLOG_DFLT(ERROR, "Failed to init nimble %d \n", ret);
        return;
    }
    //begin setting up NimBLE configuration
    //set device name
    ble_srv_gap_device_name_set("BLE-Microphone");
    //initialize the generic access profile (GAP)
    ble_svc_gap_init();
    //initialize the generic attribute profile (GATT) (see ble_transmission.h)
    ble_svc_gatt_init();
    //
    ble_gatts_count_cfg(gatt_svcs);
    //initialize the 
    ble_gatts_add_svcs(gatt_svcs);
    ble_hs_cfg.sync_cb = ble_app_on_sync;
    ble_hs_cfg.reset_cb = ble_app_on_reset;
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