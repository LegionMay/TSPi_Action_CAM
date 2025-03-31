#ifndef SD_STATUS_H
#define SD_STATUS_H

#include <lvgl/lvgl.h>

// SD卡状态结构体
typedef struct {
    uint64_t total_mb;
    uint64_t free_mb;
    uint8_t used_percent;
    lv_obj_t *status_label;
} SD_Status;

// 初始化SD监控模块
void sd_status_init(lv_obj_t *parent);

// 更新显示回调函数类型
typedef void (*update_callback_t)(lv_obj_t *label, uint8_t percent);

// 设置自定义显示格式回调
void sd_set_display_callback(update_callback_t cb);

#endif