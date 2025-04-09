#ifndef GNSS_READER_H
#define GNSS_READER_H

#include <pthread.h>
#include <time.h>

// 数据结构定义
typedef struct {
    double latitude;
    double longitude;
    char timestamp[20];
    int satellites;
    double altitude;
    time_t record_time;  // 添加记录时间
} GnssData;

// 控制结构体
typedef struct {
    int running;            // 运行标志
    pthread_mutex_t mutex;  // 互斥锁保护状态
} GnssControl;

// 函数声明
void* gnss_reading_thread(void* arg);
void* command_listener_thread(void* arg);
void* ui_update_thread(void* arg);  // 新增UI更新线程
int parse_nmea(const char* buffer, GnssData* data);
void save_to_json(const GnssData* data, const char* filepath);
void start_gnss_collection(GnssControl* control);
void stop_gnss_collection(GnssControl* control);
int is_gnss_running(GnssControl* control);
char* format_time(time_t time_value);  // 新增时间格式化函数

#endif