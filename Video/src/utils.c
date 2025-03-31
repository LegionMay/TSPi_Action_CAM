/*
 * @Author: LegionMay
 * @FilePath: /TSPi_Action/Video/src/utils.c
 */
#include "utils.h"
#include <stdio.h>

void handle_error(GstMessage *msg) {
    GError *err = NULL;
    gchar *debug_info = NULL;
    switch (GST_MESSAGE_TYPE(msg)) {
        case GST_MESSAGE_ERROR:
            gst_message_parse_error(msg, &err, &debug_info);
            g_printerr("Error: %s (%s)\n", err->message, debug_info ? debug_info : "no details");
            g_clear_error(&err);
            g_free(debug_info);
            break;
        case GST_MESSAGE_EOS:
            g_print("End of stream reached.\n");
            break;
        default:
            g_printerr("Unexpected message received.\n");
            break;
    }
}