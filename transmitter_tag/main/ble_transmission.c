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
#include "adpcm.h"
#include "aoa_ble.h"
#include "common.h"

static const char *TAG = "BLE-Microphone";
uint8_t ble_addr_type;
static bool notify_state;
static uint16_t conn_handle;
uint16_t hrs_hrm_handle;

//Define custom UUIDs
static ble_uuid128_t audio_svc_uuid = {
    .u   = { .type = BLE_UUID_TYPE_128 },
    .value = { 0x12, 0x34, 0x56, 0x78, 0x9a, 0xbc, 0xde, 0xf0,
               0x12, 0x34, 0x56, 0x78, 0x9a, 0xbc, 0xde, 0xf0 }
};

static ble_uuid128_t audio_chr_uuid = {
    .u   = { .type = BLE_UUID_TYPE_128 },
    .value = { 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88,
               0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88 }
};

/*Delete or comment out these lines when using custom UUIDs
static ble_uuid16_t audio_svc_uuid = BLE_UUID16_INIT(0x180D);
static ble_uuid16_t audio_chr_uuid = BLE_UUID16_INIT(0x2A37);
*/

// GATT read callback — required by gatt_svcs table
static int device_read(uint16_t conn_handle, uint16_t attr_handle,
                       struct ble_gatt_access_ctxt *ctxt, void *arg) {
    const char *msg = "ESP32-H2 Audio Tag";
    os_mbuf_append(ctxt->om, msg, strlen(msg));
    return 0;
}

// GATT write callback — required by gatt_svcs table
int device_write(uint16_t conn_handle, uint16_t attr_handle,
                        struct ble_gatt_access_ctxt *ctxt, void *arg) {
    // Not used by transmitter, but required
    return 0;
}


static const struct ble_gatt_chr_def audio_characteristics[] = {
    {
        .uuid       = &audio_chr_uuid.u,
        .flags      = BLE_GATT_CHR_F_NOTIFY,
        .val_handle = &hrs_hrm_handle,
        .access_cb  = device_read,
    },
    { 0 }
};

static const struct ble_gatt_svc_def gatt_svcs[] = {
    {
        .type            = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid            = &audio_svc_uuid.u,
        .characteristics = audio_characteristics,
    },
    { 0 }
};


void ble_tx_audio(void *pvParameters){
    int16_t audio_frame[AUDIO_FRAME_SAMPLES];
    //uint8_t compressed[ADPCM_FRAME_BYTES];
    uint8_t packet[ADPCM_PACKET_BYTES]; //code for testing: packet with prepended adpcm state
    adpcm_state_t adpcm_state = {0, 0, 0};
    struct os_mbuf *om;
    int rc;

    while(1) {
        if (xQueueReceive(audio_frame_queue, audio_frame, portMAX_DELAY) != pdTRUE) {
            continue;
        }
        if (!notify_state) {
            //ble_tx_audio_stop();
            continue;
        }
        // build the header to prepend to the audio packet BEFORE encoding
        adpcm_state_t *hdr = (adpcm_state_t *)packet; //puts the struct at the beginning of packet
        hdr->predicted = adpcm_state.predicted;
        hdr->step_index = adpcm_state.step_index;
        hdr->reserved = 0;
        //encode the audio into packet
        adpcm_encode_frame(audio_frame, packet + sizeof(adpcm_state_t), &adpcm_state); //starts inserting after the header positions of packet

        /* Chunk and send */
        uint16_t mtu        = ble_att_mtu(conn_handle) - 3;
        uint16_t offset     = 0;

        while (offset < ADPCM_PACKET_BYTES) {
            uint16_t chunk_size = MIN(mtu, ADPCM_PACKET_BYTES - offset);

            om = ble_hs_mbuf_from_flat(packet + offset, chunk_size);
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
        // Pace to real-time: 160 samples at 16kHz = 10ms
        //vTaskDelay(pdMS_TO_TICKS(10));
    }
}
//notes:
//modlog is NimBLE specific and so better to use than ESP_LOGI
int bleaudio_gap_event(struct ble_gap_event *event, void *arg) {
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
    //version solving legacy advertising issues
    case BLE_GAP_EVENT_DISCONNECT:
        MODLOG_DFLT(INFO, "disconnect; reason=%d\n", event->disconnect.reason);
        notify_state = false;
        conn_handle = BLE_HS_CONN_HANDLE_NONE;
        ble_gap_ext_adv_stop(0);   // stop before reconfiguring
        ble_audio_advertise();
        break;

    case BLE_GAP_EVENT_ADV_COMPLETE:
        MODLOG_DFLT(INFO, "adv complete\n");
        ble_gap_ext_adv_stop(0);
        ble_audio_advertise();
        break;
    //previous version
    /*case BLE_GAP_EVENT_DISCONNECT:
        MODLOG_DFLT(INFO, "disconnect; reason=%d\n", event->disconnect.reason);
        notify_state = false;
        //ble_tx_audio_stop();
        conn_handle = BLE_HS_CONN_HANDLE_NONE;
        //Connection terminated; resume advertising 
        ble_audio_advertise();
        break;

    case BLE_GAP_EVENT_ADV_COMPLETE:
        MODLOG_DFLT(INFO, "adv complete\n");
        ble_audio_advertise();
        break;*/

    case BLE_GAP_EVENT_SUBSCRIBE:
        MODLOG_DFLT(INFO, "subscribe event; cur_notify=%d\n value handle; "
                    "val_handle=%d\n",
                    event->subscribe.cur_notify, hrs_hrm_handle);
        notify_state = event->subscribe.cur_notify;
        ESP_LOGI(TAG, "conn_handle from subscribe=%d", conn_handle);
        break;

    case BLE_GAP_EVENT_MTU:
        MODLOG_DFLT(INFO, "mtu update event; conn_handle=%d mtu=%d\n",
                    event->mtu.conn_handle,
                    event->mtu.value);
        break;
    }
    return 0;
}

