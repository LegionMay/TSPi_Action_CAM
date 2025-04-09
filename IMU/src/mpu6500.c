/* ---------- src/mpu6500.c ---------- */
#include "imu_logger.h"
#include "i2c_utils.h"
#define _GNU_SOURCE     // 启用GNU扩展
#include <math.h>       // 包含数学库

static int mpu_fd = -1;

// MPU6500初始化
int mpu6500_init(const char *device, int addr) {
    mpu_fd = i2c_init(device, addr);
    if (mpu_fd < 0) return 0;

    // 初始化配置序列
    uint8_t init_seq[] = {
        0x6B, 0x00,   // 退出睡眠模式
        0x1B, 0x18,   // 陀螺仪±2000dps
        0x1C, 0x10,   // 加速度计±8g
        0x19, 0x04    // 采样率1kHz
    };

   for (size_t i=0; i<sizeof(init_seq); i+=2) {
        if (i2c_write_reg(mpu_fd, init_seq[i], init_seq[i+1]) != 0) {
            return 0;
        }
    }
    return 1;
}

// 读取MPU6500数据
int read_mpu6500_data(imu_raw_data *data) {
    uint8_t buf[14];
    
    if (i2c_read_reg(mpu_fd, 0x3B, buf, sizeof(buf)) != sizeof(buf))
        return 0;

    // 加速度计数据处理
    data->accel[0] = (int16_t)(buf[0]<<8 | buf[1])  * (16.0/32768.0) * 9.81;
    data->accel[1] = (int16_t)(buf[2]<<8 | buf[3])  * (16.0/32768.0) * 9.81;
    data->accel[2] = (int16_t)(buf[4]<<8 | buf[5])  * (16.0/32768.0) * 9.81;

    // 陀螺仪数据处理
    data->gyro[0] = (int16_t)(buf[8]<<8 | buf[9])  * (2000.0/32768.0) * (M_PI/180.0);
    data->gyro[1] = (int16_t)(buf[10]<<8 | buf[11]) * (2000.0/32768.0) * (M_PI/180.0);
    data->gyro[2] = (int16_t)(buf[12]<<8 | buf[13]) * (2000.0/32768.0) * (M_PI/180.0);

    clock_gettime(CLOCK_REALTIME, &data->ts);
    return 1;
}