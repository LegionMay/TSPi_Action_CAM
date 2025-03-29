#include "imu_reader.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/i2c-dev.h>
#include <linux/i2c.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <errno.h>
#include <math.h>

// MPU6500寄存器映射
#define MPU6500_REG_SMPLRT_DIV      0x19
#define MPU6500_REG_CONFIG          0x1A
#define MPU6500_REG_GYRO_CONFIG     0x1B
#define MPU6500_REG_ACCEL_CONFIG    0x1C
#define MPU6500_REG_ACCEL_CONFIG2   0x1D
#define MPU6500_REG_PWR_MGMT_1      0x6B
#define MPU6500_REG_PWR_MGMT_2      0x6C
#define MPU6500_REG_WHO_AM_I        0x75

#define MPU6500_REG_ACCEL_XOUT_H    0x3B
#define MPU6500_REG_TEMP_OUT_H      0x41
#define MPU6500_REG_GYRO_XOUT_H     0x43
#define MPU6500_REG_USER_CTRL       0x6A
#define MPU6500_REG_INT_PIN_CFG     0x37
#define MPU6500_REG_INT_ENABLE      0x38

// AK8963寄存器映射
#define AK8963_REG_WIA              0x00
#define AK8963_REG_CNTL1            0x0A
#define AK8963_REG_CNTL2            0x0B
#define AK8963_REG_ASAX             0x10
#define AK8963_REG_ST1              0x02
#define AK8963_REG_HXL              0x03

// 全局变量
static int i2c_fd = -1;                 // I2C文件描述符
static float accel_scale = 1.0f/16384;  // ±2g范围下的加速度计比例因子
static float gyro_scale = 1.0f/131;     // ±250°/s范围下的陀螺仪比例因子
static float mag_scale_x = 1.0f;        // 磁力计X轴校准比例因子
static float mag_scale_y = 1.0f;        // 磁力计Y轴校准比例因子
static float mag_scale_z = 1.0f;        // 磁力计Z轴校准比例因子

// I2C通信函数
static int i2c_write_byte(int fd, uint8_t dev_addr, uint8_t reg_addr, uint8_t data) {
    uint8_t buf[2];
    buf[0] = reg_addr;
    buf[1] = data;
    
    if (ioctl(fd, I2C_SLAVE, dev_addr) < 0) {
        perror("Failed to set I2C device address");
        return -1;
    }
    
    if (write(fd, buf, 2) != 2) {
        perror("Failed to write to I2C device");
        return -1;
    }
    
    return 0;
}

static int i2c_read_bytes(int fd, uint8_t dev_addr, uint8_t reg_addr, uint8_t *data, size_t length) {
    if (ioctl(fd, I2C_SLAVE, dev_addr) < 0) {
        perror("Failed to set I2C device address");
        return -1;
    }
    
    if (write(fd, &reg_addr, 1) != 1) {
        perror("Failed to write register address");
        return -1;
    }
    
    if (read(fd, data, length) != (ssize_t)length) {
        perror("Failed to read from I2C device");
        return -1;
    }
    
    return 0;
}

// 初始化MPU6500
static int initialize_mpu6500() {
    uint8_t who_am_i;
    
    // 检查设备ID
    if (i2c_read_bytes(i2c_fd, MPU6500_ADDR, MPU6500_REG_WHO_AM_I, &who_am_i, 1) < 0) {
        return -1;
    }
    
    if (who_am_i != 0x70) {  // MPU6500的WHO_AM_I寄存器应该返回0x70
        fprintf(stderr, "MPU6500 not found, WHO_AM_I = 0x%02X\n", who_am_i);
        return -1;
    }
    
    // 复位设备
    i2c_write_byte(i2c_fd, MPU6500_ADDR, MPU6500_REG_PWR_MGMT_1, 0x80);
    usleep(100000);  // 等待复位完成
    
    // 唤醒并选择最佳时钟源
    i2c_write_byte(i2c_fd, MPU6500_ADDR, MPU6500_REG_PWR_MGMT_1, 0x01); // PLL with X axis gyroscope reference
    
    // 配置陀螺仪量程为±250dps
    i2c_write_byte(i2c_fd, MPU6500_ADDR, MPU6500_REG_GYRO_CONFIG, 0x00);
    
    // 配置加速度计量程为±2g
    i2c_write_byte(i2c_fd, MPU6500_ADDR, MPU6500_REG_ACCEL_CONFIG, 0x00);
    
    // 配置数字低通滤波器
    i2c_write_byte(i2c_fd, MPU6500_ADDR, MPU6500_REG_CONFIG, 0x03);
    
    // 设置采样率分频器
    i2c_write_byte(i2c_fd, MPU6500_ADDR, MPU6500_REG_SMPLRT_DIV, 0x04); // 200Hz采样率
    
    // 启用旁路模式访问磁力计
    i2c_write_byte(i2c_fd, MPU6500_ADDR, MPU6500_REG_INT_PIN_CFG, 0x02);
    
    return 0;
}

