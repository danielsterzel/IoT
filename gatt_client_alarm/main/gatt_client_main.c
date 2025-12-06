/* gatt_client_main.c
   GATT client: finds device by name "ESP32_ALARM", connects, discovers service 0x00FF,
   finds char UUID 0xFF02 and registers for notify. On notify -> prints and publishes via MQTT stub.
*/

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_bt.h"
#include "esp_gap_ble_api.h"
#include "esp_gattc_api.h"
#include "esp_bt_main.h"
#include "esp_gatt_common_api.h"

static const char *TAG = "GATT_CLIENT_FULL";
#define TARGET_NAME "ESP32_ALARM"
#define SERVICE_UUID 0x00FF
#define CHAR_SENSOR_UUID 0xFF02

static esp_gatt_if_t gattc_if_global = 0;
static uint16_t conn_id_global = 0;
static esp_bd_addr_t server_bda;
static uint16_t sensor_char_handle = 0;
static bool is_connected = false;

/* MQTT stub - replace with call to mqtt_publish() from component if available */
void mqtt_publish_stub(const char* topic, const char* payload) {
    ESP_LOGI("MQTT", "PUB %s => %s", topic, payload);
}

/* GAP callback */
static void gap_cb(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param) {
    switch (event) {
        case ESP_GAP_BLE_SCAN_PARAM_SET_COMPLETE_EVT:
            esp_ble_gap_start_scanning(30);
            break;
        case ESP_GAP_BLE_SCAN_RESULT_EVT: {
            if (param->scan_rst.search_evt == ESP_GAP_SEARCH_INQ_RES_EVT) {
                uint8_t *adv_name = esp_ble_resolve_adv_data(param->scan_rst.ble_adv, ESP_BLE_AD_TYPE_NAME_CMPL, NULL);
                if (adv_name && strcmp((char*)adv_name, TARGET_NAME) == 0) {
                    ESP_LOGI(TAG, "Found target %s, connecting...", TARGET_NAME);
                    memcpy(server_bda, param->scan_rst.bda, sizeof(esp_bd_addr_t));
                    esp_ble_gap_stop_scanning();
                    esp_ble_gattc_open(gattc_if_global, server_bda, true);
                }
            }
            break;
        }
        default:
            break;
    }
}

/* GATTC callback */
static void gattc_cb(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if,
                     esp_ble_gattc_cb_param_t *param) {
    switch (event) {
        case ESP_GATTC_REG_EVT:
            ESP_LOGI(TAG, "GATTC_REG_EVT");
            gattc_if_global = gattc_if;
            esp_ble_gap_set_scan_params(&(esp_ble_scan_params_t){
                .scan_type = BLE_SCAN_TYPE_ACTIVE,
                .own_addr_type = BLE_ADDR_TYPE_PUBLIC,
                .scan_filter_policy = BLE_SCAN_FILTER_ALLOW_ALL,
                .scan_interval = 0x50,
                .scan_window = 0x30,
            });
            break;

        case ESP_GATTC_CONNECT_EVT:
            ESP_LOGI(TAG, "CONNECT event");
            conn_id_global = param->connect.conn_id;
            is_connected = true;
            // Start service discovery
            esp_ble_gattc_search_service(gattc_if_global, conn_id_global, NULL);
            break;

        case ESP_GATTC_DISCONNECT_EVT:
            ESP_LOGW(TAG, "DISCONNECTED");
            is_connected = false;
            sensor_char_handle = 0;
            // start scanning again
            esp_ble_gap_start_scanning(30);
            break;

        case ESP_GATTC_SEARCH_RES_EVT: {
            esp_gatt_srvc_id_t *srvc_id = &param->search_res.srvc_id;
            if (srvc_id->id.uuid.len == ESP_UUID_LEN_16 &&
                srvc_id->id.uuid.uuid.uuid16 == SERVICE_UUID) {
                ESP_LOGI(TAG, "Found service 0x00FF");
            }
            break;
        }

        case ESP_GATTC_SEARCH_CMPL_EVT:
            ESP_LOGI(TAG, "SEARCH complete, requesting chars...");
            // enumerate characteristics in service - pass NULL to get first then call repeatedly in real code
            esp_ble_gattc_get_characteristic(gattc_if_global, conn_id_global, 0, NULL);
            break;

        case ESP_GATTC_GET_CHAR_EVT: {
            esp_gatt_id_t *char_id = &param->get_char.char_id;
            if (char_id->uuid.len == ESP_UUID_LEN_16 &&
                char_id->uuid.uuid.uuid16 == CHAR_SENSOR_UUID) {
                sensor_char_handle = param->get_char.char_handle;
                ESP_LOGI(TAG, "Sensor char found handle=%d", sensor_char_handle);
                // register for notify on that handle
                esp_ble_gattc_register_for_notify(gattc_if_global, server_bda, sensor_char_handle);
            } else {
                // request next characteristic (simplified approach)
                // NOTE: proper code queries by handle ranges; this skeleton will work with simple example
            }
            break;
        }

        case ESP_GATTC_REG_FOR_NOTIFY_EVT: {
            ESP_LOGI(TAG, "Registered for notify result");
            // need to write descriptor to enable notifications (Client Characteristic Configuration)
            // For simplicity we attempt to write 0x0001 to descriptor
            uint16_t ccc_val = 0x0001;
            esp_ble_gattc_write_char_descr(
                gattc_if_global,
                conn_id_global,
                param->reg_for_notify.handle + 1, // descriptor handle guess — in full implementation discover descriptors
                sizeof(ccc_val),
                (uint8_t*)&ccc_val,
                ESP_GATT_WRITE_TYPE_RSP,
                ESP_GATT_AUTH_REQ_NONE
            );
            break;
        }

        case ESP_GATTC_NOTIFY_EVT: {
            char buf[128] = {0};
            int len = param->notify.value_len;
            if (len > 0 && len < (int)sizeof(buf)) memcpy(buf, param->notify.value, len);
            ESP_LOGI(TAG, "NOTIFY: %s", buf);
            mqtt_publish_stub("smartsec/user123/esp32_alarm/door_01/sensor", buf);
            break;
        }

        default:
            break;
    }
}

void app_main(void) {
    nvs_flash_init();
    esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT);
    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    esp_bt_controller_init(&bt_cfg);
    esp_bt_controller_enable(ESP_BT_MODE_BLE);
    esp_bluedroid_init();
    esp_bluedroid_enable();

    esp_ble_gattc_register_callback(gattc_cb);
    esp_ble_gap_register_callback(gap_cb);
    esp_ble_gattc_app_register(0);

    ESP_LOGI(TAG, "GATT client ready - scanning for " TARGET_NAME);
    // main loop just idle: BLE callbacks do the job
    while (1) vTaskDelay(pdMS_TO_TICKS(10000));
}
