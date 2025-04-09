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
#define UI_FIFO_PATH "/tmp/gnss_ui_fifo"  // 新增UI通信管道路径
#define RECORD_INTERVAL 10  // 记录间隔，单位为秒

// 全局GNSS数据，用于线程间共享
static GnssData current_gnss_data;
static pthread_mutex_t data_mutex = PTHREAD_MUTEX_INITIALIZER;

// 格式化时间为字符串
char* format_time(time_t time_value) {
    static char buffer[64];
    struct tm *tm_info = localtime(&time_value);
    strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", tm_info);
    return buffer;
}

// 修改GNSS初始化和数据收集函数

void* gnss_reading_thread(void* arg) {
    GnssControl* control = (GnssControl*)arg;
    int serial_fd;
    char buffer[BUFFER_SIZE];
    GnssData gnss_data;
    time_t last_record_time = 0;
    char json_filename[256];
    FILE *json_file = NULL;
    int is_first_entry = 1;
    
    // 确保目录存在
    system("mkdir -p /mnt/sdcard");
    
    // 初始化默认GNSS数据
    memset(&gnss_data, 0, sizeof(GnssData));
    strcpy(gnss_data.timestamp, "UNKNOWN");
    gnss_data.latitude = 0.0;
    gnss_data.longitude = 0.0;
    gnss_data.altitude = 0.0;
    gnss_data.satellites = 0;
    gnss_data.record_time = time(NULL);
    
    // 创建初始JSON文件
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    snprintf(json_filename, sizeof(json_filename), "/mnt/sdcard/gnss_%04d%02d%02d_%02d%02d%02d.json",
             t->tm_year + 1900, t->tm_mon + 1, t->tm_mday, 
             t->tm_hour, t->tm_min, t->tm_sec);
    
    // 立即写入一个有效的JSON结构，即使没有数据
    json_file = fopen(json_filename, "w");
    if (json_file) {
        fprintf(json_file, "[\n");
        // 立即写入一条初始记录，确保文件不为空
        fprintf(json_file, 
                "  {\n"
                "    \"timestamp\": \"%s\",\n"
                "    \"coords\": [%.6f, %.6f],\n"
                "    \"altitude\": %.1f,\n"
                "    \"satellites\": %d,\n"
                "    \"record_time\": \"%s\",\n"
                "    \"status\": \"initial\"\n"
                "  }\n",
                gnss_data.timestamp,
                gnss_data.latitude, gnss_data.longitude,
                gnss_data.altitude,
                gnss_data.satellites,
                format_time(gnss_data.record_time));
        fclose(json_file);
        printf("Created GNSS JSON file with initial data: %s\n", json_filename);
        is_first_entry = 0;
    } else {
        perror("Failed to create GNSS JSON file");
        // 尝试在/tmp创建文件
        snprintf(json_filename, sizeof(json_filename), "/tmp/gnss_%04d%02d%02d_%02d%02d%02d.json",
                t->tm_year + 1900, t->tm_mon + 1, t->tm_mday, 
                t->tm_hour, t->tm_min, t->tm_sec);
        json_file = fopen(json_filename, "w");
        if (json_file) {
            fprintf(json_file, "[\n");
            fprintf(json_file, 
                    "  {\n"
                    "    \"timestamp\": \"%s\",\n"
                    "    \"coords\": [%.6f, %.6f],\n"
                    "    \"altitude\": %.1f,\n"
                    "    \"satellites\": %d,\n"
                    "    \"record_time\": \"%s\",\n"
                    "    \"status\": \"initial\"\n"
                    "  }\n",
                    gnss_data.timestamp,
                    gnss_data.latitude, gnss_data.longitude,
                    gnss_data.altitude,
                    gnss_data.satellites,
                    format_time(gnss_data.record_time));
            fclose(json_file);
            printf("Created fallback GNSS JSON file: %s\n", json_filename);
            is_first_entry = 0;
        }
    }
    
    // 主循环开始
    while (1) {
        // 检查是否应该运行
        if (!is_gnss_running(control)) {
            sleep(1);
            continue;
        }
        
        // 打开串口
        serial_fd = open(SERIAL_PORT, O_RDONLY | O_NOCTTY);
        if (serial_fd < 0) {
            perror("Failed to open serial port");
            
            // 直接记录一条没有数据的记录
            now = time(NULL);
            if (difftime(now, last_record_time) >= 10) {
                json_file = fopen(json_filename, "a");
                if (json_file) {
                    fprintf(json_file, ",\n");
                    fprintf(json_file, 
                            "  {\n"
                            "    \"timestamp\": \"NO_DEVICE\",\n"
                            "    \"coords\": [0.0, 0.0],\n"
                            "    \"altitude\": 0.0,\n"
                            "    \"satellites\": 0,\n"
                            "    \"record_time\": \"%s\",\n"
                            "    \"status\": \"no_device\"\n"
                            "  }",
                            format_time(now));
                    fclose(json_file);
                    last_record_time = now;
                    
                    // 发送卫星数到UI
                    int fifo_fd = open(UI_FIFO_PATH, O_WRONLY | O_NONBLOCK);
                    if (fifo_fd >= 0) {
                        write(fifo_fd, "0", 1);
                        close(fifo_fd);
                    }
                    printf("GNSS: Wrote no-device record\n");
                }
            }
            
            sleep(5);
            continue;
        }
        
        printf("GNSS data collection started\n");
        last_record_time = time(NULL) - 15; // 确保第一次运行就记录数据
        
        // 当运行标志为1时，持续读取数据
        while (is_gnss_running(control)) {
            ssize_t bytes_read = read(serial_fd, buffer, BUFFER_SIZE - 1);
            if (bytes_read > 0) {
                buffer[bytes_read] = '\0';
                
                // 解析NMEA数据
                if (parse_nmea(buffer, &gnss_data)) {
                    now = time(NULL);
                    gnss_data.record_time = now;
                    
                    // 检查是否达到记录间隔（10秒）
                    if (difftime(now, last_record_time) >= 10) {
                        // 追加到JSON文件
                        json_file = fopen(json_filename, "a");
                        if (json_file) {
                            if (!is_first_entry) {
                                fprintf(json_file, ",\n");
                            } else {
                                is_first_entry = 0;
                            }
                            
                            fprintf(json_file, 
                                    "  {\n"
                                    "    \"timestamp\": \"%s\",\n"
                                    "    \"coords\": [%.6f, %.6f],\n"
                                    "    \"altitude\": %.1f,\n"
                                    "    \"satellites\": %d,\n"
                                    "    \"record_time\": \"%s\",\n"
                                    "    \"status\": \"active\"\n"
                                    "  }",
                                    gnss_data.timestamp,
                                    gnss_data.latitude, gnss_data.longitude,
                                    gnss_data.altitude,
                                    gnss_data.satellites,
                                    format_time(gnss_data.record_time));
                            fclose(json_file);
                            last_record_time = now;
                            
                            printf("GNSS data recorded: %d satellites\n", gnss_data.satellites);
                            
                            // 发送卫星数到UI
                            int fifo_fd = open(UI_FIFO_PATH, O_WRONLY | O_NONBLOCK);
                            if (fifo_fd >= 0) {
                                char sat_str[16];
                                snprintf(sat_str, sizeof(sat_str), "%d", gnss_data.satellites);
                                write(fifo_fd, sat_str, strlen(sat_str));
                                close(fifo_fd);
                            }
                        } else {
                            perror("Failed to open GNSS JSON file for appending");
                        }
                    }
                }
            } else if (bytes_read < 0) {
                perror("Error reading from serial port");
            }
            
            usleep(500000);  // 0.5秒
        }
        
        close(serial_fd);
        
        // 正确完成JSON文件
        json_file = fopen(json_filename, "a");
        if (json_file) {
            fprintf(json_file, "\n]\n"); // 结束JSON数组
            fclose(json_file);
            printf("GNSS JSON file closed properly\n");
            
            // 为下一次启动准备新文件名和标志
            is_first_entry = 1;
            
            now = time(NULL);
            t = localtime(&now);
            snprintf(json_filename, sizeof(json_filename), "/mnt/sdcard/gnss_%04d%02d%02d_%02d%02d%02d.json",
                     t->tm_year + 1900, t->tm_mon + 1, t->tm_mday, 
                     t->tm_hour, t->tm_min, t->tm_sec);
            
            // 创建新文件并添加初始记录
            json_file = fopen(json_filename, "w");
            if (json_file) {
                fprintf(json_file, "[\n");
                fprintf(json_file, 
                        "  {\n"
                        "    \"timestamp\": \"%s\",\n"
                        "    \"coords\": [%.6f, %.6f],\n"
                        "    \"altitude\": %.1f,\n"
                        "    \"satellites\": %d,\n"
                        "    \"record_time\": \"%s\",\n"
                        "    \"status\": \"initial\"\n"
                        "  }\n",
                        gnss_data.timestamp,
                        gnss_data.latitude, gnss_data.longitude,
                        gnss_data.altitude,
                        gnss_data.satellites,
                        format_time(gnss_data.record_time));
                fclose(json_file);
                is_first_entry = 0;
            }
        }
        
        printf("GNSS data collection stopped\n");
    }
    
    return NULL;
}

