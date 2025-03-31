#include "pipeline.h"
#include "shm_utils.h"
#include "utils.h"
#include <gst/app/gstappsink.h>
#include <time.h>
#include <string.h>

// 共享内存指针（需在 main.c 中定义并初始化）
extern void* shm_ptr;

// appsink 回调函数，将 RGBA 数据写入共享内存
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
        // 确保数据大小匹配（800x450 RGBA = 800 * 450 * 4 = 1,440,000 字节）
        if (map.size == 1440000) {
            memcpy(shm_ptr, map.data, map.size);
        } else {
            g_printerr("Buffer size mismatch: %lu (expected 1,440,000)\n", map.size);
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
               *app_queue, *videoscale, *capsfilter_scale, *videoconvert, *capsfilter_rgba, *videoflip, *app_sink,
               *audio_src, *audio_queue, *audio_enc, *audio_convert, *audio_resample;
    GstAppSinkCallbacks callbacks = { NULL, NULL, on_new_sample };

    // 创建元素（关键修改：添加格式转换链）
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
    capsfilter_scale = gst_element_factory_make("capsfilter", "scale_caps");
    videoconvert = gst_element_factory_make("videoconvert", "converter");
    capsfilter_rgba = gst_element_factory_make("capsfilter", "rgba_caps");
    videoflip = gst_element_factory_make("videoflip", "videoflip");
    app_sink = gst_element_factory_make("appsink", "app_sink");
    
    // 音频元素
    audio_src = gst_element_factory_make("alsasrc", "audio-source");
    audio_queue = gst_element_factory_make("queue", "audio_queue");
    audio_enc = gst_element_factory_make("vorbisenc", "audio-encoder");
    audio_convert = gst_element_factory_make("audioconvert", "audio-convert");
    audio_resample = gst_element_factory_make("audioresample", "audio-resample");

    // 检查元素创建是否成功
    if (!pipeline || !source || !tee || !enc_queue || !enc || !parse || !mux || !filesink ||
        !app_queue || !videoscale || !capsfilter_scale || !videoconvert || !capsfilter_rgba || !videoflip || !app_sink ||
        !audio_src || !audio_queue || !audio_enc || !audio_convert || !audio_resample) {
        g_printerr("元素创建失败\n");
        if (pipeline) gst_object_unref(pipeline);
        return NULL;
    }

    // 设置视频源属性（NV12 格式）
    g_object_set(G_OBJECT(source), "device", "/dev/video0", NULL);

    // 生成文件名（基于时间戳）
    char filename[256];
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    snprintf(filename, sizeof(filename), "/mnt/sdcard/record_%04d%02d%02d_%02d%02d%02d.mkv",
             t->tm_year + 1900, t->tm_mon + 1, t->tm_mday, t->tm_hour, t->tm_min, t->tm_sec);
    g_object_set(G_OBJECT(filesink), "location", filename, NULL);

    // 设置音频重采样质量
    g_object_set(G_OBJECT(audio_resample), "quality", 2, NULL);

    // 设置预览分支属性
    g_object_set(G_OBJECT(app_sink), "emit-signals", TRUE, "sync", FALSE, NULL);
    gst_app_sink_set_callbacks(GST_APP_SINK(app_sink), &callbacks, NULL, NULL);
    g_object_set(G_OBJECT(videoflip), "method", 1, NULL);  // 顺时针旋转90度

    // 关键修改 1：设置缩放 Caps（NV12 → 800x450）
    GstCaps *scale_caps = gst_caps_new_simple("video/x-raw",
                                            "format", G_TYPE_STRING, "NV12",
                                            "width", G_TYPE_INT, 800,
                                            "height", G_TYPE_INT, 450,
                                            NULL);
    g_object_set(capsfilter_scale, "caps", scale_caps, NULL);
    gst_caps_unref(scale_caps);

    // 关键修改 2：设置 RGBA 转换 Caps
    GstCaps *rgba_caps = gst_caps_new_simple("video/x-raw",
                                           "format", G_TYPE_STRING, "RGBA",
                                           NULL);
    g_object_set(capsfilter_rgba, "caps", rgba_caps, NULL);
    gst_caps_unref(rgba_caps);

    // 添加元素到管道
    gst_bin_add_many(GST_BIN(pipeline),
                     source, tee, enc_queue, enc, parse, mux, filesink,
                     app_queue, videoscale, capsfilter_scale, videoconvert, capsfilter_rgba, videoflip, app_sink,
                     audio_src, audio_queue, audio_convert, audio_resample, audio_enc,
                     NULL);

    // 链接视频源分支（NV12 1080p）
    GstCaps *src_caps = gst_caps_new_simple("video/x-raw",
                                          "format", G_TYPE_STRING, "NV12",
                                          "width", G_TYPE_INT, config->record_width,
                                          "height", G_TYPE_INT, config->record_height,
                                          "framerate", GST_TYPE_FRACTION, config->record_framerate, 1,
                                          NULL);
    if (!gst_element_link_filtered(source, tee, src_caps)) {
        g_printerr("无法链接视频源和 tee\n");
        goto error;
    }
    gst_caps_unref(src_caps);

    // 链接视频录制分支
    if (!gst_element_link_many(tee, enc_queue, enc, parse, mux, filesink, NULL)) {
        g_printerr("无法链接录制分支\n");
        goto error;
    }

    // 链接音频分支
    if (!gst_element_link_many(audio_src, audio_queue, audio_convert, audio_resample, audio_enc, mux, NULL)) {
        g_printerr("无法链接音频分支\n");
        goto error;
    }

    // 关键修改 3：链接预览分支（NV12 → 缩放 → 转RGBA → 旋转）
    if (!gst_element_link_many(tee, app_queue, videoscale, capsfilter_scale,
                             videoconvert, capsfilter_rgba, videoflip, app_sink, NULL)) {
        g_printerr("无法链接预览分支\n");
        goto error;
    }

    return pipeline;

error:
    gst_object_unref(pipeline);
    return NULL;
}

void start_pipeline(GstElement *pipeline) {
    if (!pipeline) {
        g_printerr("Pipeline is NULL.\n");
        return;
    }
    GstStateChangeReturn ret = gst_element_set_state(pipeline, GST_STATE_PLAYING);
    if (ret == GST_STATE_CHANGE_FAILURE) {
        g_printerr("无法启动管道\n");
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
        handle_error(msg);  // 假设 handle_error 在 utils.h 中实现
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