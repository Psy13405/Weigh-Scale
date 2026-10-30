#pragma once
#include "esp_err.h"

// Initialize NimBLE and start advertising as WEIGH_SCALE
esp_err_t bluetooth_init(void);
// Update the value the phone sees (read) and send a notification if connected
void ble_set_weight(float grams);