// UI更新线程 - 每10秒发送卫星数到UI
void* ui_update_thread(void* arg) {
    GnssControl* control = (GnssControl*)arg;
    int fifo_fd;
    time_t last_update_time = 0;
    char buffer[32];
    
    // 创建UI通信管道
    unlink(UI_FIFO_PATH);
    if (mkfifo(UI_FIFO_PATH, 0666) < 0) {
        perror("Failed to create UI FIFO");
        return NULL;
    }
    
    printf("UI update thread started. FIFO: %s\n", UI_FIFO_PATH);
    
    while (1) {
        // 即使GNSS没有运行，也尝试更新UI（显示0颗卫星）
        time_t now = time(NULL);
        
        // 每10秒更新一次UI
        if (difftime(now, last_update_time) >= RECORD_INTERVAL) {
            // 打开FIFO用于写入
            fifo_fd = open(UI_FIFO_PATH, O_WRONLY | O_NONBLOCK);
            if (fifo_fd >= 0) {
                int satellites = 0;
                
                // 如果GNSS在运行，使用当前数据
                if (is_gnss_running(control)) {
                    pthread_mutex_lock(&data_mutex);
                    satellites = current_gnss_data.satellites;
                    pthread_mutex_unlock(&data_mutex);
                }
                
                // 发送卫星数到UI
                snprintf(buffer, sizeof(buffer), "%d", satellites);
                write(fifo_fd, buffer, strlen(buffer));
                close(fifo_fd);
                
                printf("Sent satellites count to UI: %d\n", satellites);
                last_update_time = now;
            } else if (errno != ENXIO) {  // 忽略"没有进程在读取"的错误
                perror("Failed to open UI FIFO for writing");
            }
        }
        
        // 休眠1秒
        sleep(1);
    }
    
    return NULL;
}

