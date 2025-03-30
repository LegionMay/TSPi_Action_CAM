#include <lvgl/lvgl.h>
#include "../ui.h"

#include <stdio.h>
#include <sys/shm.h>
#include <string.h>

#define SHM_KEY 1234
#define SHM_SIZE 800 * 450 * 4

static void record_btn_event_cb(lv_event_t *e) {
    static uint8_t is_recording = 0;
    lv_obj_t *record_btn = lv_event_get_target(e);
    lv_obj_t *record_label = lv_obj_get_child(record_btn, 0);
    
    if (is_recording) {
        printf("Recording stopped\n");
        lv_label_set_text(record_label, LV_SYMBOL_PLAY);
        lv_obj_set_style_bg_color(record_btn, lv_color_hex(0xFF0000), LV_PART_MAIN);
    } else {
        printf("Recording started\n");
        lv_label_set_text(record_label, LV_SYMBOL_STOP);
        lv_obj_set_style_bg_color(record_btn, lv_color_hex(0xCC0000), LV_PART_MAIN);
    }
    is_recording = !is_recording;
}

static void menu_btn_event_cb(lv_event_t *e) {
    lv_screen_load(ui_Screen2);
}

static void update_video_preview(lv_timer_t *timer) {
    lv_obj_t *video_area = (lv_obj_t *)lv_timer_get_user_data(timer);
    static void *shm_ptr = NULL;

    if (!shm_ptr) {
        int shmid = shmget(SHM_KEY, SHM_SIZE, 0666);
        if (shmid == -1) return;
        shm_ptr = shmat(shmid, NULL, 0);
        if (shm_ptr == (void *)-1) return;
    }

    lv_image_set_src(video_area, shm_ptr);
    lv_obj_invalidate(video_area);
}

void ui_Screen1_screen_init(void) {
    lv_display_t *disp = lv_display_get_default();
    int32_t hor_res = lv_display_get_horizontal_resolution(disp);
    int32_t ver_res = lv_display_get_vertical_resolution(disp);

    // 创建自适应主容器
    ui_Screen1 = lv_obj_create(NULL);
    lv_obj_remove_flag(ui_Screen1, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(ui_Screen1, hor_res, ver_res);
    lv_obj_set_flex_flow(ui_Screen1, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_bg_color(ui_Screen1, lv_color_hex(0x000000), 0);

    // 视频预览容器（弹性布局）
    lv_obj_t *video_container = lv_obj_create(ui_Screen1);
    lv_obj_set_size(video_container, hor_res, ver_res - 30); // 为控制栏保留空间
    lv_obj_set_flex_align(video_container, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_bg_color(video_container, lv_color_hex(0x222222), 0);
    lv_obj_remove_style_all(video_container);

    // 视频预览区域（固定分辨率）
    lv_obj_t *video_area = lv_image_create(video_container);
    lv_obj_set_size(video_area, 800, 480); // 固定为 800x450
    lv_obj_set_pos(video_area, 0, 0);
    lv_obj_set_style_bg_color(video_area, lv_color_hex(0x222222), LV_PART_MAIN);
    lv_obj_set_style_border_width(video_area, 0, LV_PART_MAIN);

    // 控制栏（自动适配屏幕宽度）
    lv_obj_t *bottom_bar = lv_obj_create(ui_Screen1);
    lv_obj_set_size(bottom_bar, hor_res, 30);
    lv_obj_set_flex_flow(bottom_bar, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(bottom_bar, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_bg_opa(bottom_bar, LV_OPA_50, 0);
    lv_obj_set_style_bg_color(bottom_bar, lv_color_hex(0x000000), 0);

    // 品牌标识
    lv_obj_t *brand_label = lv_label_create(bottom_bar);
    lv_label_set_text(brand_label, "TSPiAction");
    lv_obj_set_style_text_font(brand_label, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(brand_label, lv_color_white(), 0);

    // 拍摄模式
    lv_obj_t *mode_label = lv_label_create(bottom_bar);
    lv_label_set_text(mode_label, "1080P30");
    lv_obj_set_style_text_font(mode_label, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(mode_label, lv_color_white(), 0);

    // 设置按钮
    lv_obj_t *menu_btn = lv_button_create(bottom_bar);
    lv_obj_set_size(menu_btn, 40, 30);
    lv_obj_add_event_cb(menu_btn, menu_btn_event_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *menu_icon = lv_label_create(menu_btn);
    lv_label_set_text(menu_icon, LV_SYMBOL_SETTINGS);
    lv_obj_center(menu_icon);
    lv_obj_set_style_text_font(menu_icon, &lv_font_montserrat_20, 0);

    // 录制按钮（独立于控制栏）
    lv_obj_t *record_btn = lv_button_create(ui_Screen1);
    lv_obj_set_size(record_btn, 70, 70);
    lv_obj_align(record_btn, LV_ALIGN_BOTTOM_MID, 0, -50); // 屏幕底部居中
    lv_obj_set_style_radius(record_btn, 35, 0);
    lv_obj_set_style_bg_color(record_btn, lv_color_hex(0xFF0000), 0);
    lv_obj_add_event_cb(record_btn, record_btn_event_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *record_icon = lv_label_create(record_btn);
    lv_label_set_text(record_icon, LV_SYMBOL_PLAY); // 初始为播放三角形
    lv_obj_center(record_icon);
    lv_obj_set_style_text_color(record_icon, lv_color_white(), 0);
    lv_obj_set_style_text_font(record_icon, &lv_font_montserrat_24, 0);

    // 视频更新定时器
    lv_timer_create(update_video_preview, 33, video_area);
    lv_screen_load(ui_Screen1);
}