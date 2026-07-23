#include <gst/gst.h>
#include <stdio.h>
#include <time.h>

#include "animal_event.h"
#include "gstnvdsmeta.h"
#include "nvdsmeta.h"

#define NUM_SOURCES 2
#define MAX_TRACKED_OBJECTS 1000
#define MIN_CONFIRMATION_FRAMES 5

#define VIDEOS_DIR   "/home/zehnmindai/Developer/wild-animal-detection/videos"
#define PGIE_CONFIG  "/home/zehnmindai/Developer/wild-animal-detection/config_infer_primary_yolo11.txt"
#define HLS_DIR      "/home/zehnmindai/Developer/wild-animal-detection/backend/static/hls"
#define TRACKER_LIB  "/opt/nvidia/deepstream/deepstream-9.1/lib/libnvds_nvmultiobjecttracker.so"
#define TRACKER_CFG  "/opt/nvidia/deepstream/deepstream-9.1/samples/configs/deepstream-app/config_tracker_NvDCF_perf.yml"

typedef struct {
    GstElement *streammux;
    guint source_id;
} SourceContext;

typedef struct {
    guint64 tracker_id;
    guint detection_count;
    gboolean event_sent;
} TrackedAnimalState;

static TrackedAnimalState tracked_animals[MAX_TRACKED_OBJECTS];
static guint tracked_animal_count = 0;

/* Finds the tracking state for a tracker_id, creating one if needed. */
static TrackedAnimalState *get_or_create_tracked_animal(guint64 tracker_id)
{
    for (guint i = 0; i < tracked_animal_count; i++) {
        if (tracked_animals[i].tracker_id == tracker_id) {
            return &tracked_animals[i];
        }
    }

    if (tracked_animal_count >= MAX_TRACKED_OBJECTS) {
        g_printerr("Maximum tracked object limit reached.\n");
        return NULL;
    }

    TrackedAnimalState *state = &tracked_animals[tracked_animal_count++];
    state->tracker_id = tracker_id;
    state->detection_count = 0;
    state->event_sent = FALSE;

    return state;
}

/* COCO class IDs for the animal classes this pipeline reports on. */
static gfloat get_animal_confidence_threshold(gint class_id)
{
    switch (class_id) {
        case 14: return 0.50f; /* bird */
        case 15: return 0.70f; /* cat */
        case 16: return 0.75f; /* dog */
        case 17: return 0.70f; /* horse */
        case 18: return 0.70f; /* sheep */
        case 19: return 0.70f; /* cow */
        case 20: return 0.70f; /* elephant */
        case 21: return 0.70f; /* bear */
        case 22: return 0.70f; /* zebra */
        case 23: return 0.70f; /* giraffe */
        default: return 1.0f;  /* not an animal class */
    }
}

static const char *get_animal_name(gint class_id)
{
    switch (class_id) {
        case 14: return "bird";
        case 15: return "cat";
        case 16: return "dog";
        case 17: return "horse";
        case 18: return "sheep";
        case 19: return "cow";
        case 20: return "elephant";
        case 21: return "bear";
        case 22: return "zebra";
        case 23: return "giraffe";
        default: return NULL;
    }
}

/* uridecodebin creates its source pad dynamically; link it to nvstreammux once it appears. */
static void pad_added_handler(GstElement *src, GstPad *new_pad, gpointer user_data)
{
    (void)src;

    SourceContext *context = (SourceContext *)user_data;
    GstElement *streammux = context->streammux;
    guint source_id = context->source_id;

    GstCaps *caps = gst_pad_get_current_caps(new_pad);
    if (!caps) {
        caps = gst_pad_query_caps(new_pad, NULL);
    }
    if (!caps) {
        g_printerr("Source %u: failed to get pad capabilities.\n", source_id);
        return;
    }

    const GstStructure *structure = gst_caps_get_structure(caps, 0);
    const gchar *name = gst_structure_get_name(structure);

    if (!g_str_has_prefix(name, "video/")) {
        gst_caps_unref(caps);
        return;
    }

    g_print("Source %u: video pad detected: %s\n", source_id, name);

    gchar pad_name[32];
    g_snprintf(pad_name, sizeof(pad_name), "sink_%u", source_id);

    GstPad *sink_pad = gst_element_request_pad_simple(streammux, pad_name);
    if (!sink_pad) {
        g_printerr("Source %u: failed to get %s from nvstreammux.\n", source_id, pad_name);
        gst_caps_unref(caps);
        return;
    }

    if (gst_pad_is_linked(sink_pad)) {
        g_print("Source %u: %s is already linked.\n", source_id, pad_name);
        gst_object_unref(sink_pad);
        gst_caps_unref(caps);
        return;
    }

    GstPadLinkReturn link_result = gst_pad_link(new_pad, sink_pad);
    if (link_result != GST_PAD_LINK_OK) {
        g_printerr("Source %u: failed to link decoder to %s (error %d).\n", source_id, pad_name, link_result);
    } else {
        g_print("Source %u: decoder linked to %s successfully.\n", source_id, pad_name);
    }

    gst_object_unref(sink_pad);
    gst_caps_unref(caps);
}