// 命令监听线程 - 监听其他进程的控制命令
void* command_listener_thread(void* arg) {
    GnssControl* control = (GnssControl*)arg;
    int fifo_fd;
    char cmd[20];
    
    // 创建命名管道
    unlink(FIFO_PATH);
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

// 保存为JSON格式 - 使用简洁英文标签
void save_to_json(const GnssData* data, const char* filepath) {
    FILE* fp;
    char filename[256];
    
    // 创建文件名，使用与录制视频相同的格式
    time_t now = data->record_time;
    struct tm *t = localtime(&now);
    snprintf(filename, sizeof(filename), "/mnt/sdcard/gnss_%04d%02d%02d_%02d%02d%02d.json",
             t->tm_year + 1900, t->tm_mon + 1, t->tm_mday, 
             t->tm_hour, t->tm_min, t->tm_sec);
    
    // 创建或追加模式打开文件
    fp = fopen(filename, "w");
    if (fp == NULL) {
        perror("Failed to open file for writing");
        return;
    }
    
    // 写入JSON格式数据 - 使用简洁的英文标签
    fprintf(fp, "{\n");
    fprintf(fp, "  \"time\": \"%s\",\n", format_time(data->record_time));
    fprintf(fp, "  \"coords\": \"%f, %f\",\n", data->latitude, data->longitude);
    fprintf(fp, "  \"alt\": \"%.1fm\",\n", data->altitude);
    fprintf(fp, "  \"sats\": %d\n", data->satellites);
    fprintf(fp, "}\n");
    
    fclose(fp);
    printf("GNSS data saved to %s\n", filename);
}