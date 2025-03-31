/*
 * @Author: LegionMay
 * @FilePath: /TSPi_Action/Video/src/main.c
 */

#include "pipeline.h"
#include "shm_utils.h"
#include "config.h"
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char *argv[]) {
    GstElement *pipeline = NULL;
    VideoConfig *config = NULL;
    int shmid = -1;

    // 初始化GStreamer
    gst_init(&argc, &argv);

    // 加载配置
    config = load_config();
    if (!config) {
        fprintf(stderr, "Failed to load config.\n");
        return -1;
    }

    // 计算预览分辨率（屏幕800x480）
    calculate_preview_size(config, 800, 480);

    // 创建共享内存
    shmid = create_shm();
    if (shmid == -1) {
        free_config(config);
        return -1;
    }
    shm_ptr = attach_shm(shmid);
    if (!shm_ptr) {
        destroy_shm(shmid);
        free_config(config);
        return -1;
    }

    // 创建并启动管道
    pipeline = create_pipeline(config);
    if (!pipeline) {
        detach_shm(shm_ptr);
        destroy_shm(shmid);
        free_config(config);
        return -1;
    }
    start_pipeline(pipeline);

    // 监听管道消息
    monitor_pipeline(pipeline);

    // 清理资源
    cleanup_pipeline(pipeline);
    detach_shm(shm_ptr);
    destroy_shm(shmid);
    free_config(config);

    return 0;
}