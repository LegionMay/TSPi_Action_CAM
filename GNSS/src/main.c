/*
 * @Author: LegionMay
 * @FilePath: /TSPi_Action/GNSS/src/main.c
 */
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <signal.h>
#include "gnss_reader.h"

GnssControl g_control;

// 信号处理函数
void signal_handler(int sig) {
    if (sig == SIGTERM || sig == SIGINT) {
        printf("Terminating GNSS collector...\n");
        exit(0);
    }
}

int main() {
    pthread_t gnss_thread, cmd_thread, ui_thread;
    int ret;
    
    // 初始化控制结构
    g_control.running = 0;  // 初始状态为停止
    pthread_mutex_init(&g_control.mutex, NULL);
    
    // 注册信号处理
    signal(SIGTERM, signal_handler);
    signal(SIGINT, signal_handler);
    
    printf("Starting GNSS data collector...\n");
    
    // 创建命令监听线程
    ret = pthread_create(&cmd_thread, NULL, command_listener_thread, &g_control);
    if (ret != 0) {
        perror("Failed to create command thread");
        return 1;
    }
    
    // 创建GNSS数据采集线程
    ret = pthread_create(&gnss_thread, NULL, gnss_reading_thread, &g_control);
    if (ret != 0) {
        perror("Failed to create GNSS thread");
        return 1;
    }
    
    // 创建UI更新线程
    ret = pthread_create(&ui_thread, NULL, ui_update_thread, &g_control);
    if (ret != 0) {
        perror("Failed to create UI update thread");
        return 1;
    }
    
    // 主线程等待
    while (1) {
        sleep(60);
    }
    
    // 释放资源
    pthread_mutex_destroy(&g_control.mutex);
    return 0;
}