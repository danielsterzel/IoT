/*  ==================== MQTT requirements ====================
 * 1. First thing we need is connection config (type: esp_mqtt_client_config_t) it gives us a description to which broker we need to connect to:
        includes information such as: broker address, port, client_id, keepalive? 
 * 2. Second thing we need is an event handler:
        - MQTT_EVENT_CONNECTED.
        - MQTT_EVENT_DISCONNECTED.
        - MQTT_EVENT_DATA - a message has been received.
        - MQTT_EVENT_PUBLISHED - a message has been sent.
        - MQTT_EVENT_ERROR.
 * 3. Then we need to initialize a client (esp_mqtt_client_init).
 * 4. Client start (esp_mqtt_client_start) 
 *      - a socket is opened,
 *      - client connects to a broker,
 *      - handshake with MQTT,
 *      - Starts receiving and sending messages.
 * 5. Publishing messages   ->      on topics.
 * 
 * */

#include "mqtt_client.h" // from ESP IDF
#include "anti_theft_mqtt_client.h" // my own header file
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stddef.h>

#define USER_ID  "daniel"
#define DEVICE_ID "device01"

static AntiTheftMqttTopics topicStruct;

static const char* TAG = "[ ANTI-THEFT MQTT ]";

static void mqttEventHandler(void* handlerArguments, esp_event_base_t base, int32_t eventId, void* eventData); // fwd 

static void antiTheftPublishStatus(const char *statusMsg); // fwd

static esp_mqtt_client_handle_t clientHandle = NULL;

void antiTheftMqttStart() {

       antiTheftTopicsInit(&topicStruct, USER_ID, DEVICE_ID);
       ESP_LOGI(TAG, "Initializing connection config . . .");
       
       esp_mqtt_client_config_t mqttConfig = {
              .broker.address.uri = "mqtt://172.20.10.10",
              .broker.address.port = 1883,
              .credentials.client_id = "anti_theft_device_01",
              .session.keepalive = 15
       };

       clientHandle = esp_mqtt_client_init(&mqttConfig);

       if(clientHandle == NULL) {
              ESP_LOGE(TAG, "Failed to create a MQTT client.");
              return;
       }

       esp_mqtt_client_register_event(clientHandle, 
              ESP_EVENT_ANY_ID,
              mqttEventHandler,
              NULL);
       

       ESP_LOGI(TAG, "Starting client.");
       esp_mqtt_client_start(clientHandle);
}


// ====================== definitions ======================
static void mqttEventHandler(
       void* handlerArguments,
       esp_event_base_t base,
       int32_t eventId,
       void* eventData)
{
       esp_mqtt_event_handle_t event = eventData;

       switch(eventId) 
       {
              case MQTT_EVENT_CONNECTED:
                     ESP_LOGI(TAG, " ------------MQTT_EVENT_CONNECTED------------ \n successfull connection to this pc broker");
                     
                     
                     char commandTopic[TOPIC_BUFFER_SIZE];
                     antiTheftBuildTopic(&topicStruct, commandTopic, TOPIC_BUFFER_SIZE, "command", NULL);

                     esp_mqtt_client_subscribe(clientHandle, commandTopic, 1);

                     ESP_LOGI(TAG, "Subscribing to command topic: %s", commandTopic);
                     
                     break;

              case MQTT_EVENT_DISCONNECTED:
                            ESP_LOGE(TAG, "------------ MQTT DISCONNECTED !!! ------------");
                            ESP_LOGI(TAG, ". . . Trying to reconnect . . . ");
                            vTaskDelay(pdMS_TO_TICKS(2000));
                            esp_mqtt_client_reconnect(clientHandle);
                            break;
              case MQTT_EVENT_ERROR:
                            ESP_LOGE(TAG, "------------ MQTT ERROR ------------");
                            break;
              case MQTT_EVENT_DATA:
                            ESP_LOGI(TAG, " ------------ MQTT DATA ------------");
                            
                            char cmdTopic[TOPIC_BUFFER_SIZE]; // we dont use malloc in event handler because of scheduler or stack blocking

                            antiTheftBuildTopic(&topicStruct, cmdTopic, TOPIC_BUFFER_SIZE, "command", NULL);
                            
                            if (event->topic_len != strlen(cmdTopic) || strncmp(event->topic, cmdTopic, event->topic_len) != 0)
                            {
                                   break;
                            }

                            const size_t eventLength = event->data_len;
                            const char* data = event->data;
                            if(strncmp(data, "ARM", eventLength) == 0)
                            {
                                   ESP_LOGI(TAG, "DEVICE IS BEING ARMED");
                                   antiTheftPublishStatus("ARMED");
                                   // rest of arming logic
                            }
                            else if (strncmp(data, "DISARM", eventLength) == 0)
                            {
                                   ESP_LOGI(TAG, "DEVICE IS BEING DISARMED");
                                   antiTheftPublishStatus("DISARMED");
                                   // rest of disarming logic
                            }
                            else if(strncmp(data, "LOCATE", eventLength) == 0)
                            {
                                   ESP_LOGI(TAG, "LOCATING THE DEVICE");
                                   // rest of locating logic
                            }

                            break;
              case MQTT_EVENT_PUBLISHED:
                            ESP_LOGI(TAG, "------------ MQTT PUBLISH ------------ \n Message ID: %d", event->msg_id);
                            break;
              default:
                            ESP_LOGI(TAG, "Other MQTT event received with this event id: %d", eventId);
                            break;
       }
}

// PROJEKT / USER / DEVICE / CATEGORY / SUBCATEGORY
const char* antiTheftBuildTopic(const AntiTheftMqttTopics* t, 
       char* buffer,
       size_t bufferSize,
       const char* category,
       const char* subcategory)
{
       if(subcategory == NULL){
              snprintf(buffer, bufferSize, 
              "anti_theft/%s/%s/%s", 
              t->userId, t->deviceId, category);
              return buffer;
       }              
       snprintf(buffer, bufferSize,
       "anti_theft/%s/%s/%s/%s",
       t->userId, t->deviceId, category, subcategory);
       return buffer;
}

void antiTheftPublishStatus(const char *statusMsg)
{
    if (clientHandle == NULL) {
        ESP_LOGE("[ ANTI-THEFT MQTT ]", "Cannot publish, client not initialized!");
        return;
    }

    const char *topic = topicStruct.topicStatus;
    int msg_id = esp_mqtt_client_publish(clientHandle, topic, statusMsg, 0, 1, 0);

    ESP_LOGI("[ ANTI-THEFT MQTT ]", "Published status '%s' (msg_id=%d)", statusMsg, msg_id);
}


void antiTheftTopicsInit(AntiTheftMqttTopics* t, const char* user, const char* device) {
       strcpy(t->userId, user);
       strcpy(t->deviceId, device);

       snprintf(t->topicCommand, TOPIC_BUFFER_SIZE, "anti_theft/%s/%s/command", user, device);
       snprintf(t->topicStatus, TOPIC_BUFFER_SIZE, "anti_theft/%s/%s/status", user, device);
       snprintf(t->topicEvent, TOPIC_BUFFER_SIZE, "anti_theft/%s/%s/event", user, device);
       snprintf(t->topicDebug, TOPIC_BUFFER_SIZE, "anti_theft/%s/%s/debug", user, device);

}