// 初始化AK8963磁力计
static int initialize_ak8963() {
    uint8_t who_am_i;
    uint8_t asa[3];
    
    // 检查设备ID
    if (i2c_read_bytes(i2c_fd, AK8963_ADDR, AK8963_REG_WIA, &who_am_i, 1) < 0) {
        return -1;
    }
    
    if (who_am_i != 0x48) {  // AK8963的WIA寄存器应该返回0x48
        fprintf(stderr, "AK8963 not found, WIA = 0x%02X\n", who_am_i);
        return -1;
    }
    
    // 复位AK8963
    i2c_write_byte(i2c_fd, AK8963_ADDR, AK8963_REG_CNTL2, 0x01);
    usleep(100000);  // 等待复位完成
    
    // 进入FUSE ROM访问模式
    i2c_write_byte(i2c_fd, AK8963_ADDR, AK8963_REG_CNTL1, 0x0F);
    usleep(100000);
    
    // 读取灵敏度调整值
    if (i2c_read_bytes(i2c_fd, AK8963_ADDR, AK8963_REG_ASAX, asa, 3) < 0) {
        return -1;
    }
    
    // 计算校准系数
    mag_scale_x = ((((float)asa[0] - 128) / 256) + 1) * 4912.0f / 32760.0f;
    mag_scale_y = ((((float)asa[1] - 128) / 256) + 1) * 4912.0f / 32760.0f;
    mag_scale_z = ((((float)asa[2] - 128) / 256) + 1) * 4912.0f / 32760.0f;
    
    // 退出FUSE ROM访问模式
    i2c_write_byte(i2c_fd, AK8963_ADDR, AK8963_REG_CNTL1, 0x00);
    usleep(100000);
    
    // 设置连续测量模式2 (100Hz) 和16位输出
    i2c_write_byte(i2c_fd, AK8963_ADDR, AK8963_REG_CNTL1, 0x16);
    usleep(100000);
    
    return 0;
}

// 初始化IMU
int initialize_imu() {
    // 打开I2C设备
    i2c_fd = open(I2C_BUS, O_RDWR);
    if (i2c_fd < 0) {
        perror("Failed to open I2C bus");
        return -1;
    }
    
    // 初始化MPU6500
    if (initialize_mpu6500() < 0) {
        close(i2c_fd);
        return -1;
    }
    
    // 初始化AK8963
    if (initialize_ak8963() < 0) {
        close(i2c_fd);
        return -1;
    }
    
    return 0;
}

// 读取IMU数据
int read_imu_data(ImuRawData* data) {
    uint8_t buffer[21];
    uint8_t mag_buffer[7];
    struct timeval tv;
    int16_t raw_data[10]; // accel(3) + temp(1) + gyro(3) + mag(3)
    uint8_t mag_status;
    
    if (data == NULL) {
        return -1;
    }
    
    // 获取当前时间戳
    gettimeofday(&tv, NULL);
    data->timestamp = (uint64_t)tv.tv_sec * 1000000 + tv.tv_usec;
    
    // 读取加速度计、温度传感器和陀螺仪数据
    if (i2c_read_bytes(i2c_fd, MPU6500_ADDR, MPU6500_REG_ACCEL_XOUT_H, buffer, 14) < 0) {
        return -1;
    }
    
    // 读取磁力计数据前先检查数据就绪状态
    if (i2c_read_bytes(i2c_fd, AK8963_ADDR, AK8963_REG_ST1, &mag_status, 1) < 0) {
        return -1;
    }
    
    // 仅当有新数据时才读取磁力计数据
    if (mag_status & 0x01) {
        if (i2c_read_bytes(i2c_fd, AK8963_ADDR, AK8963_REG_HXL, mag_buffer, 7) < 0) {
            return -1;
        }
        
        // 检查是否有磁力计数据溢出
        if (!(mag_buffer[6] & 0x08)) {
            // 合并高低字节
            raw_data[7] = (int16_t)(mag_buffer[1] << 8 | mag_buffer[0]);
            raw_data[8] = (int16_t)(mag_buffer[3] << 8 | mag_buffer[2]);
            raw_data[9] = (int16_t)(mag_buffer[5] << 8 | mag_buffer[4]);
            
            // 应用校准和转换为物理单位 (µT)
            data->mag_x = (float)raw_data[7] * mag_scale_x;
            data->mag_y = (float)raw_data[8] * mag_scale_y;
            data->mag_z = (float)raw_data[9] * mag_scale_z;
        }
    }
    
    // 合并加速度计和陀螺仪高低字节
    for (int i = 0; i < 7; i++) {
        raw_data[i] = (int16_t)(buffer[i*2] << 8 | buffer[i*2+1]);
    }
    
    // 转换为物理单位
    data->accel_x = (float)raw_data[0] * accel_scale;
    data->accel_y = (float)raw_data[1] * accel_scale;
    data->accel_z = (float)raw_data[2] * accel_scale;
    
    data->temperature = ((float)raw_data[3] / 340.0f) + 36.53f;
    
    data->gyro_x = (float)raw_data[4] * gyro_scale * M_PI / 180.0f; // 转换为弧度/秒
    data->gyro_y = (float)raw_data[5] * gyro_scale * M_PI / 180.0f;
    data->gyro_z = (float)raw_data[6] * gyro_scale * M_PI / 180.0f;
    
    return 0;
}

