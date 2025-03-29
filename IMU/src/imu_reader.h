#ifndef IMU_READER_H
#define IMU_READER_H

#include <stdint.h>
#include <pthread.h>

// IMU数据结构
typedef struct {
    // 加速度计数据 (单位: g)
    float accel_x;
    float accel_y;
    float accel_z;

    // 陀螺仪数据 (单位: rad/s)
    float gyro_x;
    float gyro_y;
    float gyro_z;

    // 磁力计数据 (单位: µT)
    float mag_x;
    float mag_y;
    float mag_z;

    // 温度 (单位: °C)
    float temperature;

    // 时间戳 (μs)
    uint64_t timestamp;
} ImuRawData;

// 处理后的姿态数据
typedef struct {
    // 欧拉角 (单位: 弧度)
    float roll;   // x轴旋转
    float pitch;  // y轴旋转
    float yaw;    // z轴旋转

    // 四元数
    float q0;
    float q1;
    float q2;
    float q3;

    // 线性加速度 (去除重力)
    float linear_accel_x;
    float linear_accel_y;
    float linear_accel_z;

    // 时间戳 (μs)
    uint64_t timestamp;
} ImuProcessedData;

// 控制结构体
typedef struct {
    int running;            // 运行标志
    pthread_mutex_t mutex;  // 互斥锁保护状态
} ImuControl;

// 常量定义
#define I2C_BUS            "/dev/i2c-1"
#define MPU6500_ADDR       0x68
#define AK8963_ADDR        0x0D 
#define FIFO_PATH          "/tmp/imu_control_fifo"

// 函数声明
int initialize_imu(void);
void* imu_reading_thread(void* arg);
void* command_listener_thread(void* arg);
int read_imu_data(ImuRawData* data);
void start_imu_collection(ImuControl* control);
void stop_imu_collection(ImuControl* control);
int is_imu_running(ImuControl* control);

#endif