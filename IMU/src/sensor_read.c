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
// Add debug messages to sensor read thread

void* sensor_read_thread(void *arg) {
    imu_data_buffer *buffer = (imu_data_buffer*)arg;
    imu_raw_data raw;
    int success_count = 0;
    int fail_count = 0;
    
    printf("[IMU] Sensor read thread started\n");
    
    while (1) {
        // Read MPU6500 data
        if (!read_mpu6500_data(&raw)) {
            fail_count++;
            if (fail_count % 100 == 0) {
                printf("[IMU] Failed to read MPU6500 data (%d consecutive failures)\n", fail_count);
            }
            usleep(100000);
            continue;
        }

        // Read AK8963 data
        if (!read_ak8963_data(&raw)) {
            fail_count++;
            if (fail_count % 100 == 0) {
                printf("[IMU] Failed to read AK8963 data (%d consecutive failures)\n", fail_count);
            }
            usleep(100000);
            continue;
        }

        // If we get here, we successfully read data
        fail_count = 0;
        success_count++;
        if (success_count % 1000 == 0) {
            printf("[IMU] Successfully reading sensor data (%d readings so far)\n", success_count);
        }

        pthread_mutex_lock(&buffer->mutex);
        if (buffer->count < BUFFER_SIZE) {
            int tail = (buffer->head + buffer->count) % BUFFER_SIZE;
            buffer->raw[tail] = raw;
            
            // Apply complementary filter
            complementary_filter(&raw, &buffer->filtered[tail]);
            
            buffer->count++;
        } else {
            printf("[IMU] Buffer full, dropping sample\n");
        }
        pthread_mutex_unlock(&buffer->mutex);
        
        usleep(5000); // 200Hz sampling rate
    }
    return NULL;
}

// 互补滤波器实现
// 修改complementary_filter函数

void complementary_filter(const imu_raw_data *raw, fused_data *out) {
    static float roll = 0, pitch = 0, yaw = 0;
    static struct timespec prev_ts = {0};
    static int init = 0;
    
    // 计算时间差
    float dt;
    if (prev_ts.tv_sec == 0 && prev_ts.tv_nsec == 0) {
        dt = 0.005; // 默认5ms
    } else {
        dt = (raw->ts.tv_sec - prev_ts.tv_sec) + 
             (raw->ts.tv_nsec - prev_ts.tv_nsec) * 1e-9;
        if (dt <= 0 || dt > 1.0) dt = 0.005; // 防止异常值
    }
    prev_ts = raw->ts;

    // 加速度计姿态估计
    float acc_roll = atan2f(raw->accel[1], raw->accel[2]);
    float acc_pitch = atan2f(-raw->accel[0], 
        sqrtf(raw->accel[1]*raw->accel[1] + raw->accel[2]*raw->accel[2]));

    // 互补滤波融合
    #define ALPHA 0.98
    roll = ALPHA * (roll + raw->gyro[0] * dt) + (1-ALPHA) * acc_roll;
    pitch = ALPHA * (pitch + raw->gyro[1] * dt) + (1-ALPHA) * acc_pitch;

    // 打印磁力计原始数据进行调试
    if (!init || rand() % 1000 == 0) {
        printf("[IMU] Mag raw: [%.2f, %.2f, %.2f]\n", 
              raw->mag[0], raw->mag[1], raw->mag[2]);
    }

    // 检查磁力计数据是否有效
    if (fabsf(raw->mag[0]) < 0.1 && fabsf(raw->mag[1]) < 0.1 && fabsf(raw->mag[2]) < 0.1) {
        // 磁力计数据无效，使用陀螺仪积分计算偏航角
        yaw += raw->gyro[2] * dt;
        
        // 确保在[-π, π]范围内
        while (yaw > M_PI) yaw -= 2*M_PI;
        while (yaw < -M_PI) yaw += 2*M_PI;
        
        if (!init || rand() % 1000 == 0) {
            printf("[IMU] Warning: Invalid magnetometer data, using gyro for yaw\n");
        }
    } else {
        // 磁力计数据有效，进行姿态补偿
        float mx = raw->mag[0] * cosf(pitch) + raw->mag[2] * sinf(pitch);
        float my = raw->mag[0] * sinf(roll)*sinf(pitch) + 
                  raw->mag[1] * cosf(roll) - 
                  raw->mag[2] * sinf(roll)*cosf(pitch);
                  
        // 计算偏航角
        float mag_yaw = atan2f(-my, mx);
        
        // 融合陀螺仪和磁力计数据
        #define MAG_WEIGHT 0.02  // 磁力计权重
        yaw = (1.0f - MAG_WEIGHT) * (yaw + raw->gyro[2] * dt) + MAG_WEIGHT * mag_yaw;
        
        // 确保在[-π, π]范围内
        while (yaw > M_PI) yaw -= 2*M_PI;
        while (yaw < -M_PI) yaw += 2*M_PI;
        
        if (!init || rand() % 1000 == 0) {
            printf("[IMU] Mag yaw: %.2f, Final yaw: %.2f\n", 
                  mag_yaw * 180/M_PI, yaw * 180/M_PI);
        }
    }
    
    init = 1;
    out->roll = roll;
    out->pitch = pitch;
    out->yaw = yaw;
    out->ts = raw->ts;
}