/* Runs after NvTracker: filters animal detections, confirms tracker persistence
 * over MIN_CONFIRMATION_FRAMES, and reports each confirmed object exactly once. */
static GstPadProbeReturn tracker_src_pad_buffer_probe(GstPad *pad, GstPadProbeInfo *info, gpointer user_data)
{
    (void)pad;
    (void)user_data;

    GstBuffer *buffer = GST_PAD_PROBE_INFO_BUFFER(info);
    if (!buffer) {
        return GST_PAD_PROBE_OK;
    }

    NvDsBatchMeta *batch_meta = gst_buffer_get_nvds_batch_meta(buffer);
    if (!batch_meta) {
        return GST_PAD_PROBE_OK;
    }

    for (NvDsMetaList *frame_list = batch_meta->frame_meta_list; frame_list; frame_list = frame_list->next) {
        NvDsFrameMeta *frame_meta = (NvDsFrameMeta *)frame_list->data;

        for (NvDsMetaList *object_list = frame_meta->obj_meta_list; object_list; object_list = object_list->next) {
            NvDsObjectMeta *object_meta = (NvDsObjectMeta *)object_list->data;
            gint class_id = object_meta->class_id;

            const char *animal_name = get_animal_name(class_id);
            if (!animal_name) {
                continue;
            }

            if (object_meta->confidence < get_animal_confidence_threshold(class_id)) {
                continue;
            }

            TrackedAnimalState *state = get_or_create_tracked_animal(object_meta->object_id);
            if (!state) {
                continue;
            }

            state->detection_count++;

            if (!state->event_sent && state->detection_count >= MIN_CONFIRMATION_FRAMES) {
                AnimalDetectionEvent event = {
                    .animal_name = animal_name,
                    .tracker_id = object_meta->object_id,
                    .confidence = object_meta->confidence,
                    .source_id = frame_meta->source_id,
                    .frame_number = frame_meta->frame_num,
                    .video_timestamp = (frame_meta->buf_pts != GST_CLOCK_TIME_NONE)
                        ? (gdouble)frame_meta->buf_pts / (gdouble)GST_SECOND
                        : 0.0,
                    .detected_at = time(NULL),
                };

                handle_animal_event(&event);
                state->event_sent = TRUE;
            }
        }
    }

    return GST_PAD_PROBE_OK;
}

/* Prints the GStreamer ERROR/EOS message and returns once the bus wait ends. */
static void handle_bus_message(GstMessage *msg)
{
    switch (GST_MESSAGE_TYPE(msg)) {
        case GST_MESSAGE_ERROR: {
            GError *error = NULL;
            gchar *debug_info = NULL;

            gst_message_parse_error(msg, &error, &debug_info);
            g_printerr("Error: %s\n", error->message);
            if (debug_info) {
                g_printerr("Debug info: %s\n", debug_info);
            }

            g_clear_error(&error);
            g_free(debug_info);
            break;
        }

        case GST_MESSAGE_EOS:
            g_print("End of stream.\n");
            break;

        default:
            break;
    }
}

