#include "aoa_ble.h"
#include "host/ble_hs_adv.h"
#include "host/ble_gap.h"

#define AOA_ADV_INSTANCE 1

// BLE_CTE_TYPE_AOA = 0 per cte_config.h
#ifndef BLE_CTE_TYPE_AOA
#define BLE_CTE_TYPE_AOA 0
#endif

// Eddystone UID frame matching the working example exactly
static uint8_t eddystone_uid[] = {
    0x02, 0x01, 0x06,           // Flags
    0x03, 0x03, 0xAA, 0xFE,     // 16-bit UUID list
    0x17, 0x16, 0xAA, 0xFE,     // Service data header
    0x00,                        // Frame type: UID
    0x00,                        // TX power
    // Namespace — matches ANT-B11 default filter
    0x4E, 0x49, 0x4E, 0x41, 0x2D, 0x42, 0x34, 0x54, 0x41, 0x47,
    // Instance ID
    0x00, 0x00, 0x00, 0x00, 0x00, 0x01,
    // RFU
    0x00, 0x00
};

static uint8_t s_periodic_adv_raw_data[] = {
    0x0D, BLE_HS_ADV_TYPE_COMP_NAME,
    'C','T','E',' ','T','a','g'
};

static void aoa_start_periodic_adv(void) {
    int rc;
    ble_addr_t addr;

    // Step 1 — Configure extended advertising
    struct ble_gap_ext_adv_params ext_params = {
        .own_addr_type = BLE_OWN_ADDR_RANDOM,
        .primary_phy   = BLE_HCI_LE_PHY_1M,
        .secondary_phy = BLE_HCI_LE_PHY_1M,
        .sid           = 2,
        .tx_power      = 0,
    };
    rc = ble_gap_ext_adv_configure(AOA_ADV_INSTANCE, &ext_params, NULL, NULL, NULL);
    assert(rc == 0);

    rc = ble_hs_id_gen_rnd(1, &addr);
    assert(rc == 0);
    rc = ble_gap_ext_adv_set_addr(AOA_ADV_INSTANCE, &addr);
    assert(rc == 0);

    // Step 2 — Set extended advertising data (Eddystone UID)
    struct os_mbuf *ext_data = os_msys_get_pkthdr(sizeof(eddystone_uid), 0);
    assert(ext_data);
    rc = os_mbuf_append(ext_data, eddystone_uid, sizeof(eddystone_uid));
    assert(rc == 0);
    rc = ble_gap_ext_adv_set_data(AOA_ADV_INSTANCE, ext_data);
    assert(rc == 0);

    // Step 3 — Configure periodic advertising
    struct ble_gap_periodic_adv_params periodic_params = {
        .include_tx_power = 0,
        .itvl_min         = BLE_GAP_ADV_ITVL_MS(750),
        .itvl_max         = BLE_GAP_ADV_ITVL_MS(1000),
    };
    rc = ble_gap_periodic_adv_configure(AOA_ADV_INSTANCE, &periodic_params);
    assert(rc == 0);

    // Step 4 — Set periodic advertising data
    struct os_mbuf *data = os_msys_get_pkthdr(sizeof(s_periodic_adv_raw_data), 0);
    assert(data);
    rc = os_mbuf_append(data, s_periodic_adv_raw_data, sizeof(s_periodic_adv_raw_data));
    assert(rc == 0);
    rc = ble_gap_periodic_adv_set_data(AOA_ADV_INSTANCE, data);
    assert(rc == 0);

    // Step 5 — Set CTE parameters (no antenna_ids for AOA)
    struct ble_gap_periodic_adv_cte_params cte_params = {
        .cte_length               = CTE_LENGTH,
        .cte_type                 = BLE_CTE_TYPE_AOA,
        .cte_count                = CTE_COUNT,
        .switching_pattern_length = 0,
        .antenna_ids              = NULL,
    };
    rc = ble_gap_set_connless_cte_transmit_params(AOA_ADV_INSTANCE, &cte_params);
    assert(rc == 0);

    // Step 6 — Start periodic advertising
    rc = ble_gap_periodic_adv_start(AOA_ADV_INSTANCE);
    assert(rc == 0);

    // Step 7 — Start extended advertising
    rc = ble_gap_ext_adv_start(AOA_ADV_INSTANCE, 0, 0);
    assert(rc == 0);

    // Step 8 — Enable CTE last
    rc = ble_gap_set_connless_cte_transmit_enable(AOA_ADV_INSTANCE, 1);
    if (rc != 0) {
        ESP_LOGE("AOA", "CTE enable FAILED: rc=%d", rc);
    } else {
        ESP_LOGI("AOA", "CTE transmit enabled successfully");
    }

    ESP_LOGI("AOA", "Connectionless CTE advertising started");
}

void aoa_ble_init(void) {
    ESP_LOGI("AOA", "AoA BLE init");
    aoa_start_periodic_adv();
}