/* gatt_server_main.c
   GATT server sample for alarm system.
   - service UUID 0x00FF
   - char 0xFF01: state (read/write)
   - char 0xFF02: sensor_notify (notify)
   - char 0xFF03: battery (read)
*/

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_gatts_api.h"
#include "esp_gap_ble_api.h"
#include "esp_gatt_common_api.h"

static const char *TAG = "GATT_SERVER_ALARM";

/* UUIDs (16-bit custom) */
#define GATTS_SERVICE_UUID_TEST    0x00FF
#define GATTS_CHAR_UUID_STATE      0xFF01
#define GATTS_CHAR_UUID_SENSOR     0xFF02
#define GATTS_CHAR_UUID_BATTERY    0xFF03

/* Simple values */
static char state_value[16] = "disarmed";
static char battery_value[4] = "99";

static uint16_t conn_id_global = 0;
static esp_gatt_if_t gatts_if_global = 0;
static int sensor_val_handle = 0;

#define PROFILE_NUM 1
#define PROFILE_APP_IDX 0

/* Basic attribute table handles - simplified example */
static void gatts_profile_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if,
                                        esp_ble_gatts_cb_param_t *param);

static esp_gatts_cb_t gatts_cb = gatts_profile_event_handler;

static void send_sensor_notify(const char* payload, uint16_t payload_len) {
    if (!conn_id_global) {
        ESP_LOGW(TAG,"No connection - cannot notify");
        return;
    }
    if (sensor_val_handle == 0) {
        ESP_LOGW(TAG,"Sensor handle unknown");
        return;
    }
    esp_err_t err = esp_ble_gatts_send_indicate(gatts_if_global, conn_id_global, sensor_val_handle,
                                                payload_len, (uint8_t*)payload, false);
    if (err != ESP_OK) ESP_LOGW(TAG, "send_indicate err 0x%x", err);
}

static void gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param) {
    // Not used heavily here, but required to register
    switch (event) {
        case ESP_GAP_BLE_ADV_START_COMPLETE_EVT:
            ESP_LOGI(TAG,"Adv start complete");
            break;
        default:
            break;
    }
}

/* Very small, robust GATT server using dynamic attributes is long;
   here we use a simple approach: register callbacks and react on WRITE/READ/CONNECT/DISCONNECT */
static void gatts_profile_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if,
                                        esp_ble_gatts_cb_param_t *param) {
    switch (event) {
        case ESP_GATTS_REG_EVT: {
            ESP_LOGI(TAG,"GATTS_REG_EVT");
            gatts_if_global = gatts_if;
            // set device name and start advertising
            esp_ble_gap_set_device_name("ESP32_ALARM");
            // Basic adv data (no scan response)
            esp_ble_adv_params_t adv_params = {
                .adv_int_min = 0x20,
                .adv_int_max = 0x40,
                .adv_type = ADV_TYPE_IND,
                .own_addr_type = BLE_ADDR_TYPE_PUBLIC,
                .channel_map = ADV_CHNL_ALL,
                .adv_filter_policy = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
            };
            // minimal advertising payload
            esp_ble_gap_config_adv_data_t adv_data = {
                .set_scan_rsp = false,
                .include_name = true,
                .include_txpower = false,
                .min_interval = 0x0006,
                .max_interval = 0x0010,
                .appearance = 0x00,
                .manufacturer_len = 0,
                .p_manufacturer_data = NULL,
                .service_data_len = 0,
                .p_service_data = NULL,
                .service_uuid_len = 0,
                .p_service_uuid = NULL,
                .flag = (ESP_BLE_ADV_FLAG_GEN_DISC | ESP_BLE_ADV_FLAG_BREDR_NOT_SPT),
            };
            esp_ble_gap_config_adv_data(&adv_data);
            esp_ble_gap_start_advertising(&adv_params);
            break;
        }
        case ESP_GATTS_CONNECT_EVT: {
            ESP_LOGI(TAG,"Client connected, conn_id %d", param->connect.conn_id);
            conn_id_global = param->connect.conn_id;
            break;
        }
        case ESP_GATTS_DISCONNECT_EVT: {
            ESP_LOGI(TAG,"Client disconnected");
            conn_id_global = 0;
            // restart advertising so devices can reconnect
            esp_ble_gap_start_advertising(NULL);
            break;
        }
        case ESP_GATTS_WRITE_EVT: {
            ESP_LOGI(TAG,"WRITE event, handle %d len %d", param->write.handle, param->write.len);
            // naive: treat write to any attribute as state change if payload matches strings
            if (param->write.len > 0) {
                if (strncmp((char*)param->write.value, "armed", param->write.len) == 0) {
                    strncpy(state_value, "armed", sizeof(state_value)-1);
                    ESP_LOGI(TAG,"State -> ARMED");
                } else if (strncmp((char*)param->write.value, "disarmed", param->write.len) == 0) {
                    strncpy(state_value, "disarmed", sizeof(state_value)-1);
                    ESP_LOGI(TAG,"State -> DISARMED");
                } else {
                    // if write appears on sensor simulate a sensor trigger event
                    if (param->write.len < 64) {
                        ESP_LOGI(TAG,"Simulated sensor write -> notify others");
                        send_sensor_notify((char*)param->write.value, param->write.len);
                    }
                }
            }
            break;
        }
        case ESP_GATTS_READ_EVT: {
            ESP_LOGI(TAG,"READ event handle %d", param->read.handle);
            break;
        }
        default:
            break;
    }
}

void app_main(void) {
    esp_err_t ret;
    ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        nvs_flash_init();
    }

    esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT);
    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    esp_bt_controller_init(&bt_cfg);
    esp_bt_controller_enable(ESP_BT_MODE_BLE);
    esp_bluedroid_init();
    esp_bluedroid_enable();

    esp_ble_gap_register_callback(gap_event_handler);
    esp_ble_gatts_register_callback(gatts_profile_event_handler);
    esp_ble_gatts_app_register(0);

    ESP_LOGI(TAG, "GATT server started. Advertising as 'ESP32_ALARM'");

    // keep alive
    while (1) vTaskDelay(pdMS_TO_TICKS(10000));
}