//version of ble_audio_advertise solving legacy advertising issues
void ble_audio_advertise(void) {
    int rc;

    // Configure extended advertising instance 0 for audio (connectable)
    struct ble_gap_ext_adv_params ext_params = {
        .connectable        = 1,
        .scannable          = 0,
        .directed           = 0,
        .high_duty_directed = 0,
        .legacy_pdu         = 0,
        .anonymous          = 0,
        .include_tx_power   = 0,
        .scan_req_notif     = 0,
        .itvl_min           = 160,  // 100ms
        .itvl_max           = 160,
        .channel_map        = 0,
        .own_addr_type      = BLE_OWN_ADDR_PUBLIC,
        .filter_policy      = 0,
        .primary_phy        = BLE_HCI_LE_PHY_1M,
        .secondary_phy      = BLE_HCI_LE_PHY_1M,
        .tx_power           = 0,
        .sid                = 0,
    };

    rc = ble_gap_ext_adv_configure(0, &ext_params, NULL,
                                   bleaudio_gap_event, NULL);
    if (rc != 0) {
        MODLOG_DFLT(ERROR, "Failed to configure audio ext adv: %d\n", rc);
        return;
    }

    // Set advertising data (device name)
    struct os_mbuf *data = os_msys_get_pkthdr(0, 0);
    if (data == NULL) {
        MODLOG_DFLT(ERROR, "Failed to get mbuf for adv data\n");
        return;
    }

    // Build advertising data manually
    const char *name = ble_svc_gap_device_name();
    uint8_t adv_data[31];
    uint8_t adv_len = 0;

    // Flags
    adv_data[adv_len++] = 2;               // length
    adv_data[adv_len++] = 0x01;            // type: flags
    adv_data[adv_len++] = 0x06;            // LE General Discoverable, BR/EDR not supported

    // Name
    uint8_t name_len = strlen(name);
    adv_data[adv_len++] = name_len + 1;    // length
    adv_data[adv_len++] = 0x09;            // type: complete local name
    memcpy(&adv_data[adv_len], name, name_len);
    adv_len += name_len;

    os_mbuf_append(data, adv_data, adv_len);

    rc = ble_gap_ext_adv_set_data(0, data);
    if (rc != 0) {
        MODLOG_DFLT(ERROR, "Failed to set audio adv data: %d\n", rc);
        return;
    }

    // Start extended advertising indefinitely
    rc = ble_gap_ext_adv_start(0, 0, 0);
    if (rc != 0) {
        MODLOG_DFLT(ERROR, "Failed to start audio ext adv: %d\n", rc);
        return;
    }

    MODLOG_DFLT(INFO, "Audio BLE advertising started\n");
}
//original code
/*void ble_audio_advertise(void){
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
    ESP_LOGI("BLE", "adv_set_fields rc=%d", rc);  // add this
    if (rc != 0) {
        MODLOG_DFLT(ERROR, "error setting advertisement data; rc=%d\n", rc);
        return;
    }

    //GAP - device connectivity definition
    struct ble_gap_adv_params adv_params;
    memset(&adv_params, 0, sizeof(adv_params));
    adv_params.conn_mode = BLE_GAP_CONN_MODE_UND; //undirected-connectable
    adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN; //general-discoverable

    rc = ble_gap_adv_start(ble_addr_type, NULL, BLE_HS_FOREVER, &adv_params, bleaudio_gap_event, NULL);
    ESP_LOGI("BLE", "adv_start rc=%d", rc);  // add this
    if (rc != 0) {
        MODLOG_DFLT(ERROR, "error enabling advertisement; rc=%d\n", rc);
        return;
    }
}*/

void ble_app_on_sync(void) {
    int rc = ble_hs_id_infer_auto(0, &ble_addr_type); //determines the best address type automatically
    assert(rc == 0);

    uint8_t addr_val[6] = {0};
    rc = ble_hs_id_copy_addr(ble_addr_type, addr_val, NULL);

    MODLOG_DFLT(INFO, "Device Address: %02x:%02x:%02x:%02x:%02x:%02x\n", addr_val[5], addr_val[4], addr_val[3],
                addr_val[2], addr_val[1], addr_val[0]);
    //print_addr(addr_val);
    //MODLOG_DFLT(INFO, "\n");
    aoa_ble_init(); //cte transmission through advertisement
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
    ble_svc_gap_device_name_set("BLE-Microphone");
    //initialize the generic access profile (GAP)
    ble_svc_gap_init();
    //initialize the generic attribute profile (GATT) (see ble_transmission.h)
    ble_svc_gatt_init();
    //
    ble_gatts_count_cfg(gatt_svcs);
    //initialize the 
    ble_gatts_add_svcs(gatt_svcs);

    ble_hs_cfg.sync_cb = ble_app_on_sync;
    //ble_hs_cfg.reset_cb = ble_app_on_reset;
}

void ble_host_task(void *args) {
    nimble_port_run();
    nimble_port_freertos_deinit();
}