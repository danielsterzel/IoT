/* mqtt_client_app.c
   Minimalny wrapper dla esp-mqtt. Użyj start_mqtt/stop_mqtt/mqtt_publish
*/
#include "esp_log.h"
#include "mqtt_client.h"
#include <string.h>

static const char *TAG = "MQTT_APP";
static esp_mqtt_client_handle_t client = NULL;

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data) {
    esp_mqtt_event_handle_t event = event_data;
    switch (event->event_id) {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
            break;
        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGW(TAG, "MQTT_EVENT_DISCONNECTED");
            break;
        case MQTT_EVENT_DATA:
            ESP_LOGI(TAG, "MQTT_EVENT_DATA TOPIC: %.*s DATA: %.*s",
                     event->topic_len, event->topic, event->data_len, event->data);
            break;
        default:
            break;
    }
}

void start_mqtt(const char* uri, const char* client_id) {
    if (client) return;
    esp_mqtt_client_config_t cfg = {
        .uri = uri,
        .client_id = client_id
    };
    client = esp_mqtt_client_init(&cfg);
    esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    esp_mqtt_client_start(client);
    ESP_LOGI(TAG, "MQTT started -> %s", uri);
}

void stop_mqtt(void) {
    if (!client) return;
    esp_mqtt_client_stop(client);
    esp_mqtt_client_destroy(client);
    client = NULL;
    ESP_LOGI(TAG, "MQTT stopped");
}

int mqtt_publish(const char* topic, const char* payload) {
    if (!client) return -1;
    int id = esp_mqtt_client_publish(client, topic, payload, 0, 1, 0);
    ESP_LOGI(TAG,"Published id=%d topic=%s payload=%s", id, topic, payload);
    return id;
}
