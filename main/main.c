#include "esp_wifi.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "esp_log.h"
// #include "anti_theft_mqtt_client.h"
#include "anti_theft_wifi.h"


static const char* TAG = "MAIN";

void app_main(){
    ESP_LOGI(TAG, "STARTING ANTI THEFT SYSTEM");
    antiTheftWifiInit();
    
    while(1){
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

}