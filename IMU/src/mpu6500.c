/*
 * @Author: LegionMay
 * @FilePath: /TSPi_Action/IMU/src/mpu6500.c
 */
/* ---------- src/mpu6500.c ---------- */
#include "imu_logger.h"
#include "i2c_utils.h"
#define _GNU_SOURCE     // 启用GNU扩展
#include <math.h>       // 包含数学库

static int mpu_fd = -1;

// MPU6500初始化
int mpu6500_init(const char *device, int addr) {
    printf("[IMU] Initializing MPU6500 on %s, address 0x%02X\n", device, addr);
    
    mpu_fd = i2c_init(device, addr);
    if (mpu_fd < 0) {
        printf("[IMU] Failed to open I2C device for MPU6500\n");
        return 0;
    }
    printf("[IMU] I2C device opened successfully for MPU6500\n");

    // Initialization sequence
    uint8_t init_seq[] = {
        0x6B, 0x00,   // Exit sleep mode
        0x1B, 0x18,   // Gyro range ±2000dps
        0x1C, 0x10,   // Accelerometer range ±8g
        0x19, 0x04    // Sample rate 1kHz
    };

    printf("[IMU] Sending MPU6500 initialization sequence\n");
    for (size_t i=0; i<sizeof(init_seq); i+=2) {
        printf("[IMU] Writing to MPU6500: register 0x%02X, value 0x%02X\n", 
               init_seq[i], init_seq[i+1]);
               
        if (i2c_write_reg(mpu_fd, init_seq[i], init_seq[i+1]) != 0) {
            printf("[IMU] Failed to write to MPU6500 register 0x%02X\n", init_seq[i]);
            return 0;
        }
    }
    printf("[IMU] MPU6500 initialization complete\n");
    return 1;
}

// Read MPU6500 data with debug info
int read_mpu6500_data(imu_raw_data *data) {
    uint8_t buf[14];
    
    if (i2c_read_reg(mpu_fd, 0x3B, buf, sizeof(buf)) != sizeof(buf)) {
        // Only show detailed error every 1000 attempts to avoid log spam
        static int error_count = 0;
        if ((++error_count % 1000) == 0) {
            printf("[IMU] Failed to read from MPU6500 (attempt #%d)\n", error_count);
        }
        return 0;
    }

    // Accelerometer data processing
    data->accel[0] = (int16_t)(buf[0]<<8 | buf[1])  * (16.0/32768.0) * 9.81;
    data->accel[1] = (int16_t)(buf[2]<<8 | buf[3])  * (16.0/32768.0) * 9.81;
    data->accel[2] = (int16_t)(buf[4]<<8 | buf[5])  * (16.0/32768.0) * 9.81;

    // Gyroscope data processing
    data->gyro[0] = (int16_t)(buf[8]<<8 | buf[9])  * (2000.0/32768.0) * (M_PI/180.0);
    data->gyro[1] = (int16_t)(buf[10]<<8 | buf[11]) * (2000.0/32768.0) * (M_PI/180.0);
    data->gyro[2] = (int16_t)(buf[12]<<8 | buf[13]) * (2000.0/32768.0) * (M_PI/180.0);

    clock_gettime(CLOCK_REALTIME, &data->ts);
    return 1;
}