// IMU数据读取线程
void* imu_reading_thread(void* arg) {
    ImuControl* control = (ImuControl*)arg;
    ImuRawData raw_data;
    
    while (1) {
        // 检查是否应该运行
        if (!is_imu_running(control)) {
            sleep(1);  // 未运行时降低CPU使用率
            continue;
        }
        
        // 初始化IMU
        if (initialize_imu() < 0) {
            fprintf(stderr, "Failed to initialize IMU\n");
            sleep(5);  // 失败后等待5秒再重试
            continue;
        }
        
        printf("IMU data collection started\n");
        
        // 当运行标志为1时，持续读取数据
        while (is_imu_running(control)) {
            // 读取IMU数据
            if (read_imu_data(&raw_data) == 0) {
                // 数据处理和IPC通信在main.c中实现
                printf("IMU Data: Accel[%.2f,%.2f,%.2f] Gyro[%.2f,%.2f,%.2f] Mag[%.2f,%.2f,%.2f]\n",
                       raw_data.accel_x, raw_data.accel_y, raw_data.accel_z,
                       raw_data.gyro_x, raw_data.gyro_y, raw_data.gyro_z,
                       raw_data.mag_x, raw_data.mag_y, raw_data.mag_z);
            }
            
            // 控制读取频率
            usleep(5000);  // 200Hz
        }
        
        close(i2c_fd);
        i2c_fd = -1;
        printf("IMU data collection stopped\n");
    }
    
    return NULL;
}

// 命令监听线程
void* command_listener_thread(void* arg) {
    ImuControl* control = (ImuControl*)arg;
    int fifo_fd;
    char cmd[20];
    
    // 创建命名管道
    unlink(FIFO_PATH);  // 删除可能已存在的FIFO
    if (mkfifo(FIFO_PATH, 0666) < 0) {
        perror("Failed to create FIFO");
        return NULL;
    }
    
    printf("Command listener started. FIFO: %s\n", FIFO_PATH);
    
    while (1) {
        // 打开FIFO用于读取
        fifo_fd = open(FIFO_PATH, O_RDONLY);
        if (fifo_fd < 0) {
            perror("Failed to open FIFO");
            sleep(5);
            continue;
        }
        
        // 读取命令
        memset(cmd, 0, sizeof(cmd));
        read(fifo_fd, cmd, sizeof(cmd) - 1);
        close(fifo_fd);
        
        // 处理命令
        if (strcmp(cmd, "start") == 0) {
            printf("Received command: start\n");
            start_imu_collection(control);
        } 
        else if (strcmp(cmd, "stop") == 0) {
            printf("Received command: stop\n");
            stop_imu_collection(control);
        }
        else if (strcmp(cmd, "status") == 0) {
            printf("IMU collection is %s\n", 
                   is_imu_running(control) ? "running" : "stopped");
        }
    }
    
    return NULL;
}

// 控制函数
void start_imu_collection(ImuControl* control) {
    pthread_mutex_lock(&control->mutex);
    control->running = 1;
    pthread_mutex_unlock(&control->mutex);
}

void stop_imu_collection(ImuControl* control) {
    pthread_mutex_lock(&control->mutex);
    control->running = 0;
    pthread_mutex_unlock(&control->mutex);
}

int is_imu_running(ImuControl* control) {
    int status;
    pthread_mutex_lock(&control->mutex);
    status = control->running;
    pthread_mutex_unlock(&control->mutex);
    return status;
}