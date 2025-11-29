#include <string.h>
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "lwip/sockets.h"
#include "lwip/netdb.h"
#include "driver/gpio.h"

#define WIFI_SSID      ""
#define WIFI_PASS      ""
#define HOST           "google.com"
#define PORT           80
#define BLINK_GPIO     2

static EventGroupHandle_t wifi_event_group;
static EventGroupHandle_t led_status_group;

#define WIFI_CONNECTED_BIT BIT0
#define LED_STATUS_CONNECTED_BIT BIT0

static const char *TAG = "http_example";

void wifi_event_handler(void *arg, esp_event_base_t event_base,
                        int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } 
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        xEventGroupClearBits(wifi_event_group, WIFI_CONNECTED_BIT);
        xEventGroupClearBits(led_status_group, LED_STATUS_CONNECTED_BIT);
        ESP_LOGI(TAG, "Retry connecting to the AP...");
        esp_wifi_connect();
    } 
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        xEventGroupSetBits(wifi_event_group, WIFI_CONNECTED_BIT);
        xEventGroupSetBits(led_status_group, LED_STATUS_CONNECTED_BIT);
    }
}

void led_task(void *pvParameters)
{
    gpio_reset_pin(BLINK_GPIO);
    gpio_set_direction(BLINK_GPIO, GPIO_MODE_OUTPUT);

    while (1) {
        EventBits_t bits = xEventGroupGetBits(led_status_group);

        if (bits & LED_STATUS_CONNECTED_BIT) {
            gpio_set_level(BLINK_GPIO, 1);
            vTaskDelay(500 / portTICK_PERIOD_MS);
        } else {
            gpio_set_level(BLINK_GPIO, 1);
            vTaskDelay(200 / portTICK_PERIOD_MS);
            gpio_set_level(BLINK_GPIO, 0);
            vTaskDelay(200 / portTICK_PERIOD_MS);
        }
    }
}


void http_get_task(void *pvParameters)
{
    char *buffer = malloc(1024);
    if (buffer == NULL) {
        ESP_LOGE(TAG, "Failed to allocate memory");
        vTaskDelete(NULL);
    }

    while(1) {
        xEventGroupWaitBits(wifi_event_group, WIFI_CONNECTED_BIT, false, true, portMAX_DELAY);

        struct hostent *server = gethostbyname(HOST);
        if (server == NULL) {
            ESP_LOGE(TAG, "DNS lookup failed... retrying in 5s");
            vTaskDelay(5000 / portTICK_PERIOD_MS);
            continue;
        }

        int sock = socket(AF_INET, SOCK_STREAM, 0);
        if (sock < 0) {
            ESP_LOGE(TAG, "Unable to create socket: errno %d", errno);
            vTaskDelay(1000 / portTICK_PERIOD_MS);
            continue;
        }

        struct sockaddr_in serv_addr = {
            .sin_family = AF_INET,
            .sin_port = htons(PORT),
            .sin_addr = *(struct in_addr*)server->h_addr
        };

        if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) != 0) {
            ESP_LOGE(TAG, "Socket connect failed... retrying");
            close(sock);
            vTaskDelay(4000 / portTICK_PERIOD_MS);
            continue;
        }

        char *request = malloc(200);
        if(request == NULL) {
            ESP_LOGE(TAG, "Couldn't allocate buffer:(");
            close(sock);
            continue;
        } 

        snprintf(request, 200,
                "GET / HTTP/1.1\r\n"
                "Host: %s\r\n"
                "Connection: close\r\n"
                "\r\n", HOST);

        if (send(sock, request, strlen(request), 0) < 0) {
            ESP_LOGE(TAG, "Error occurred during sending");
            free(request);
            close(sock);
            continue;
        }

        int len;
        while ((len = recv(sock, buffer, 1023, 0)) > 0) {
            buffer[len] = '\0'; 
            printf("%s", buffer);
        }
        printf("\n--- SERVER RESPONSE END ---\n");

        close(sock);
        
        ESP_LOGI(TAG, "Request finished, waiting 10s before next...");
        free(request);
        vTaskDelay(10000 / portTICK_PERIOD_MS);
    }
    
    free(buffer);
    vTaskDelete(NULL);
}

void app_main(void)
{
    nvs_flash_init();
    
    wifi_event_group = xEventGroupCreate();
    led_status_group = xEventGroupCreate();

    esp_netif_init();
    esp_event_loop_create_default();
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);

    esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL);
    esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL);

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASS,
            .threshold.authmode = WIFI_AUTH_WPA2_PSK, 
        }
    };

    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
    esp_wifi_start();

    xTaskCreate(&http_get_task, "http_get_task", 4096, NULL, 5, NULL);
    xTaskCreate(&led_task, "led_task", 2048, NULL, 5, NULL);
}
