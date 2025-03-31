/*
 * @Author: LegionMay
 * @FilePath: /TSPi_Action/Video/src/pipeline.c
 */
#include "pipeline.h"
#include "shm_utils.h"
#include "utils.h"
#include <gst/app/gstappsink.h>
#include <time.h>
#include <string.h>

// 共享内存指针（需在 main.c 中定义并初始化）
extern void* shm_ptr;

// appsink 回调函数，直接复制 buffer 到共享内存
static GstFlowReturn on_new_sample(GstAppSink *sink, gpointer user_data) {
    GstSample *sample = gst_app_sink_pull_sample(sink);
    if (!sample) {
        g_printerr("Failed to pull sample from appsink.\n");
        return GST_FLOW_ERROR;
    }

    GstBuffer *buffer = gst_sample_get_buffer(sample);
    if (!buffer) {
        g_printerr("Failed to get buffer from sample.\n");
        gst_sample_unref(sample);
        return GST_FLOW_ERROR;
    }

    // 检查共享内存是否已初始化
    if (!shm_ptr) {
        g_printerr("Shared memory pointer is NULL.\n");
        gst_sample_unref(sample);
        return GST_FLOW_ERROR;
    }

    // 映射 buffer 并复制到共享内存
    GstMapInfo map;
    if (gst_buffer_map(buffer, &map, GST_MAP_READ)) {
        if (map.size <= SHM_SIZE) {  // 确保不会溢出共享内存
            memcpy(shm_ptr, map.data, map.size);
        } else {
            g_printerr("Buffer size (%lu) exceeds shared memory size (%d).\n", map.size, SHM_SIZE);
        }
        gst_buffer_unmap(buffer, &map);
    } else {
        g_printerr("Failed to map buffer.\n");
    }

    gst_sample_unref(sample);
    return GST_FLOW_OK;
}

