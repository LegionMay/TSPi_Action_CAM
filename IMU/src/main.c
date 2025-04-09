/* ---------- src/main.c ---------- */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/msg.h>
#include "imu_logger.h"

int main() {
    pthread_t read_thread, log_thread;
    imu_data_buffer buffer = {0};
    int msqid;

    // 初始化传感器
    if (!mpu6500_init("/dev/i2c-1", 0x68) || 
        !ak8963_init("/dev/i2c-1", 0x0D)) {
        fprintf(stderr, "传感器初始化失败\n");
        exit(EXIT_FAILURE);
    }

    // 创建消息队列
    if ((msqid = create_msg_queue()) == -1) {
        exit(EXIT_FAILURE);
    }

    // 初始化互斥锁
    if (pthread_mutex_init(&buffer.mutex, NULL) != 0) {
        perror("互斥锁初始化失败");
        exit(EXIT_FAILURE);
    }

    // 创建数据采集线程
    if (pthread_create(&read_thread, NULL, sensor_read_thread, &buffer) != 0) {
        perror("线程创建失败");
        exit(EXIT_FAILURE);
    }

    // 创建日志记录线程
    if (pthread_create(&log_thread, NULL, logging_thread, &msqid) != 0) {
        perror("线程创建失败");
        exit(EXIT_FAILURE);
    }

    // 等待线程结束
    pthread_join(read_thread, NULL);
    pthread_join(log_thread, NULL);

    // 清理资源
    msgctl(msqid, IPC_RMID, NULL);
    pthread_mutex_destroy(&buffer.mutex);
    
    printf("程序正常退出\n");
    return 0;
}