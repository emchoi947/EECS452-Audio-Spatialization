#ifndef AOA_BLE_H
#define AOA_BLE_H

#include "common.h"
#include "nimble/nimble_port.h"
#include "host/ble_hs.h"
#include "host/ble_gap.h"

// CTE length in 8us units (20 = 160us, valid range 2-20)
#define CTE_LENGTH      20
#define CTE_COUNT       1   // number of CTEs per periodic adv event

void aoa_ble_init(void);

#endif