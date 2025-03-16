//main.c
#include <unistd.h>
#include <pthread.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>

#include "lvgl/lvgl.h"
#include "lvgl/demos/lv_demos.h"
#include "lvgl/src/core/lv_global.h"
#include "ui/ui.h"

#include <gst/gst.h>

#if LV_USE_WAYLAND
#include "backends/interface.h"
#endif

extern GstElement *pipeline;
extern GstBus *bus;

uint16_t window_width = 800;  // 默认宽度
uint16_t window_height = 480; // 默认高度
bool fullscreen = true;
bool maximize = true;

static void configure_simulator(int argc, char **argv);
static const char *getenv_default(const char *name, const char *dflt);

// 添加定期刷新的定时器回调函数
static void display_refresh_timer_cb(lv_timer_t * timer) {
    lv_display_t * disp = timer->user_data;
    lv_display_send_event(disp, LV_EVENT_REFRESH, NULL);
    lv_refr_now(disp);
}

#if LV_USE_EVDEV
static void indev_deleted_cb(lv_event_t * e)
{
    if(LV_GLOBAL_DEFAULT()->deinit_in_progress) return;
    lv_obj_t * cursor_obj = lv_event_get_user_data(e);
    lv_obj_delete(cursor_obj);
}

static void discovery_cb(lv_indev_t * indev, lv_evdev_type_t type, void * user_data)
{
    LV_LOG_USER("new '%s' device discovered", type == LV_EVDEV_TYPE_REL ? "REL" :
                                              type == LV_EVDEV_TYPE_ABS ? "ABS" :
                                              type == LV_EVDEV_TYPE_KEY ? "KEY" :
                                              "unknown");
    lv_display_t * disp = user_data;
    lv_indev_set_display(indev, disp);

    if(type == LV_EVDEV_TYPE_REL) {
        LV_IMAGE_DECLARE(mouse_cursor_icon);
        lv_obj_t * cursor_obj = lv_image_create(lv_display_get_screen_active(disp));
        lv_image_set_src(cursor_obj, &mouse_cursor_icon);
        lv_indev_set_cursor(indev, cursor_obj);
        lv_indev_add_event_cb(indev, indev_deleted_cb, LV_EVENT_DELETE, cursor_obj);
    }
}

static void lv_linux_init_input_pointer(lv_display_t *disp)
{
    const char *input_device = getenv("LV_LINUX_EVDEV_POINTER_DEVICE");
    if (input_device == NULL) {
        LV_LOG_USER("LV_LINUX_EVDEV_POINTER_DEVICE not set. Using evdev automatic discovery.");
        lv_evdev_discovery_start(discovery_cb, disp);
        return;
    }
    lv_indev_t *touch = lv_evdev_create(LV_INDEV_TYPE_POINTER, input_device);
    lv_indev_set_display(touch, disp);

    LV_IMAGE_DECLARE(mouse_cursor_icon);
    lv_obj_t * cursor_obj = lv_image_create(lv_display_get_screen_active(disp));
    lv_image_set_src(cursor_obj, &mouse_cursor_icon);
    lv_indev_set_cursor(touch, cursor_obj);
}
#endif

#if LV_USE_LINUX_FBDEV
static void lv_linux_disp_init(void)
{
    const char *device = getenv_default("LV_LINUX_FBDEV_DEVICE", "/dev/fb0");
    lv_display_t * disp = lv_linux_fbdev_create();
    

    // 设置横屏
    lv_display_set_rotation(disp, LV_DISPLAY_ROTATION_90);
    
    // 设置分辨率（确保横屏）
    lv_display_set_resolution(disp, window_width, window_height);
    
    // 添加定期刷新定时器，每秒刷新一次，防止画面消失
    lv_timer_create(display_refresh_timer_cb, 1000, disp);
    
#if LV_USE_EVDEV
    lv_linux_init_input_pointer(disp);

     // 添加转换函数以修复触摸坐标
    // lv_indev_t * indev = lv_indev_get_next(NULL);
    // while(indev) {
    //     if(lv_indev_get_type(indev) == LV_INDEV_TYPE_POINTER) {
    //         // 设置坐标变换 - 告诉LVGL输入设备应适应显示旋转
    //         lv_indev_set_display(indev, disp);
    //     }
    //     indev = lv_indev_get_next(indev);
    // }
#endif
    lv_linux_fbdev_set_file(disp, device);
}
#elif LV_USE_LINUX_DRM
static void lv_linux_disp_init(void)
{
    const char *device = getenv_default("LV_LINUX_DRM_CARD", "/dev/dri/card0");
    lv_display_t * disp = lv_linux_drm_create();
    
    // 设置分辨率（确保横屏）
    lv_display_set_resolution(disp, window_width, window_height);
    
    // 添加定期刷新定时器，每秒刷新一次，防止画面消失
    lv_timer_create(display_refresh_timer_cb, 1000, disp);
    
#if LV_USE_EVDEV
    lv_linux_init_input_pointer(disp);
#endif
    lv_linux_drm_set_file(disp, device, -1);
}
#elif LV_USE_SDL
static void lv_linux_disp_init(void)
{
    lv_sdl_window_create(window_width, window_height);
}
#elif LV_USE_WAYLAND
    /* see backend/wayland.c */
#else
#error Unsupported configuration
#endif

#if LV_USE_WAYLAND == 0
void lv_linux_run_loop(void)
{
    uint32_t idle_time;
    while(1) {
        idle_time = lv_timer_handler();
        usleep(idle_time * 1000);
    }
}
#endif

static const char *getenv_default(const char *name, const char *dflt)
{
    return getenv(name) ? : dflt;
}

static void configure_simulator(int argc, char **argv)
{
    int opt = 0;
    fullscreen = maximize = true;
    window_width = atoi(getenv("LV_SIM_WINDOW_WIDTH") ? : "800");
    window_height = atoi(getenv("LV_SIM_WINDOW_HEIGHT") ? : "480");

    while ((opt = getopt(argc, argv, "fmw:h:")) != -1) {
        switch (opt) {
        case 'f':
            fullscreen = true;
            if (LV_USE_WAYLAND == 0) {
                fprintf(stderr, "SDL driver doesn't support fullscreen mode on start\n");
                exit(1);
            }
            break;
        case 'm':
            maximize = true;
            if (LV_USE_WAYLAND == 0) {
                fprintf(stderr, "SDL driver doesn't support maximized mode on start\n");
                exit(1);
            }
            break;
        case 'w':
            window_width = atoi(optarg);
            break;
        case 'h':
            window_height = atoi(optarg);
            break;
        case ':':
            fprintf(stderr, "Option -%c requires an argument.\n", optopt);
            exit(1);
        case '?':
            fprintf(stderr, "Unknown option -%c.\n", optopt);
            exit(1);
        }
    }
}

int main(int argc, char **argv)
{
    configure_simulator(argc, argv);

    /* 初始化LVGL */
    lv_init();

    /* 初始化显示后端 */
    lv_linux_disp_init();


    /* 初始化UI */
    my_ui_init();

    /* 运行LVGL事件循环 */
    lv_linux_run_loop();

    if (pipeline) {
        gst_element_set_state(pipeline, GST_STATE_NULL);
        gst_object_unref(pipeline);
    }
    if (bus) {
        gst_object_unref(bus);
    }
    gst_deinit();

    return 0;
}