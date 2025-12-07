#ifndef MPU6050_H
#define MPU6050_H

#include "driver/i2c.h"
#include "esp_err.h"

// MPU6050 I2C Address
#define MPU6050_ADDR            0x68

// Registers
#define MPU6050_REG_PWR_MGMT_1  0x6B
#define MPU6050_REG_ACCEL_XOUT_H 0x3B
#define MPU6050_REG_WHO_AM_I    0x75

// Structure to hold sensor data
typedef struct {
    int16_t accel_x;
    int16_t accel_y;
    int16_t accel_z;
    int16_t temp;
    int16_t gyro_x;
    int16_t gyro_y;
    int16_t gyro_z;
} mpu6050_data_t;

/**
 * @brief Initialize the MPU6050 sensor
 * * @param port The I2C port number (I2C_NUM_0 or I2C_NUM_1)
 * @return esp_err_t ESP_OK on success
 */
esp_err_t mpu6050_init(i2c_port_t port);

/**
 * @brief Read all accelerometer, temperature, and gyroscope data
 * * @param port The I2C port number
 * @param data Pointer to mpu6050_data_t struct to fill
 * @return esp_err_t ESP_OK on success
 */
esp_err_t mpu6050_read_data(i2c_port_t port, mpu6050_data_t *data);

#endif // MPU6050_H