int main(int argc, char *argv[])
{
    gst_init(&argc, &argv);

    GstElement *pipeline = gst_pipeline_new("wild-animal-detection-pipeline");
    if (!pipeline) {
        g_printerr("Failed to create pipeline.\n");
        return -1;
    }

    /* One uridecodebin per video source. */
    GstElement *sources[NUM_SOURCES] = {NULL};
    for (guint i = 0; i < NUM_SOURCES; i++) {
        gchar source_name[32];
        g_snprintf(source_name, sizeof(source_name), "video-source-%u", i);

        sources[i] = gst_element_factory_make("uridecodebin", source_name);
        if (!sources[i]) {
            g_printerr("Failed to create source %u.\n", i);
            gst_object_unref(pipeline);
            return -1;
        }
    }

    /* Inference + tracking + tiling. */
    GstElement *streammux = gst_element_factory_make("nvstreammux", "stream-muxer");
    GstElement *pgie      = gst_element_factory_make("nvinfer", "primary-inference");
    GstElement *tracker   = gst_element_factory_make("nvtracker", "tracker");
    GstElement *tiler     = gst_element_factory_make("nvmultistreamtiler", "tiler");
    GstElement *converter = gst_element_factory_make("nvvideoconvert", "video-converter");
    GstElement *osd       = gst_element_factory_make("nvdsosd", "on-screen-display");

    /* HLS output: OSD -> nvvideoconvert -> h264enc -> h264parse -> mpegtsmux -> hlssink */
    GstElement *post_osd_converter = gst_element_factory_make("nvvideoconvert", "post-osd-converter");
    GstElement *encoder            = gst_element_factory_make("nvv4l2h264enc", "h264-encoder");
    GstElement *parser             = gst_element_factory_make("h264parse", "h264-parser");
    GstElement *muxer              = gst_element_factory_make("mpegtsmux", "mpegts-muxer");
    GstElement *hls_sink           = gst_element_factory_make("hlssink", "hls-sink");

    if (!streammux || !pgie || !tracker || !tiler || !converter || !osd ||
        !post_osd_converter || !encoder || !parser || !muxer || !hls_sink) {
        g_printerr("Failed to create pipeline elements.\n");
        gst_object_unref(pipeline);
        return -1;
    }

    const gchar *source_uris[NUM_SOURCES] = {
        "file://" VIDEOS_DIR "/video1.mp4",
        "file://" VIDEOS_DIR "/video2.mp4",
    };

    for (guint i = 0; i < NUM_SOURCES; i++) {
        g_object_set(G_OBJECT(sources[i]), "uri", source_uris[i], NULL);
    }

    g_object_set(G_OBJECT(streammux),
        "batch-size", NUM_SOURCES,
        "width", 640,
        "height", 640,
        "enable-padding", TRUE,
        "batched-push-timeout", 40000,
        NULL);

    g_object_set(G_OBJECT(pgie), "config-file-path", PGIE_CONFIG, NULL);

    g_object_set(G_OBJECT(tracker),
        "tracker-width", 640,
        "tracker-height", 384,
        "ll-lib-file", TRACKER_LIB,
        "ll-config-file", TRACKER_CFG,
        NULL);

    /* Two sources side by side: [ Source 0 | Source 1 ] */
    g_object_set(G_OBJECT(tiler),
        "rows", 1,
        "columns", 2,
        "width", 1280,
        "height", 640,
        NULL);

    g_object_set(G_OBJECT(encoder), "bitrate", 4000000, NULL);

    g_object_set(G_OBJECT(hls_sink),
        "location", HLS_DIR "/segment%05d.ts",
        "playlist-location", HLS_DIR "/playlist.m3u8",
        "target-duration", 2,
        "max-files", 5,
        "playlist-length", 5,
        NULL);

    gst_bin_add_many(GST_BIN(pipeline),
        streammux, pgie, tracker, tiler, converter, osd,
        post_osd_converter, encoder, parser, muxer, hls_sink,
        NULL);

    for (guint i = 0; i < NUM_SOURCES; i++) {
        gst_bin_add(GST_BIN(pipeline), sources[i]);
    }

    if (!gst_element_link_many(streammux, pgie, tracker, tiler, converter, osd,
                                post_osd_converter, encoder, parser, muxer, hls_sink, NULL)) {
        g_printerr("Failed to link HLS pipeline elements.\n");
        gst_object_unref(pipeline);
        return -1;
    }

    /* Detection metadata probe, independent of the HLS output branch. */
    GstPad *tracker_src_pad = gst_element_get_static_pad(tracker, "src");
    if (!tracker_src_pad) {
        g_printerr("Failed to get tracker src pad.\n");
    } else {
        gst_pad_add_probe(tracker_src_pad, GST_PAD_PROBE_TYPE_BUFFER, tracker_src_pad_buffer_probe, NULL, NULL);
        gst_object_unref(tracker_src_pad);
    }

    /* Link each source's dynamic pad to nvstreammux once it becomes available. */
    SourceContext source_contexts[NUM_SOURCES];
    for (guint i = 0; i < NUM_SOURCES; i++) {
        source_contexts[i].streammux = streammux;
        source_contexts[i].source_id = i;
        g_signal_connect(sources[i], "pad-added", G_CALLBACK(pad_added_handler), &source_contexts[i]);
    }

    g_print("Starting DeepStream pipeline...\n");
    g_print("HLS output: backend/static/hls/playlist.m3u8\n");

    if (gst_element_set_state(pipeline, GST_STATE_PLAYING) == GST_STATE_CHANGE_FAILURE) {
        g_printerr("Failed to start pipeline.\n");
        gst_element_set_state(pipeline, GST_STATE_NULL);
        gst_object_unref(pipeline);
        return -1;
    }

    if (!animal_stream_start()) {
        g_printerr("Warning: failed to notify backend about stream start.\n");
    }

    GstBus *bus = gst_element_get_bus(pipeline);
    GstMessage *msg = gst_bus_timed_pop_filtered(bus, GST_CLOCK_TIME_NONE,
                                                  GST_MESSAGE_ERROR | GST_MESSAGE_EOS);
    if (msg) {
        handle_bus_message(msg);
        gst_message_unref(msg);
    }

    if (!animal_stream_stop()) {
        g_printerr("Warning: failed to notify backend about stream stop.\n");
    }

    gst_object_unref(bus);
    gst_element_set_state(pipeline, GST_STATE_NULL);
    gst_object_unref(pipeline);

    return 0;
}