#include "load_cell.h"
#include "driver/gpio.h"
#include "esp_rom_sys.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define HX711_DOUT_PIN   3
#define HX711_SCK_PIN    2
#define CAL_FACTOR       105.62f   // counts per gram
#define READY_TIMEOUT_MS 1000
#define AVG_SAMPLES      4

static int32_t s_offset = 0;
static portMUX_TYPE s_mux = portMUX_INITIALIZER_UNLOCKED;

// Shift in a single bit from HX711
static inline uint8_t hx711_shift_in(void) {
    gpio_set_level(HX711_SCK_PIN, 1);
    esp_rom_delay_us(1);
    uint8_t bit = gpio_get_level(HX711_DOUT_PIN);
    gpio_set_level(HX711_SCK_PIN, 0);
    esp_rom_delay_us(1);
    return bit;
}

// Read a single value from HX711, blocking until ready or timeout
static esp_err_t hx711_read(int32_t *out) {
    // Wait for ready (DOUT low), with timeout
    TickType_t start = xTaskGetTickCount();
    while (gpio_get_level(HX711_DOUT_PIN) == 1) {
        if ((xTaskGetTickCount() - start) > pdMS_TO_TICKS(READY_TIMEOUT_MS)) {
            return ESP_ERR_TIMEOUT;
        }
        vTaskDelay(1);
    }

    uint32_t count = 0;

    // Keep SCK from staying high >60 us if an interrupt fires
    portENTER_CRITICAL(&s_mux);
    for (int i = 0; i < 24; i++) {
        count = (count << 1) | hx711_shift_in();
    }
    hx711_shift_in();   // 25th pulse: gain 128, channel A for next reading
    portEXIT_CRITICAL(&s_mux);

    if (count & 0x800000) {
        count |= 0xFF000000;   // sign-extend 24 -> 32 bits
    }
    
    *out = (int32_t)count;
    
    return ESP_OK;
}

// Read multiple samples and return the average
static esp_err_t hx711_read_avg(int samples, int32_t *out) {
    int64_t sum = 0;
    for (int i = 0; i < samples; i++) {
        int32_t v;
        esp_err_t err = hx711_read(&v);
        if (err != ESP_OK) return err;
        sum += v;
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    *out = (int32_t)(sum / samples);
    return ESP_OK;
}

// Initialize the GPIO pins for HX711
esp_err_t load_cell_init(void) {
    gpio_config_t io = {
        .pin_bit_mask = (1ULL << HX711_SCK_PIN),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    esp_err_t err = gpio_config(&io);
    if (err != ESP_OK) return err;

    io.pin_bit_mask = (1ULL << HX711_DOUT_PIN);
    io.mode = GPIO_MODE_INPUT;
    err = gpio_config(&io);
    if (err != ESP_OK) return err;

    gpio_set_level(HX711_SCK_PIN, 0);
    return ESP_OK;
}

// Tare the scale by reading the current value and storing it as an offset
esp_err_t load_cell_tare(void) {
    return hx711_read_avg(AVG_SAMPLES, &s_offset);
}

// Read the weight in grams, applying the offset and calibration factor
esp_err_t load_cell_read_grams(float *out_g) {
    int32_t raw;
    esp_err_t err = hx711_read_avg(AVG_SAMPLES, &raw);
    if (err != ESP_OK) return err;

    *out_g = (raw - s_offset) / CAL_FACTOR;
    if (*out_g < 0) *out_g = 0;
    return ESP_OK;
}