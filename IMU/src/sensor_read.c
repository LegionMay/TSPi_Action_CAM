/*
 * @Author: LegionMay
 * @FilePath: /TSPi_Action/IMU/src/sensor_read.c
 */
/* ---------- src/sensor_read.c ---------- */
#include <unistd.h>
#include <math.h>
#include <linux/i2c-dev.h>
#include "imu_logger.h"
#include <stdlib.h>    // 定义 exit 和 EXIT_FAILURE
#include <unistd.h>    // 定义 usleep

// MPU6500数据采集
void* sensor_read_thread(void *arg) {
    imu_data_buffer *buffer = (imu_data_buffer*)arg;
    imu_raw_data raw;
    
    while (1) {
        // 读取MPU6500数据
        if (!read_mpu6500_data(&raw)) {
            usleep(100000);
            continue;
        }

        // 读取AK8963数据
        if (!read_ak8963_data(&raw)) {
            usleep(100000);
            continue;
        }

        pthread_mutex_lock(&buffer->mutex);
        if (buffer->count < BUFFER_SIZE) {
            int tail = (buffer->head + buffer->count) % BUFFER_SIZE;
            buffer->raw[tail] = raw;
            
            // 执行互补滤波
            complementary_filter(&raw, &buffer->filtered[tail]);
            
            buffer->count++;
        }
        pthread_mutex_unlock(&buffer->mutex);
        
        usleep(5000); // 200Hz采样率
    }
    return NULL;
}

// 互补滤波器实现
void complementary_filter(const imu_raw_data *raw, fused_data *out) {
    static float roll = 0, pitch = 0;
    static struct timespec prev_ts = {0};
    
    // 计算时间差
    float dt = (raw->ts.tv_sec - prev_ts.tv_sec) + 
              (raw->ts.tv_nsec - prev_ts.tv_nsec) * 1e-9;
    prev_ts = raw->ts;

    // 加速度计姿态估计
    float acc_roll = atan2f(raw->accel[1], raw->accel[2]);
    float acc_pitch = atan2f(-raw->accel[0], 
        sqrtf(raw->accel[1]*raw->accel[1] + raw->accel[2]*raw->accel[2]));

    // 互补滤波融合
    #define ALPHA 0.98
    roll = ALPHA * (roll + raw->gyro[0] * dt) + (1-ALPHA) * acc_roll;
    pitch = ALPHA * (pitch + raw->gyro[1] * dt) + (1-ALPHA) * acc_pitch;

    // 磁力计偏航角计算
    float mx = raw->mag[0] * cos(pitch) + raw->mag[2] * sin(pitch);
    float my = raw->mag[0] * sin(roll)*sin(pitch) + 
              raw->mag[1] * cos(roll) - 
              raw->mag[2] * sin(roll)*cos(pitch);
    out->yaw = atan2(-my, mx);

    out->roll = roll;
    out->pitch = pitch;
    out->ts = raw->ts;
}