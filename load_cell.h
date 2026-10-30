#pragma once
#include "esp_err.h"

esp_err_t load_cell_init(void);                 // GPIO setup
esp_err_t load_cell_tare(void);                 // zero the scale (remove weight first)
esp_err_t load_cell_read_grams(float *out_g);   // averaged reading in grams