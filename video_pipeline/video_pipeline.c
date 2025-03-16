#include "video_pipeline.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <errno.h>

GstElement *pipeline = NULL;
GstElement *source = NULL;
GstElement *capsfilter = NULL;
GstElement *queue = NULL;
GstElement *convert = NULL;
GstElement *scale = NULL;
GstElement *videocrop = NULL;
GstElement *kmssink = NULL;
GstBus *bus = NULL;

MppCtx mpp_ctx = NULL;
MppApi *mpi = NULL;
FILE *output_file = NULL;
gboolean is_recording = FALSE;

// 视频区域在UI上的位置和大小
int video_x = 0;
int video_y = 0;
int video_width = 0;
int video_height = 0;

// 初始化视频管道，使用kmssink直接渲染到特定区域
void video_pipeline_init(int x, int y, int width, int height) {
    // 保存视频区域信息
    video_x = x;
    video_y = y;
    video_width = width;
    video_height = height;
    
    printf("Video display area: x=%d, y=%d, %dx%d\n", x, y, width, height);

    // 初始化GStreamer
    gst_init(NULL, NULL);

    // 创建预览管道
    pipeline = gst_pipeline_new("kmssink-pipeline");
    source = gst_element_factory_make("v4l2src", "source");
    capsfilter = gst_element_factory_make("capsfilter", "capsfilter");
    queue = gst_element_factory_make("queue", "queue");
    convert = gst_element_factory_make("videoconvert", "convert");
    scale = gst_element_factory_make("videoscale", "scale");
    videocrop = gst_element_factory_make("videocrop", "crop");
    kmssink = gst_element_factory_make("kmssink", "kmssink");

    if (!pipeline || !source || !capsfilter || !queue || 
        !convert || !scale || !videocrop || !kmssink) {
        fprintf(stderr, "Failed to create GStreamer elements\n");
        return;
    }

    // 配置source
    g_object_set(G_OBJECT(source), 
                "device", "/dev/video0", 
                NULL);

    // 配置capsfilter - 设置摄像头输入格式
    GstCaps *caps = gst_caps_new_simple("video/x-raw",
                                        "format", G_TYPE_STRING, "NV12",
                                        "width", G_TYPE_INT, CAM_WIDTH,
                                        "height", G_TYPE_INT, CAM_HEIGHT,
                                        "framerate", GST_TYPE_FRACTION, 30, 1,
                                        NULL);
    g_object_set(G_OBJECT(capsfilter), "caps", caps, NULL);
    gst_caps_unref(caps);

    // 配置视频裁剪
    g_object_set(G_OBJECT(videocrop),
                "left", 0,
                "right", 0,
                "top", 0,
                "bottom", 0,
                NULL);

    // 配置kmssink
    g_object_set(G_OBJECT(kmssink),
                "driver-name", "rockchip", // 根据具体平台修改
                "connector-id", 0,         // 默认连接器，可能需要调整
                "plane-id", 0,             // 默认图层，可能需要调整
                "render-rectangle", video_x, video_y, video_width, video_height, // 渲染区域
                "sync", FALSE,             // 禁用同步以减少延迟
                NULL);

    // 链接元素
    gst_bin_add_many(GST_BIN(pipeline), source, capsfilter, 
                    queue, convert, scale, videocrop, kmssink, NULL);
                    
    if (!gst_element_link_many(source, capsfilter, queue, 
                             convert, scale, videocrop, kmssink, NULL)) {
        fprintf(stderr, "Failed to link GStreamer elements\n");
        gst_object_unref(pipeline);
        return;
    }

    // 初始化MPP编码器 (用于录制)
    RK_S32 ret = mpp_create(&mpp_ctx, &mpi);
    if (ret != MPP_OK) {
        fprintf(stderr, "mpp_create failed: %d\n", ret);
        return;
    }

    ret = mpp_init(mpp_ctx, MPP_CTX_ENC, MPP_VIDEO_CodingAVC);
    if (ret != MPP_OK) {
        fprintf(stderr, "mpp_init failed: %d\n", ret);
        return;
    }

    // 打开busloop接收消息
    bus = gst_element_get_bus(pipeline);
}

void video_pipeline_cleanup(void) {
    if (pipeline) {
        gst_element_set_state(pipeline, GST_STATE_NULL);
        gst_object_unref(pipeline);
        pipeline = NULL;
    }
    
    if (bus) {
        gst_object_unref(bus);
        bus = NULL;
    }
    
    if (mpp_ctx) {
        mpi->reset(mpp_ctx);
        mpp_destroy(mpp_ctx);
        mpp_ctx = NULL;
    }
    
    if (output_file) {
        fclose(output_file);
        output_file = NULL;
    }
}

