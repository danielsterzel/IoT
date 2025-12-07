#ifndef MPU6050_H
#define MPU6050_H

#include "driver/i2c.h"

#define MPU6050_I2C_ADDR 0x68 // need to check it  should be this one because no other mpu 

typedef struct {
    float ax, ay, az;
    float gx, gy, gz;
} Mpu6050Data;

void mpu6050Init();
void mpu6050ReadRaw(int16_t accel[3], int16_t gyro[3]);
void mpu6050Read(Mpu6050Data* out);

#endif // MPU6050_H