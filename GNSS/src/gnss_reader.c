#include "gnss_reader.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <sys/stat.h>
#include <errno.h>

#define SERIAL_PORT "/dev/ttyS9"
#define BUFFER_SIZE 1024
#define JSON_PATH "/mnt/sdcard/gnss_data.json"
#define FIFO_PATH "/tmp/gnss_control_fifo"

// GNSS数据读取线程
void* gnss_reading_thread(void* arg) {
    GnssControl* control = (GnssControl*)arg;
    int serial_fd;
    char buffer[BUFFER_SIZE];
    GnssData gnss_data;
    
    while (1) {
        // 检查是否应该运行
        if (!is_gnss_running(control)) {
            sleep(1);  // 未运行时降低CPU使用率
            continue;
        }
        
        // 打开串口
        serial_fd = open(SERIAL_PORT, O_RDONLY | O_NOCTTY);
        if (serial_fd < 0) {
            perror("Failed to open serial port");
            sleep(5);  // 失败后等待一段时间再重试
            continue;
        }
        
        printf("GNSS data collection started\n");
        
        // 当运行标志为1时，持续读取数据
        while (is_gnss_running(control)) {
            ssize_t bytes_read = read(serial_fd, buffer, BUFFER_SIZE - 1);
            if (bytes_read > 0) {
                buffer[bytes_read] = '\0';
                
                // 解析NMEA数据
                if (parse_nmea(buffer, &gnss_data)) {
                    // 保存为JSON
                    save_to_json(&gnss_data, JSON_PATH);
                }
            }
            
            // 休眠2秒
            sleep(2);
        }
        
        close(serial_fd);
        printf("GNSS data collection stopped\n");
    }
    
    return NULL;
}

// 命令监听线程 - 监听其他进程的控制命令
void* command_listener_thread(void* arg) {
    GnssControl* control = (GnssControl*)arg;
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
            start_gnss_collection(control);
        } 
        else if (strcmp(cmd, "stop") == 0) {
            printf("Received command: stop\n");
            stop_gnss_collection(control);
        }
        else if (strcmp(cmd, "status") == 0) {
            printf("GNSS collection is %s\n", 
                   is_gnss_running(control) ? "running" : "stopped");
        }
    }
    
    return NULL;
}

// 开始GNSS数据采集
void start_gnss_collection(GnssControl* control) {
    pthread_mutex_lock(&control->mutex);
    control->running = 1;
    pthread_mutex_unlock(&control->mutex);
}

// 停止GNSS数据采集
void stop_gnss_collection(GnssControl* control) {
    pthread_mutex_lock(&control->mutex);
    control->running = 0;
    pthread_mutex_unlock(&control->mutex);
}

// 检查GNSS是否在运行
int is_gnss_running(GnssControl* control) {
    int status;
    pthread_mutex_lock(&control->mutex);
    status = control->running;
    pthread_mutex_unlock(&control->mutex);
    return status;
}

// 解析NMEA数据（实现重点关注GGA语句）
int parse_nmea(const char* buffer, GnssData* data) {
    char *line, *saveptr, *token;
    char temp[BUFFER_SIZE];
    
    // 拷贝缓冲区以便使用strtok_r
    strncpy(temp, buffer, BUFFER_SIZE - 1);
    
    // 按行处理NMEA数据
    line = strtok_r(temp, "\r\n", &saveptr);
    while (line != NULL) {
        // 检查是否是GGA语句 (定位数据)
        if (strncmp(line, "$GPGGA", 6) == 0 || strncmp(line, "$GNGGA", 6) == 0) {
            int i = 0;
            char *token_saveptr;
            token = strtok_r(line, ",", &token_saveptr);
            
            while (token != NULL) {
                i++;
                switch (i) {
                    case 2: // 时间
                        snprintf(data->timestamp, sizeof(data->timestamp), "%s", token);
                        break;
                    case 3: // 纬度
                        data->latitude = atof(token);
                        break;
                    case 5: // 经度
                        data->longitude = atof(token);
                        break;
                    case 8: // 卫星数
                        data->satellites = atoi(token);
                        break;
                    case 10: // 海拔高度
                        data->altitude = atof(token);
                        break;
                }
                token = strtok_r(NULL, ",", &token_saveptr);
            }
            return 1; // 成功解析
        }
        
        line = strtok_r(NULL, "\r\n", &saveptr);
    }
    
    return 0; // 没有找到有效数据
}

// 保存为JSON格式
void save_to_json(const GnssData* data, const char* filepath) {
    FILE* fp;
    time_t now;
    struct tm *t;
    char timestamp[64];
    
    // 获取当前时间作为文件名
    time(&now);
    t = localtime(&now);
    strftime(timestamp, sizeof(timestamp), "%Y%m%d_%H%M%S", t);
    
    // 追加模式打开文件
    fp = fopen(filepath, "a");
    if (fp == NULL) {
        perror("Failed to open file for writing");
        return;
    }
    
    // 写入JSON格式数据
    fprintf(fp, "{\n");
    fprintf(fp, "  \"timestamp\": \"%s\",\n", data->timestamp);
    fprintf(fp, "  \"latitude\": %.6f,\n", data->latitude);
    fprintf(fp, "  \"longitude\": %.6f,\n", data->longitude);
    fprintf(fp, "  \"satellites\": %d,\n", data->satellites);
    fprintf(fp, "  \"altitude\": %.2f,\n", data->altitude);
    fprintf(fp, "  \"record_time\": \"%s\"\n", timestamp);
    fprintf(fp, "}\n");
    
    fclose(fp);
}