// 开始录制视频
void video_start_record(void) {
    if (is_recording) return;
    
    // 打开输出文件
    output_file = fopen("/tmp/video.h264", "wb");
    if (!output_file) {
        perror("Failed to open output file");
        return;
    }
    
    // 配置MPP编码器
    MppEncCfg cfg;
    mpp_enc_cfg_init(&cfg);
    mpp_enc_cfg_set_s32(cfg, "prep:width", CAM_WIDTH);
    mpp_enc_cfg_set_s32(cfg, "prep:height", CAM_HEIGHT);
    mpp_enc_cfg_set_s32(cfg, "prep:format", MPP_FMT_YUV420SP);
    mpp_enc_cfg_set_s32(cfg, "rc:mode", MPP_ENC_RC_MODE_CBR);
    mpp_enc_cfg_set_s32(cfg, "rc:bps_target", 2000000); // 2Mbps
    mpp_enc_cfg_set_s32(cfg, "rc:fps_in_flex", 0);
    mpp_enc_cfg_set_s32(cfg, "rc:fps_in_num", 30);
    mpp_enc_cfg_set_s32(cfg, "rc:fps_in_denorm", 1);
    mpp_enc_cfg_set_s32(cfg, "rc:gop", 30);
    
    RK_S32 ret = mpi->control(mpp_ctx, MPP_ENC_SET_CFG, cfg);
    if (ret != MPP_OK) {
        fprintf(stderr, "MPP encoder config failed: %d\n", ret);
        fclose(output_file);
        output_file = NULL;
        return;
    }
    
    is_recording = TRUE;
    printf("Starting recording to /tmp/video.h264\n");
    
    // 启动录制线程
    pthread_t record_thread;
    pthread_create(&record_thread, NULL, video_record_thread, NULL);
    pthread_detach(record_thread);
}

// 停止录制视频
void video_stop_record(void) {
    if (!is_recording) return;
    
    is_recording = FALSE;
    printf("Stopping recording\n");
    
    // 录制线程会自行关闭文件
}

// 启动视频预览
void *video_preview_thread(void *arg) {
    printf("Starting video preview thread\n");
    
    // 启动管道
    GstStateChangeReturn ret = gst_element_set_state(pipeline, GST_STATE_PLAYING);
    if (ret == GST_STATE_CHANGE_FAILURE) {
        fprintf(stderr, "Failed to start GStreamer pipeline\n");
        return NULL;
    }
    
    printf("Pipeline state change: %s\n", 
           (ret == GST_STATE_CHANGE_SUCCESS) ? "SUCCESS" : 
           (ret == GST_STATE_CHANGE_ASYNC) ? "ASYNC" : 
           (ret == GST_STATE_CHANGE_NO_PREROLL) ? "NO_PREROLL" : "UNKNOWN");

    // 监听消息
    GstMessage *msg;
    while ((msg = gst_bus_timed_pop_filtered(bus, GST_CLOCK_TIME_NONE,
           GST_MESSAGE_ERROR | GST_MESSAGE_EOS | GST_MESSAGE_STATE_CHANGED))) {
        
        switch (GST_MESSAGE_TYPE(msg)) {
        case GST_MESSAGE_ERROR: {
            GError *err;
            gchar *debug;
            gst_message_parse_error(msg, &err, &debug);
            fprintf(stderr, "GStreamer error: %s (%s)\n", err->message, debug);
            g_error_free(err);
            g_free(debug);
            break;
        }
        case GST_MESSAGE_EOS:
            printf("End of stream\n");
            break;
        case GST_MESSAGE_STATE_CHANGED: {
            GstState old_state, new_state;
            gst_message_parse_state_changed(msg, &old_state, &new_state, NULL);
            if (GST_MESSAGE_SRC(msg) == GST_OBJECT(pipeline)) {
                printf("Pipeline state changed from %s to %s\n",
                       gst_element_state_get_name(old_state),
                       gst_element_state_get_name(new_state));
            }
            break;
        }
        default:
            break;
        }
        
        gst_message_unref(msg);
    }
    
    return NULL;
}

// 录制线程
void *video_record_thread(void *arg) {
    // 创建tee分支管道来录制视频
    // 注：在实际应用中，你可能需要创建一个单独的GStreamer录制管道
    // 或者修改现有管道添加tee元素和分支
    
    // 简化的录制实现，实际场景中需要V4L2和MPP的复杂交互
    while (is_recording) {
        usleep(33333); // ~30fps
        // 实际应该从视频源获取帧数据，编码，然后写入文件
    }
    
    if (output_file) {
        fclose(output_file);
        output_file = NULL;
    }
    
    return NULL;
}