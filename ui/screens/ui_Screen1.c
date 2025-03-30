#include <lvgl/lvgl.h>
#include "../ui.h"

#include <stdio.h>
#include <sys/shm.h>
#include <string.h>

// 共享内存配置（与 shm_utils.h 一致）
#define SHM_KEY 1234
#define SHM_SIZE 800 * 450 * 4  // 800x450 RGBA

// 录制按钮事件回调
static void record_btn_event_cb(lv_event_t *e) {
    printf("Recording started\n");
    // TODO: 告知视频处理进程获取视频流并保存的逻辑
}

// 菜单按钮事件回调
static void menu_btn_event_cb(lv_event_t *e) {
    // 切换到菜单界面
    lv_screen_load(ui_Screen2);
}

// 视频预览更新函数
static void update_video_preview(lv_timer_t *timer) {
    lv_obj_t *video_area = (lv_obj_t *)lv_timer_get_user_data(timer);
    static void *shm_ptr = NULL;

    // 初次附加共享内存
    if (!shm_ptr) {
        int shmid = shmget(SHM_KEY, SHM_SIZE, 0666);
        if (shmid == -1) {
            perror("shmget failed");
            return;
        }
        shm_ptr = shmat(shmid, NULL, 0);
        if (shm_ptr == (void *)-1) {
            perror("shmat failed");
            return;
        }
    }

    // 将共享内存中的视频帧设置为图像源
    lv_image_set_src(video_area, shm_ptr);
    lv_obj_invalidate(video_area); // 强制重绘
}

void ui_Screen1_screen_init(void) {
    // 获取屏幕尺寸
    lv_display_t *disp = lv_display_get_default();
    int32_t hor_res = lv_display_get_horizontal_resolution(disp);
    int32_t ver_res = lv_display_get_vertical_resolution(disp);
    LV_LOG_USER("DISPLAY: %dx%d", hor_res, ver_res);

    // 创建全屏主界面
    ui_Screen1 = lv_obj_create(NULL);
    lv_obj_set_style_pad_all(ui_Screen1, 0, LV_PART_MAIN);
    lv_obj_set_style_margin_all(ui_Screen1, 0, LV_PART_MAIN);
    lv_obj_set_style_border_width(ui_Screen1, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(ui_Screen1, 0, LV_PART_MAIN);
    lv_obj_set_size(ui_Screen1, hor_res, ver_res);
    lv_obj_remove_flag(ui_Screen1, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(ui_Screen1, lv_color_hex(0x000000), LV_PART_MAIN);

    // 创建视频容器
    lv_obj_t *video_container = lv_obj_create(ui_Screen1);
    lv_obj_remove_style_all(video_container);
    lv_obj_set_size(video_container, hor_res, ver_res);
    lv_obj_set_pos(video_container, 0, 0);
    lv_obj_set_style_bg_color(video_container, lv_color_hex(0x222222), LV_PART_MAIN);

    // 创建视频预览区域（800x450）
    lv_obj_t *video_area = lv_image_create(video_container);
    lv_obj_set_size(video_area, 800, 480); // 固定为 800x450
    lv_obj_set_pos(video_area, 0, 0);
    lv_obj_set_style_bg_color(video_area, lv_color_hex(0x222222), LV_PART_MAIN);
    lv_obj_set_style_border_width(video_area, 0, LV_PART_MAIN);

    // 创建定时器以更新视频预览
    lv_timer_t *timer = lv_timer_create(update_video_preview, 33, video_area); // 约30fps
    lv_timer_set_user_data(timer, video_area);

    // 底部半透明控制栏
    lv_obj_t *bottom_bar = lv_obj_create(video_container);
    lv_obj_remove_style_all(bottom_bar);
    lv_obj_set_size(bottom_bar, hor_res, 30);
    lv_obj_set_pos(bottom_bar, 0, ver_res - 30);
    lv_obj_set_style_bg_color(bottom_bar, lv_color_hex(0x000000), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(bottom_bar, LV_OPA_50, LV_PART_MAIN);

    // "ActionCAM" 文本
    lv_obj_t *brand_label = lv_label_create(bottom_bar);
    lv_label_set_text(brand_label, "ActionCAM");
    lv_obj_align(brand_label, LV_ALIGN_LEFT_MID, 10, 0);
    lv_obj_set_style_text_font(brand_label, &lv_font_montserrat_20, LV_PART_MAIN);
    lv_obj_set_style_text_color(brand_label, lv_color_hex(0xFFFFFF), LV_PART_MAIN);

    // 当前拍摄模式显示
    lv_obj_t *mode_label = lv_label_create(bottom_bar);
    lv_label_set_text(mode_label, "1080P60"); // 示例模式，可动态更新
    lv_obj_align(mode_label, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_text_font(mode_label, &lv_font_montserrat_16, LV_PART_MAIN);
    lv_obj_set_style_text_color(mode_label, lv_color_hex(0xFFFFFF), LV_PART_MAIN);

    // 菜单按钮
    lv_obj_t *menu_btn = lv_button_create(bottom_bar);
    lv_obj_set_size(menu_btn, 60, 30);
    lv_obj_align(menu_btn, LV_ALIGN_RIGHT_MID, -10, 0);
    lv_obj_set_style_bg_color(menu_btn, lv_color_hex(0x555555), LV_PART_MAIN);
    lv_obj_add_event_cb(menu_btn, menu_btn_event_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *menu_label = lv_label_create(menu_btn);
    lv_label_set_text(menu_label, "Menu");
    lv_obj_center(menu_label);
    lv_obj_set_style_text_color(menu_label, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_set_style_text_font(menu_label, &lv_font_montserrat_16, LV_PART_MAIN);

    // 录制按钮
    lv_obj_t *record_btn = lv_button_create(video_container);
    lv_obj_set_size(record_btn, 60, 60);
    lv_obj_align(record_btn, LV_ALIGN_RIGHT_MID, -20, -15);
    lv_obj_set_style_radius(record_btn, 30, LV_PART_MAIN);
    lv_obj_set_style_bg_color(record_btn, lv_color_hex(0xFF0000), LV_PART_MAIN);
    lv_obj_add_event_cb(record_btn, record_btn_event_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *record_label = lv_label_create(record_btn);
    lv_label_set_text(record_label, "REC");
    lv_obj_center(record_label);
    lv_obj_set_style_text_color(record_label, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_set_style_text_font(record_label, &lv_font_montserrat_16, LV_PART_MAIN);

    // 加载主界面
    lv_screen_load(ui_Screen1);
}