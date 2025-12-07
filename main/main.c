#include "esp_wifi.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "esp_log.h"
#include "driver/i2c.h"         // Added for I2C config
#include "anti_theft_wifi.h"
#include "mpu6050.h"            // Added MPU6050 header

static const char* TAG = "MAIN";

// I2C Configuration
#define I2C_MASTER_SCL_IO           22
#define I2C_MASTER_SDA_IO           21
#define I2C_MASTER_NUM              0
#define I2C_MASTER_FREQ_HZ          100000
#define I2C_MASTER_TX_BUF_DISABLE   0
#define I2C_MASTER_RX_BUF_DISABLE   0

static void i2c_master_init(void)
{
    i2c_config_t conf = {
            .mode = I2C_MODE_MASTER,
            .sda_io_num = I2C_MASTER_SDA_IO,
            .scl_io_num = I2C_MASTER_SCL_IO,
            .sda_pullup_en = GPIO_PULLUP_ENABLE,
            .scl_pullup_en = GPIO_PULLUP_ENABLE,
            .master.clk_speed = I2C_MASTER_FREQ_HZ,
    };
    i2c_param_config(I2C_MASTER_NUM, &conf);
    i2c_driver_install(I2C_MASTER_NUM, conf.mode, I2C_MASTER_RX_BUF_DISABLE, I2C_MASTER_TX_BUF_DISABLE, 0);
}

void app_main(){
    ESP_LOGI(TAG, "STARTING ANTI THEFT SYSTEM");

    // Initialize NVS and WiFi
    antiTheftWifiInit();

    // Initialize I2C
    i2c_master_init();

    // Initialize MPU6050
    if (mpu6050_init(I2C_MASTER_NUM) == ESP_OK) {
        ESP_LOGI(TAG, "Sensor is online.");
    } else {
        ESP_LOGE(TAG, "Sensor initialization failed");
    }

    // Main loop
    mpu6050_data_t mpu_data;
    while(1){
        if (mpu6050_read_data(I2C_MASTER_NUM, &mpu_data) == ESP_OK) {
            ESP_LOGI(TAG, "Accel: X=%d Y=%d Z=%d", mpu_data.accel_x, mpu_data.accel_y, mpu_data.accel_z);
        } else {
            ESP_LOGE(TAG, "Error reading sensor");
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}