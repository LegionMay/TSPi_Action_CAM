#include "../include/config.h"
#include <stdlib.h>

VideoConfig* load_config(void) {
    VideoConfig *config = (VideoConfig *)malloc(sizeof(VideoConfig));
    if (!config) {
        return NULL;
    }
    // 默认配置：1080p 30fps
    config->record_width = 1920;
    config->record_height = 1080;
    config->record_framerate = 30;
    config->preview_width = 800;    // 预览分辨率稍后计算
    config->preview_height = 480;
    // TODO: 可扩展为从共享内存或文件读取配置参数
    return config;
}

void calculate_preview_size(VideoConfig *config, int screen_width, int screen_height) {
    if (!config) return;
    float aspect_ratio = (float)config->record_width / config->record_height;
    if (screen_width / aspect_ratio <= screen_height) {
        config->preview_width = screen_width;
        config->preview_height = (int)(screen_width / aspect_ratio);
    } else {
        config->preview_height = screen_height;
        config->preview_width = (int)(screen_height * aspect_ratio);
    }
}

void free_config(VideoConfig *config) {
    if (config) {
        free(config);
    }
}