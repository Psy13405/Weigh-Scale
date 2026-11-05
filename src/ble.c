#include "ble.h"
#include "string.h"
#include "stdbool.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "nvs_flash.h"
#include "esp_mac.h"
#include "math.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "host/util/util.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"


// Bluetooth Configuration
#define BLE_DEVICE_NAME_MAX_LEN     32
#define DEFAULT_DEVICE_NAME         "WEIGH_SCALE"

// UUIDs
static const ble_uuid16_t gatt_svc_uuid         = BLE_UUID16_INIT(0xFFE0);
static const ble_uuid16_t gatt_char_weight_uuid = BLE_UUID16_INIT(0xFFE2);

// BLE state
static char device_name[BLE_DEVICE_NAME_MAX_LEN] = DEFAULT_DEVICE_NAME;

static const char *BLE_TAG = "BLE";
static volatile uint16_t weight_char_handle = 0;
static volatile float latest_weight = 0;

// Forward declarations
static void ble_app_advertise(void);
static int gatt_char_access(uint16_t conn_handle, uint16_t attr_handle,
                             struct ble_gatt_access_ctxt *ctxt, void *arg);


// ─── BLE Notify Helpers ───────────────────────────────────────────────────────
static int32_t weight_to_int32(float grams) {
    return (int32_t)lroundf(grams * 10.0f);
}

void ble_set_weight(float grams)
{
    latest_weight = grams;
    if (weight_char_handle != 0) {
        ble_gatts_chr_updated(weight_char_handle);
    }
}

// ─── GATT Service Definition ─────────────────────────────────────────────────
static const struct ble_gatt_svc_def gatt_svcs[] = {
    {
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = &gatt_svc_uuid.u,
        .characteristics = (struct ble_gatt_chr_def[]) {
            {
                .uuid = &gatt_char_weight_uuid.u,
                .access_cb = gatt_char_access,
                .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_NOTIFY,
                .val_handle = (uint16_t *)&weight_char_handle,
            },
            { 0 }
        },
    },
    { 0 }
};

static int gatt_char_access(uint16_t conn_handle, uint16_t attr_handle,
                              struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    if (ctxt->op != BLE_GATT_ACCESS_OP_READ_CHR) return 0;

//    char buf[16];
    if (attr_handle == weight_char_handle) {
        int32_t weight_int = weight_to_int32(latest_weight);
        os_mbuf_append(ctxt->om, &weight_int, sizeof(weight_int));
    
        // int len = snprintf(buf, sizeof(buf), "%.1f", latest_weight_g);
        // os_mbuf_append(ctxt->om, buf, len);
    }
    return 0;
}

// ─── BLE (NimBLE) ────────────────────────────────────────────────────────────
static int gap_event_handler(struct ble_gap_event *event, void *arg)
{
    switch (event->type) {
        case BLE_GAP_EVENT_CONNECT:
            if (event->connect.status == 0) {
                ESP_LOGI(BLE_TAG, "Device connected");
            } else {
                ble_app_advertise();
            }
            break;
        
        case BLE_GAP_EVENT_SUBSCRIBE:
            if (event->subscribe.cur_notify) {
                ESP_LOGI(BLE_TAG, "Device subscribed to weight notifications");
                ble_gatts_chr_updated(weight_char_handle);
            }
            break;

        case BLE_GAP_EVENT_DISCONNECT:
            ESP_LOGI(BLE_TAG, "Device disconnected, reason: %d", event->disconnect.reason);
            ble_app_advertise();
            break;

        default:
            break;
    }
    return 0;
}

static void ble_app_advertise(void)
{
    struct ble_hs_adv_fields fields = {0};
    fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;
    fields.name = (uint8_t *)device_name;
    fields.name_len = strlen(device_name);
    fields.name_is_complete = 1;

    int rc = ble_gap_adv_set_fields(&fields);
    if (rc != 0) {
        ESP_LOGE(BLE_TAG, "Failed to set adv fields: %d", rc);
        return;
    }

    struct ble_gap_adv_params adv_params = {0};
    adv_params.conn_mode = BLE_GAP_CONN_MODE_UND;
    adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN;

    rc = ble_gap_adv_start(BLE_OWN_ADDR_PUBLIC, NULL, BLE_HS_FOREVER,
                           &adv_params, gap_event_handler, NULL);
    if (rc != 0) {
        ESP_LOGE(BLE_TAG, "Failed to start advertising: %d", rc);
    }
}

// ─── BLE Initialization ─────────────────────────────────────────────────────
static void ble_app_on_sync(void)
{
    ble_hs_util_ensure_addr(0);
    ble_app_advertise();
    ESP_LOGI(BLE_TAG, "BLE advertising started: %s", device_name);
}

static void nimble_host_task(void *param)
{
    nimble_port_run();
    nimble_port_freertos_deinit();
}

esp_err_t bluetooth_init(void)
{
    esp_log_level_set("NimBLE", ESP_LOG_WARN);
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    nimble_port_init();
    ble_svc_gap_init();
    ble_svc_gatt_init();
    ble_gatts_count_cfg(gatt_svcs);
    ble_gatts_add_svcs(gatt_svcs);
    ble_svc_gap_device_name_set(device_name);
    ble_hs_cfg.sync_cb = ble_app_on_sync;
    nimble_port_freertos_init(nimble_host_task);

    ESP_LOGI(BLE_TAG, "NimBLE initialized, device name: %s", device_name);
    return ESP_OK;
}