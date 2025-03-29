#include "pipeline.h"
#include "shm_utils.h"
#include "utils.h"
#include <gst/app/gstappsink.h>
#include <time.h>
#include <string.h>

// appsink 回调函数，将视频帧写入共享内存
static GstFlowReturn on_new_sample(GstAppSink *sink, gpointer user_data) {
    GstSample *sample = gst_app_sink_pull_sample(sink);
    if (!sample) {
        return GST_FLOW_ERROR;
    }
    GstBuffer *buffer = gst_sample_get_buffer(sample);
    if (buffer && shm_ptr) {
        GstMapInfo map;
        if (gst_buffer_map(buffer, &map, GST_MAP_READ)) {
            memcpy(shm_ptr, map.data, map.size);
            gst_buffer_unmap(buffer, &map);
        }
    }
    gst_sample_unref(sample);
    return GST_FLOW_OK;
}

GstElement* create_pipeline(VideoConfig *config) {
    if (!config) return NULL;

    GstElement *pipeline, *source, *tee, *enc_queue, *enc, *parse, *mux, *filesink,
               *app_queue, *videoscale, *videoflip, *videoconvert, *app_sink;
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
    videoflip = gst_element_factory_make("videoflip", "videoflip");
    videoconvert = gst_element_factory_make("videoconvert", "videoconvert");
    app_sink = gst_element_factory_make("appsink", "app_sink");

    if (!pipeline || !source || !tee || !enc_queue || !enc || !parse || !mux || !filesink ||
        !app_queue || !videoscale || !videoflip || !videoconvert || !app_sink) {
        g_printerr("Failed to create elements.\n");
        if (pipeline) gst_object_unref(pipeline);
        return NULL;
    }

    // 设置source属性
    g_object_set(G_OBJECT(source), "device", "/dev/video0", NULL);

    // 生成文件名，基于创建时间
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    char filename[256];
    snprintf(filename, sizeof(filename), "/mnt/sdcard/record_%04d%02d%02d_%02d%02d%02d.mkv",
             t->tm_year + 1900, t->tm_mon + 1, t->tm_mday, t->tm_hour, t->tm_min, t->tm_sec);
    g_object_set(G_OBJECT(filesink), "location", filename, NULL);

    // 设置app_sink属性
    g_object_set(G_OBJECT(app_sink), "emit-signals", TRUE, "sync", FALSE, NULL);
    gst_app_sink_set_callbacks(GST_APP_SINK(app_sink), &callbacks, NULL, NULL);

    // 设置videoflip属性
    g_object_set(G_OBJECT(videoflip), "method", 1, NULL);  // 顺时针旋转90度

    // 创建source_caps
    GstCaps *source_caps = gst_caps_new_simple("video/x-raw",
                                               "format", G_TYPE_STRING, "NV12",
                                               "width", G_TYPE_INT, config->record_width,
                                               "height", G_TYPE_INT, config->record_height,
                                               "framerate", GST_TYPE_FRACTION, config->record_framerate, 1,
                                               NULL);

    // 添加元素到管道
    gst_bin_add_many(GST_BIN(pipeline), source, tee, enc_queue, enc, parse, mux, filesink,
                     app_queue, videoscale, videoflip, videoconvert, app_sink, NULL);

    // 链接source和tee
    if (!gst_element_link_filtered(source, tee, source_caps)) {
        g_printerr("Failed to link source and tee.\n");
        gst_caps_unref(source_caps);
        gst_object_unref(pipeline);
        return NULL;
    }
    gst_caps_unref(source_caps);

    // 录制分支：tee -> enc_queue -> enc -> parse -> mux -> filesink
    if (!gst_element_link_many(tee, enc_queue, enc, parse, mux, filesink, NULL)) {
        g_printerr("Failed to link recording branch.\n");
        gst_object_unref(pipeline);
        return NULL;
    }

    // 预览分支：tee -> app_queue -> videoscale -> videoflip -> videoconvert -> app_sink
    // 移除手动链接videoscale和videoflip的代码，由gst_element_link_many自动处理
    if (!gst_element_link_many(tee, app_queue, videoscale, videoflip, videoconvert, app_sink, NULL)) {
        g_printerr("Failed to link preview branch.\n");
        gst_object_unref(pipeline);
        return NULL;
    }

    return pipeline;
}
void start_pipeline(GstElement *pipeline) {
    if (!pipeline) return;
    GstStateChangeReturn ret = gst_element_set_state(pipeline, GST_STATE_PLAYING);
    if (ret == GST_STATE_CHANGE_FAILURE) {
        g_printerr("Unable to set pipeline to playing state.\n");
    }
}

void monitor_pipeline(GstElement *pipeline) {
    if (!pipeline) return;
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