GstElement* create_pipeline(VideoConfig *config) {
    if (!config) {
        g_printerr("VideoConfig is NULL.\n");
        return NULL;
    }

    GstElement *pipeline, *source, *tee, *enc_queue, *enc, *parse, *mux, *filesink,
               *app_queue, *videoscale, *capsfilter, *videoflip, *app_sink,
               *audio_src, *audio_queue, *audio_enc, *audio_convert, *audio_resample;
    GstAppSinkCallbacks callbacks = { NULL, NULL, on_new_sample };

    // 创建元素
    pipeline = gst_pipeline_new("video-pipeline");
    source = gst_element_factory_make("v4l2src", "source");
    tee = gst_element_factory_make("tee", "tee");
    enc_queue = gst_element_factory_make("queue", "enc_queue");
    enc = gst_element_factory_make("mpph265enc", "encoder");
    parse = gst_element_factory_make("h265parse", "parser");
    mux = gst_element_factory_make("matroskamux", "muxer");
    filesink = gst_element_factory_make("filesink", "filesink");
    app_queue = gst_element_factory_make("queue", "app_queue");
    videoscale = gst_element_factory_make("videoscale", "videoscale");
    capsfilter = gst_element_factory_make("capsfilter", "scaler_caps");  // 新增 capsfilter
    videoflip = gst_element_factory_make("videoflip", "videoflip");
    app_sink = gst_element_factory_make("appsink", "app_sink");
    
    // 添加音频元素
    audio_src = gst_element_factory_make("alsasrc", "audio-source");
    audio_queue = gst_element_factory_make("queue", "audio_queue");
    audio_enc = gst_element_factory_make("vorbisenc", "audio-encoder");
    audio_convert = gst_element_factory_make("audioconvert", "audio-convert");
    audio_resample = gst_element_factory_make("audioresample", "audio-resample");

    // 检查元素创建是否成功
    if (!pipeline || !source || !tee || !enc_queue || !enc || !parse || !mux || !filesink ||
        !app_queue || !videoscale || !capsfilter || !videoflip || !app_sink ||
        !audio_src || !audio_queue || !audio_enc || !audio_convert || !audio_resample) {
        g_printerr("Failed to create elements.\n");
        if (pipeline) gst_object_unref(pipeline);
        return NULL;
    }

    // 设置视频源属性
    g_object_set(G_OBJECT(source), "device", "/dev/video0", NULL);

    // 生成文件名，基于创建时间
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    char filename[256];
    snprintf(filename, sizeof(filename), "/mnt/sdcard/record_%04d%02d%02d_%02d%02d%02d.mkv",
             t->tm_year + 1900, t->tm_mon + 1, t->tm_mday, t->tm_hour, t->tm_min, t->tm_sec);
    g_object_set(G_OBJECT(filesink), "location", filename, NULL);

    // 设置音频重采样质量
    g_object_set(G_OBJECT(audio_resample), "quality", 2, NULL);  // 高质量重采样

    // 设置 app_sink 属性
    g_object_set(G_OBJECT(app_sink), "emit-signals", TRUE, "sync", FALSE, NULL);
    gst_app_sink_set_callbacks(GST_APP_SINK(app_sink), &callbacks, NULL, NULL);

    // 设置 videoflip 属性
    g_object_set(G_OBJECT(videoflip), "method", 1, NULL);  // 顺时针旋转90度

    // 创建视频源 Caps（NV12 格式）
    GstCaps *video_caps = gst_caps_new_simple("video/x-raw",
                                               "format", G_TYPE_STRING, "NV12",
                                               "width", G_TYPE_INT, config->record_width,
                                               "height", G_TYPE_INT, config->record_height,
                                               "framerate", GST_TYPE_FRACTION, config->record_framerate, 1,
                                               NULL);

    // 关键修改：设置缩放后的 Caps（800x450 NV12）
    GstCaps *scale_caps = gst_caps_new_simple("video/x-raw",
                                             "format", G_TYPE_STRING, "NV12",
                                             "width", G_TYPE_INT, 800,
                                             "height", G_TYPE_INT, 450,
                                             NULL);
    g_object_set(capsfilter, "caps", scale_caps, NULL);
    gst_caps_unref(scale_caps);

    // 添加元素到管道
    gst_bin_add_many(GST_BIN(pipeline),
                     source, tee, enc_queue, enc, parse, mux, filesink,
                     app_queue, videoscale, capsfilter, videoflip, app_sink,
                     audio_src, audio_queue, audio_convert, audio_resample, audio_enc,
                     NULL);

    // 链接视频源和 tee
    if (!gst_element_link_filtered(source, tee, video_caps)) {
        g_printerr("Failed to link video source and tee.\n");
        gst_caps_unref(video_caps);
        gst_object_unref(pipeline);
        return NULL;
    }
    gst_caps_unref(video_caps);

    // 链接视频录制分支
    if (!gst_element_link_many(tee, enc_queue, enc, parse, mux, filesink, NULL)) {
        g_printerr("Failed to link video recording branch.\n");
        gst_object_unref(pipeline);
        return NULL;
    }

    // 链接音频分支
    if (!gst_element_link_many(audio_src, audio_queue, audio_convert, audio_resample, audio_enc, mux, NULL)) {
        g_printerr("Failed to link audio encoding branch.\n");
        gst_object_unref(pipeline);
        return NULL;
    }

    // 关键修改：预览分支链接（使用 capsfilter）
    if (!gst_element_link_many(tee, app_queue, videoscale, capsfilter, videoflip, app_sink, NULL)) {
        g_printerr("Failed to link preview branch.\n");
        gst_object_unref(pipeline);
        return NULL;
    }

    return pipeline;
}

// 以下函数保持不变
void start_pipeline(GstElement *pipeline) {
    if (!pipeline) {
        g_printerr("Pipeline is NULL.\n");
        return;
    }
    GstStateChangeReturn ret = gst_element_set_state(pipeline, GST_STATE_PLAYING);
    if (ret == GST_STATE_CHANGE_FAILURE) {
        g_printerr("Unable to set pipeline to playing state.\n");
    }
}

void monitor_pipeline(GstElement *pipeline) {
    if (!pipeline) {
        g_printerr("Pipeline is NULL.\n");
        return;
    }
    GstBus *bus = gst_element_get_bus(pipeline);
    GstMessage *msg = gst_bus_timed_pop_filtered(bus, GST_CLOCK_TIME_NONE,
                                                 GST_MESSAGE_ERROR | GST_MESSAGE_EOS);
    if (msg) {
        handle_error(msg);
        gst_message_unref(msg);
    }
    gst_object_unref(bus);
}

void cleanup_pipeline(GstElement *pipeline) {
    if (pipeline) {
        gst_element_set_state(pipeline, GST_STATE_NULL);
        gst_object_unref(pipeline);
    }
}