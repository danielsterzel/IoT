#include "mpu6050.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stddef.h>


/*                  =============== INFORMATION ===============
 * Sample Rate Divider -> SMPLRT_DIV:
 *     Mpu has different sampling rates for gyroscope(8 kHz) and acclerometer(1 kHz)
 * 
 * FSYNC -> Frame Synchronization:
 *      no need for it because we do not need to sync up with a different clock.
 * We set DLPF to 42 Hz to remove high frequnecy noise, stabilize readings and in order to
 * reduce vibration sensitivity 
 */


static const char* TAG = "MPU6050";

#define I2C_PORT I2C_NUM_0
#define I2C_SDA 1
#define I2C_SCL 2
// =============== fwds ===============
static esp_err_t mpuWrite(uint8_t registr, uint8_t value);
static esp_err_t mpuRead(uint8_t registr, uint8_t* buffer, size_t len);
static void i2cMasterInit();

void mpu6050Init()
{
    i2cMasterInit();

    mpuWrite(0x6B, 0x00); // turning on Register -> PWR_MGMT_1 we set to 0 to wake up the sensor
    mpuWrite(0x19, 0x07); // SMPLRT_DIV
    mpuWrite(0x1A, 0x03); // FSync cancelattion and setting DLPF to 42 Hz
    mpuWrite(0x1B, 0x00); // gyro
    mpuWrite(0x1C, 0x00); // accel
    ESP_LOGI(TAG, "MPU6050 Initialized successfully.");
}

void mpu6050ReadRaw(int16_t accel[3], int16_t gyro[3])
{
    uint8_t raw[14];

    if(mpuRead(0x3B, raw, 14) != ESP_OK)
    {
        ESP_LOGE(TAG, "FAILED TO READ SENSOR DATA !!!!");
        return;
    }

    /**
     * 0 to 1 accel x
     * 2 to 3 accel y
     * 4 to 5 accel z
     * etc.
     * 6 to 7 is temp
     * we shift the high byte to the left in order to get a 16 bit integer
     */
    accel[0] = (raw[0] << 8) | raw[1];
    accel[1] = (raw[2] << 8) | raw[3];
    accel[2] = (raw[4] << 8) | raw[5];

    gyro[0] = (raw[8] << 8) | raw[9];
    gyro[1] = (raw[10] << 8) | raw[11];
    gyro[2] = (raw[12] << 8) | raw[13];
}

void mpu6050Read(Mpu6050Data* output)
{
    int16_t accel[3], gyro[3];
    mpu6050ReadRaw(accel, gyro);
    
    const float accelScale = 16384.0f; // for +-2 g from the datasheet. 8192 / 16384 results in 0.5 g
    const float gyroScale = 131.0f; // for + - 250 deg tilts 131 LSB is 1 degree
 
    output->ax = accel[0] / accelScale;
    output->ay = accel[1] / accelScale;
    output->az = accel[2] / accelScale;

    output->gx = gyro[0] / gyroScale;
    output->gy = gyro[1] / gyroScale;
    output->gz = gyro[2] / gyroScale;
} 



static void i2cMasterInit(){
    i2c_config_t config = { 0 };

    config.mode = I2C_MODE_MASTER;
    config.sda_io_num = I2C_SDA;
    config.scl_io_num = I2C_SCL;
    config.sda_pullup_en = GPIO_PULLUP_ENABLE;
    config.scl_pullup_en = GPIO_PULLUP_ENABLE;
    config.master.clk_speed = 400000;

    ESP_ERROR_CHECK(i2c_param_config(I2C_PORT, &config));
    ESP_ERROR_CHECK(i2c_driver_install(I2C_PORT, config.mode, 0, 0, 0));
    ESP_LOGI(TAG, "I2C initialized (SDA=%d, SCL=%d)", I2C_SDA, I2C_SCL);

}


static esp_err_t mpuWrite(uint8_t registr, uint8_t value) 
{
    uint8_t buffer[2] = { registr, value };
    return i2c_master_write_to_device(I2C_PORT, MPU6050_I2C_ADDR, buffer, 2, pdMS_TO_TICKS(100));
}

static esp_err_t mpuRead(uint8_t registr, uint8_t *buffer, size_t len)
{
    return i2c_master_write_read_device(I2C_PORT,
    MPU6050_I2C_ADDR,
    &registr, 1,
    buffer, len,
    pdMS_TO_TICKS(100));
}