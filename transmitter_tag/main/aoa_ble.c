#include "aoa_ble.h"

// Periodic advertising with CTE (connectionless AoA transmitter)

static uint8_t aoa_adv_handle = BLE_HCI_LE_SET_EXT_ADV_HANDLE_MAX;

static void aoa_start_periodic_adv(void) {
    int rc;

    // Step 1 — Set up extended advertising instance (required for periodic adv)
    struct ble_gap_ext_adv_params ext_params = {
        .connectable    = 0,
        .scannable      = 0,
        .legacy_pdu     = 0,        // must be 0 for periodic adv
        .anonymous      = 0,
        .include_tx_power = 0,
        .primary_phy    = BLE_HCI_LE_PHY_1M,
        .secondary_phy  = BLE_HCI_LE_PHY_1M,
        .sid            = 0,        // advertising set ID
        .itvl_min       = 1600,      // 1 s (units of 0.625ms)
        .itvl_max       = 1600,
        .own_addr_type  = BLE_OWN_ADDR_PUBLIC,
    };

    rc = ble_gap_ext_adv_configure(0, &ext_params, NULL, NULL, NULL);
    if (rc != 0) {
        ESP_LOGE("AOA", "Failed to configure ext adv: %d", rc);
        return;
    }
    aoa_adv_handle = 0;

    // Step 2 — Set up periodic advertising parameters on the same handle
    struct ble_gap_periodic_adv_params periodic_params = {
        .itvl_min           = 800,  // 1 s (units of 1.25ms)
        .itvl_max           = 800,
        .include_tx_power   = 0,
    };

    rc = ble_gap_periodic_adv_configure(aoa_adv_handle, &periodic_params);
    if (rc != 0) {
        ESP_LOGE("AOA", "Failed to configure periodic adv: %d", rc);
        return;
    }

    // Step 3 — Set CTE parameters on the periodic advertising
    struct ble_gap_periodic_adv_cte_params cte_params = {
        .cte_len            = CTE_LENGTH,
        .cte_type           = BLE_HCI_LE_CTE_TYPE_AOA,  // AoA, no antenna switching on TX
        .cte_count          = CTE_COUNT,
        .switch_pattern_len = 0,
        .switch_pattern     = NULL,
    };

    rc = ble_gap_periodic_adv_set_cte_params(aoa_adv_handle, &cte_params);
    if (rc != 0) {
        ESP_LOGE("AOA", "Failed to set CTE params: %d", rc);
        return;
    }

    // Step 4 — Start periodic advertising
    rc = ble_gap_periodic_adv_start(aoa_adv_handle);
    if (rc != 0) {
        ESP_LOGE("AOA", "Failed to start periodic adv: %d", rc);
        return;
    }

    // Step 5 — Start extended advertising so the periodic adv is discoverable
    rc = ble_gap_ext_adv_start(aoa_adv_handle, 0, 0);
    if (rc != 0) {
        ESP_LOGE("AOA", "Failed to start ext adv: %d", rc);
        return;
    }

    ESP_LOGI("AOA", "Connectionless CTE advertising started");
}

void aoa_ble_init(void) {
    ESP_LOGI("AOA", "AoA BLE init");
    aoa_start_periodic_adv();
}