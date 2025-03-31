#include "sd_status.h"
#include <sys/statvfs.h>

static SD_Status sd;
static update_callback_t user_callback = NULL;

static void update_status_display() {
    static char buf[16];
    
    if (sd.total_mb == 0) {
        snprintf(buf, sizeof(buf), "NO SD");
    } else if (user_callback) {
        user_callback(sd.status_label, sd.used_percent);
    } else {
        snprintf(buf, sizeof(buf), "%d%%", sd.used_percent);
        lv_label_set_text(sd.status_label, buf);
    }
}

static void get_storage_info() {
    struct statvfs vfs;
    if (statvfs("/mnt/sdcard", &vfs) == 0) {
        uint64_t blocksize = vfs.f_bsize;
        sd.total_mb = (blocksize * vfs.f_blocks) >> 20;
        sd.free_mb = (blocksize * vfs.f_bfree) >> 20;
        sd.used_percent = (sd.total_mb > 0) ? 
            100 - (sd.free_mb * 100 / sd.total_mb) : 0;
    } else {
        sd.total_mb = sd.free_mb = sd.used_percent = 0;
    }
}

static void storage_update_task(lv_timer_t *timer) {
    get_storage_info();
    update_status_display();
}

void sd_status_init(lv_obj_t *parent) {
    // 创建状态显示标签
    sd.status_label = lv_label_create(parent);
    lv_obj_set_style_text_font(sd.status_label, &lv_font_montserrat_14, 0);
    
    // 初始获取信息
    get_storage_info();
    
    // 创建更新定时器
    lv_timer_create(storage_update_task, 2000, NULL);
}

void sd_set_display_callback(update_callback_t cb) {
    user_callback = cb;
}