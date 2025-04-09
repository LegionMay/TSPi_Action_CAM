/*
 * @Author: LegionMay
 * @FilePath: /TSPi_Action/IMU/src/logger.c
 */
/* ---------- src/logger.c ---------- */
#include <stdio.h>
#include <sys/msg.h>
#include <time.h>
#include <string.h>
#include <errno.h>
#include "imu_logger.h"

// 创建消息队列
int create_msg_queue(void) {
    int msqid;
    
    // 尝试创建新队列
    if ((msqid = msgget(MSG_KEY, IPC_CREAT | IPC_EXCL | 0666)) == -1) {
        if (errno == EEXIST) { // 队列已存在
            msqid = msgget(MSG_KEY, 0);
            msgctl(msqid, IPC_RMID, NULL); // 删除旧队列
            msqid = msgget(MSG_KEY, IPC_CREAT | 0666);
        } else {
            perror("msgget失败");
            return -1;
        }
    }
    return msqid;
}

// 发送消息到队列
void send_to_msg_queue(int msqid, const imu_msg *msg) {
    if (msgsnd(msqid, msg, sizeof(imu_msg) - sizeof(long), 0) == -1) {
        perror("msgsnd失败");
        exit(EXIT_FAILURE);
    }
}

// 写入CSV文件
void write_to_csv(const fused_data *data) {
    static FILE *fp = NULL;
    char filename[64];
    time_t t = time(NULL);
    struct tm *tm = localtime(&t);

    if (fp == NULL || (t - file_start_time) >= 86400) {
        if (fp != NULL) fclose(fp);
        
        snprintf(filename, sizeof(filename), 
               "/mnt/sdcard/record_%04d%02d%02d_%02d%02d%02d.csv",
               tm->tm_year+1900, tm->tm_mon+1, tm->tm_mday,
               tm->tm_hour, tm->tm_min, tm->tm_sec);
        
        fp = fopen(filename, "w");
        if (!fp) {
            perror("文件打开失败");
            exit(EXIT_FAILURE);
        }
        fprintf(fp, "Timestamp,Roll(deg),Pitch(deg),Yaw(deg)\n");
        file_start_time = t;
    }

    fprintf(fp, "%ld.%09ld,%.2f,%.2f,%.2f\n", 
          data->ts.tv_sec, data->ts.tv_nsec,
          data->roll * 180/M_PI, 
          data->pitch * 180/M_PI,
          data->yaw * 180/M_PI);
    fflush(fp);
}

// 日志记录线程
void* logging_thread(void *arg) {
    int msqid = *(int*)arg;
    imu_data_buffer buffer;
    imu_msg msg;

    while (1) {
        pthread_mutex_lock(&buffer.mutex);
        if (buffer.count > 0) {
            // 从缓冲区获取数据
            fused_data data = buffer.filtered[buffer.head];
            
            // 填充消息结构
            msg.mtype = 1; // 消息类型
            msg.roll = data.roll;
            msg.pitch = data.pitch;
            msg.yaw = data.yaw;
            msg.ts = data.ts;

            // 写入文件和消息队列
            write_to_csv(&data);
            send_to_msg_queue(msqid, &msg);

            // 更新缓冲区索引
            buffer.head = (buffer.head + 1) % BUFFER_SIZE;
            buffer.count--;
        }
        pthread_mutex_unlock(&buffer.mutex);
        usleep(10000); // 10ms休眠
    }
    return NULL;
}