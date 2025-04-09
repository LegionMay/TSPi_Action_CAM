/*
 * @Author: LegionMay
 * @FilePath: /TSPi_Action/IMU/src/imu_logger.h
 */
/* ---------- src/imu_logger.h ---------- */
#ifndef IMU_LOGGER_H
#define IMU_LOGGER_H

#include <sys/msg.h>
#include <pthread.h>
#include <time.h>
#include "i2c_utils.h" 

#define MSG_KEY 0x1234      // 消息队列键值
#define BUFFER_SIZE 128     // 环形缓冲区大小
#define MAX_MSG_SIZE 64     // 最大消息大小

// System V消息结构体
typedef struct {
    long mtype;             // 必须作为第一个成员
    float roll;             // 横滚角（弧度）
    float pitch;            // 俯仰角
    float yaw;              // 偏航角
    struct timespec ts;     // 时间戳
} imu_msg;

// 传感器原始数据结构
typedef struct {
    float accel[3];         // 加速度计 (m/s²)
    float gyro[3];          // 陀螺仪 (rad/s)
    float mag[3];           // 磁力计 (μT)
    struct timespec ts;     // 时间戳
} imu_raw_data;

// 融合后的姿态数据
typedef struct {
    float roll;
    float pitch;
    float yaw;
    struct timespec ts;
} fused_data;

// 环形缓冲区结构
typedef struct {
    imu_raw_data raw[BUFFER_SIZE];
    fused_data filtered[BUFFER_SIZE];
    int head;
    int count;
    pthread_mutex_t mutex;
} imu_data_buffer;

// 函数声明
int mpu6500_init(const char *device, int addr);
int ak8963_init(const char *device, int addr);
int create_msg_queue(void);
void send_to_msg_queue(int msqid, const imu_msg *msg);
void* sensor_read_thread(void *arg);
void* logging_thread(void *arg);

#endif