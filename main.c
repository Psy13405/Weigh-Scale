#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_check.h"
#include "esp_rom_sys.h"
#include "driver/gpio.h"
#include "load_cell.h"
#include "ble.h"

// State machine to change weight units
typedef enum {
    METRIC,
    IMPERIAL
} State_t;

// Global variables for handling IO
static const char *TAG = "HX711";
static volatile State_t current_state = METRIC;
static TaskHandle_t button_task_handle = NULL;

// Button task to handle mode switching
static void button_task(void *arg)
{
    while (true) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        current_state = (current_state == METRIC) ? IMPERIAL : METRIC;
        ESP_LOGI(TAG, "Button mode: %s", current_state == METRIC ? "METRIC" : "IMPERIAL");
    }
}

// Button ISR handler to notify the button task
static void button_isr_handler(void *arg)
{
    static TickType_t last_press = 0;
    const TickType_t now = xTaskGetTickCountFromISR();

    if ((now - last_press) < pdMS_TO_TICKS(200)) {
        return;
    }
    last_press = now;

    BaseType_t higher_priority_task_woken = pdFALSE;
    vTaskNotifyGiveFromISR(button_task_handle, &higher_priority_task_woken);

    if (higher_priority_task_woken) {
        portYIELD_FROM_ISR();
    }
}

// Initialize the button GPIO and ISR
esp_err_t button_init(void)
{
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << GPIO_NUM_4),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_NEGEDGE,
    };

    ESP_RETURN_ON_ERROR(gpio_config(&io_conf), TAG, "Button GPIO setup failed");
    ESP_RETURN_ON_ERROR(gpio_install_isr_service(0), TAG, "ISR service failed");
    return gpio_isr_handler_add(GPIO_NUM_4, button_isr_handler, NULL);
}

void app_main(void) {
    // Initialize the load cell and Bluetooth
    ESP_ERROR_CHECK(load_cell_init());
    ESP_LOGI(TAG, "HX711 Load Sensor Test");
    ESP_LOGI(TAG, "Taring... remove any weight");
    ESP_ERROR_CHECK(bluetooth_init());

    vTaskDelay(pdMS_TO_TICKS(2000));

    // Tare the scale to zero
    if (load_cell_tare() != ESP_OK) {
        ESP_LOGE(TAG, "HX711 not responding, check wiring");
        return;
    }
    ESP_LOGI(TAG, "Tare done");

    // Create the button task and initialize the button GPIO
    xTaskCreate(button_task, "button_task", 2048, NULL, 5,
            &button_task_handle);
    ESP_ERROR_CHECK(button_init());
    
    while (1) {
        float g;
        // Read the weight in grams and convert to pounds if in imperial mode
        if (load_cell_read_grams(&g) == ESP_OK) {
            if (current_state == IMPERIAL) {
                float lbs = g * 0.00220462f;
                ESP_LOGI(TAG, "%.2f lbs", lbs);
                ble_set_weight(lbs);
            } else {
                ESP_LOGI(TAG, "%.2f g", g);
            }
            ble_set_weight(g);
        } else {
            ESP_LOGE(TAG, "Read timeout, check wiring");
        }
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}