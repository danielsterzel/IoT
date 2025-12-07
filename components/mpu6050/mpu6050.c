#include "mpu6050.h"
#include "esp_log.h"
#include "driver/i2c.h"

static const char *TAG = "MPU6050";

esp_err_t mpu6050_init(i2c_port_t port)
{
    esp_err_t ret;

    // Check WHO_AM_I to ensure device is connected
    uint8_t who_am_i;
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (MPU6050_ADDR << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, MPU6050_REG_WHO_AM_I, true);
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (MPU6050_ADDR << 1) | I2C_MASTER_READ, true);
    i2c_master_read_byte(cmd, &who_am_i, I2C_MASTER_LAST_NACK);
    i2c_master_stop(cmd);
    ret = i2c_master_cmd_begin(port, cmd, pdMS_TO_TICKS(100));
    i2c_cmd_link_delete(cmd);

    if (ret != ESP_OK || who_am_i != 0x68) {
        ESP_LOGE(TAG, "MPU6050 not found (WHO_AM_I=0x%02x)", who_am_i);
        return ESP_FAIL;
    }

    // Wake up MPU6050 (It starts in sleep mode)
    cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (MPU6050_ADDR << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, MPU6050_REG_PWR_MGMT_1, true);
    i2c_master_write_byte(cmd, 0x00, true); // Write 0 to wake up
    i2c_master_stop(cmd);
    ret = i2c_master_cmd_begin(port, cmd, pdMS_TO_TICKS(100));
    i2c_cmd_link_delete(cmd);

    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "MPU6050 Initialized successfully");
    } else {
        ESP_LOGE(TAG, "Failed to wake up MPU6050");
    }

    return ret;
}

esp_err_t mpu6050_read_data(i2c_port_t port, mpu6050_data_t *data)
{
    if (data == NULL) return ESP_ERR_INVALID_ARG;

    uint8_t raw_data[14];

    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (MPU6050_ADDR << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, MPU6050_REG_ACCEL_XOUT_H, true);
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (MPU6050_ADDR << 1) | I2C_MASTER_READ, true);
    i2c_master_read(cmd, raw_data, 14, I2C_MASTER_LAST_NACK);
    i2c_master_stop(cmd);
    esp_err_t ret = i2c_master_cmd_begin(port, cmd, pdMS_TO_TICKS(100));
    i2c_cmd_link_delete(cmd);

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read data");
        return ret;
    }

    // Parse data (Big Endian)
    data->accel_x = (int16_t)((raw_data[0] << 8) | raw_data[1]);
    data->accel_y = (int16_t)((raw_data[2] << 8) | raw_data[3]);
    data->accel_z = (int16_t)((raw_data[4] << 8) | raw_data[5]);
    data->temp    = (int16_t)((raw_data[6] << 8) | raw_data[7]);
    data->gyro_x  = (int16_t)((raw_data[8] << 8) | raw_data[9]);
    data->gyro_y  = (int16_t)((raw_data[10] << 8) | raw_data[11]);
    data->gyro_z  = (int16_t)((raw_data[12] << 8) | raw_data[13]);

    return ESP_OK;
}