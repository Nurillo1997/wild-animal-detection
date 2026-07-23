#include <gst/gst.h>
#include <stdio.h>
#include "animal_event.h"
#include "gstnvdsmeta.h"
#include "nvdsmeta.h"

#include <time.h>

static GHashTable *seen_animal_ids = NULL;

#define NUM_SOURCES 2

//struct 
typedef struct
{
    GstElement *streammux;
    guint source_id;
} SourceContext;

static void
pad_added_handler(
    GstElement *src,
    GstPad *new_pad,
    gpointer user_data)
{
    SourceContext *context =
        (SourceContext *)user_data;

    GstElement *streammux =
        context->streammux;

    guint source_id =
        context->source_id;

    GstCaps *caps = NULL;
    const GstStructure *structure = NULL;
    const gchar *name = NULL;
    GstPad *sink_pad = NULL;

    caps = gst_pad_get_current_caps(new_pad);

    if (!caps)
    {
        caps = gst_pad_query_caps(
            new_pad,
            NULL
        );
    }

    if (!caps)
    {
        g_printerr(
            "Source %u: Failed to get pad capabilities.\n",
            source_id
        );

        return;
    }

    structure =
        gst_caps_get_structure(caps, 0);

    name =
        gst_structure_get_name(structure);

    /*
     * uridecodebin may create both
     * video and audio pads.
     *
     * We only connect video pads
     * to nvstreammux.
     */
    if (!g_str_has_prefix(name, "video/"))
    {
        gst_caps_unref(caps);
        return;
    }

    g_print(
        "Source %u: Video pad detected: %s\n",
        source_id,
        name
    );

    /*
     * Each source gets its own
     * nvstreammux sink pad:
     *
     * source 0 -> sink_0
     * source 1 -> sink_1
     * source 2 -> sink_2
     */
    gchar pad_name[32];

    g_snprintf(
        pad_name,
        sizeof(pad_name),
        "sink_%u",
        source_id
    );

    sink_pad =
        gst_element_request_pad_simple(
            streammux,
            pad_name
        );

    if (!sink_pad)
    {
        g_printerr(
            "Source %u: Failed to get %s from nvstreammux.\n",
            source_id,
            pad_name
        );

        gst_caps_unref(caps);

        return;
    }

    if (gst_pad_is_linked(sink_pad))
    {
        g_print(
            "Source %u: %s is already linked.\n",
            source_id,
            pad_name
        );

        gst_object_unref(sink_pad);
        gst_caps_unref(caps);

        return;
    }

    GstPadLinkReturn link_result =
        gst_pad_link(
            new_pad,
            sink_pad
        );

    if (link_result != GST_PAD_LINK_OK)
    {
        g_printerr(
            "Source %u: Failed to link decoder to %s. Error: %d\n",
            source_id,
            pad_name,
            link_result
        );
    }
    else
    {
        g_print(
            "Source %u: Decoder linked to %s successfully.\n",
            source_id,
            pad_name
        );
    }

    gst_object_unref(sink_pad);
    gst_caps_unref(caps);
}

//tracking holatini saqalaydigan struct
#define MAX_TRACKED_OBJECTS 1000
#define MIN_CONFIRMATION_FRAMES 5

typedef struct
{
    guint64 tracker_id;
    guint detection_count;
    gboolean event_sent;
} TrackedAnimalState;

static TrackedAnimalState tracked_animals[MAX_TRACKED_OBJECTS];
static guint tracked_animal_count = 0;


//Tracker ID bo‘yicha state topadigan funksiya
static TrackedAnimalState *
get_or_create_tracked_animal(guint64 tracker_id)
{
    for (guint i = 0; i < tracked_animal_count; i++)
    {
        if (tracked_animals[i].tracker_id == tracker_id)
        {
            return &tracked_animals[i];
        }
    }

    if (tracked_animal_count >= MAX_TRACKED_OBJECTS)
    {
        return NULL;
    }

    TrackedAnimalState *state =
        &tracked_animals[tracked_animal_count++];

    state->tracker_id = tracker_id;
    state->detection_count = 0;
    state->event_sent = FALSE;

    return state;
}

static gfloat
get_animal_confidence_threshold(gint class_id)
{
    switch (class_id)
    {
        case 14: /* bird */
            return 0.50f;

        case 15: /* cat */
            return 0.70f;

        case 16: /* dog */
            return 0.75f;

        case 17: /* horse */
            return 0.70f;

        case 18: /* sheep */
            return 0.70f;

        case 19: /* cow */
            return 0.70f;

        case 20: /* elephant */
            return 0.70f;

        case 21: /* bear */
            return 0.70f;

        case 22: /* zebra */
            return 0.70f;

        case 23: /* giraffe */
            return 0.70f;

        default:
            return 1.0f;
    }
}

//type of animals
static const char *
get_animal_name(gint class_id)
{
    switch (class_id)
    {
        case 14:
            return "bird";

        case 15:
            return "cat";

        case 16:
            return "dog";

        case 17:
            return "horse";

        case 18:
            return "sheep";

        case 19:
            return "cow";

        case 20:
            return "elephant";

        case 21:
            return "bear";

        case 22:
            return "zebra";

        case 23:
            return "giraffe";

        default:
            return NULL;
    }
}

static GstPadProbeReturn
pgie_src_pad_buffer_probe(
    GstPad *pad,
    GstPadProbeInfo *info,
    gpointer user_data)
{
    GstBuffer *buffer = GST_PAD_PROBE_INFO_BUFFER(info);

    if (!buffer)
    {
        return GST_PAD_PROBE_OK;
    }

    NvDsBatchMeta *batch_meta =
        gst_buffer_get_nvds_batch_meta(buffer);

    if (!batch_meta)
    {
        return GST_PAD_PROBE_OK;
    }

    for (NvDsMetaList *frame_list = batch_meta->frame_meta_list;
         frame_list != NULL;
         frame_list = frame_list->next)
    {
        NvDsFrameMeta *frame_meta =
            (NvDsFrameMeta *)frame_list->data;

        for (NvDsMetaList *object_list = frame_meta->obj_meta_list;
             object_list != NULL;
             object_list = object_list->next)
        {
            NvDsObjectMeta *object_meta =
                (NvDsObjectMeta *)object_list->data;

            gint class_id = object_meta->class_id;


            const char *animal_name =
                 get_animal_name(class_id);

            if (animal_name != NULL)
{
    gfloat confidence_threshold =
        get_animal_confidence_threshold(class_id);

    if (object_meta->confidence < confidence_threshold)
    {
        continue;
    }

    TrackedAnimalState *state =
        get_or_create_tracked_animal(object_meta->object_id);

    if (state != NULL)
    {
        state->detection_count++;

        if (!state->event_sent &&
            state->detection_count >= MIN_CONFIRMATION_FRAMES)
        {
            AnimalDetectionEvent event = {
                .animal_name = animal_name,
                .tracker_id = object_meta->object_id,
                .confidence = object_meta->confidence,
                .source_id = frame_meta->source_id,
                .frame_number = frame_meta->frame_num,
                .detected_at = time(NULL)
            };

            handle_animal_event(&event);

            state->event_sent = TRUE;
        }
    }
}


        }
    }

    return GST_PAD_PROBE_OK;
}

int main(int argc, char *argv[])
{
    GstElement *pipeline = NULL;
    GstElement *sources[NUM_SOURCES] = {NULL};
    GstElement *streammux = NULL;
    GstElement *pgie = NULL;
    GstElement *tracker = NULL;
    GstElement *sink = NULL;

    GstBus *bus = NULL;
    GstMessage *msg = NULL;


    gst_init(&argc, &argv);

    seen_animal_ids = g_hash_table_new(
        g_direct_hash,
        g_direct_equal
);  
SourceContext source_contexts[NUM_SOURCES];

    pipeline = gst_pipeline_new("wild-animal-detection-pipeline");

    for (guint i = 0; i < NUM_SOURCES; i++)
{
    gchar source_name[32];

    g_snprintf(
        source_name,
        sizeof(source_name),
        "video-source-%u",
        i
    );

    sources[i] =
        gst_element_factory_make(
            "uridecodebin",
            source_name
        );

    if (!sources[i])
    {
        g_printerr(
            "Failed to create source %u.\n",
            i
        );

        gst_object_unref(pipeline);

        return -1;
    }
}

    streammux = gst_element_factory_make(
        "nvstreammux",
        "stream-muxer"
    );

    pgie = gst_element_factory_make(
        "nvinfer",
        "primary-inference"
    );

    tracker = gst_element_factory_make(
    "nvtracker",
    "tracker"
);

    sink = gst_element_factory_make(
        "fakesink",
        "sink"
    );

    if (!pipeline || !streammux || !pgie || !sink)
{
    g_printerr(
        "Failed to create pipeline elements.\n"
    );

    if (pipeline)
    {
        gst_object_unref(pipeline);
    }

    return -1;
}

    /* Configure video source */
    const gchar *source_uris[NUM_SOURCES] = {
    "file:///home/zehnmindai/Developer/wild-animal-detection/videos/video1.mp4",
    "file:///home/zehnmindai/Developer/wild-animal-detection/videos/video2.mp4"
};

for (guint i = 0; i < NUM_SOURCES; i++)
{
    g_object_set(
        G_OBJECT(sources[i]),
        "uri",
        source_uris[i],
        NULL
    );
}

    /* Configure nvstreammux */
    g_object_set(
    G_OBJECT(streammux),
    "batch-size", NUM_SOURCES,
    "width", 640,
    "height", 640,
    "enable-padding", TRUE,
    "batched-push-timeout", 40000,
    NULL
);

    /* Configure inference */
    g_object_set(
        G_OBJECT(pgie),
        "config-file-path",
        "/home/zehnmindai/Developer/wild-animal-detection/config_infer_primary_yolo11.txt",
        NULL
    );

    /* Configure tracker */
g_object_set(
    G_OBJECT(tracker),
    "tracker-width", 640,
    "tracker-height", 384,
    "ll-lib-file",
    "/opt/nvidia/deepstream/deepstream-9.1/lib/libnvds_nvmultiobjecttracker.so",
    "ll-config-file",
    "/opt/nvidia/deepstream/deepstream-9.1/samples/configs/deepstream-app/config_tracker_NvDCF_perf.yml",
    NULL
);

   gst_bin_add_many(
    GST_BIN(pipeline),
    streammux,
    pgie,
    tracker,
    sink,
    NULL
);
for (guint i = 0; i < NUM_SOURCES; i++)
{
    gst_bin_add(
        GST_BIN(pipeline),
        sources[i]
    );
}

    if (!gst_element_link_many(
        streammux,
        pgie,
        tracker,
        sink,
        NULL))
    {
        g_printerr("Failed to link pipeline elements.\n");
        gst_object_unref(pipeline);

        return -1;
    }


    GstPad *tracker_src_pad =
    gst_element_get_static_pad(tracker, "src");

    if (!tracker_src_pad)
{
    g_printerr("Failed to get tracker src pad.\n");
}
else
{
    gst_pad_add_probe(
        tracker_src_pad,
        GST_PAD_PROBE_TYPE_BUFFER,
        pgie_src_pad_buffer_probe,
        NULL,
        NULL
    );

    gst_object_unref(tracker_src_pad);
}

    /*
     * uridecodebin creates its source pad dynamically.
     * When the pad becomes available, pad_added_handler()
     * connects it to nvstreammux.
     */
    for (guint i = 0; i < NUM_SOURCES; i++)
{
    source_contexts[i].streammux =
        streammux;

    source_contexts[i].source_id =
        i;

    g_signal_connect(
        sources[i],
        "pad-added",
        G_CALLBACK(pad_added_handler),
        &source_contexts[i]
    );
}

    g_print("Starting DeepStream pipeline...\n");

    gst_element_set_state(
        pipeline,
        GST_STATE_PLAYING
    );

    bus = gst_element_get_bus(pipeline);

    msg = gst_bus_timed_pop_filtered(
        bus,
        GST_CLOCK_TIME_NONE,
        GST_MESSAGE_ERROR | GST_MESSAGE_EOS
    );

    if (msg != NULL)
    {
        switch (GST_MESSAGE_TYPE(msg))
        {
            case GST_MESSAGE_ERROR:
            {
                GError *error = NULL;
                gchar *debug_info = NULL;

                gst_message_parse_error(
                    msg,
                    &error,
                    &debug_info
                );

                g_printerr(
                    "Error: %s\n",
                    error->message
                );

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

        gst_message_unref(msg);
    }

    gst_object_unref(bus);

    gst_element_set_state(
        pipeline,
        GST_STATE_NULL
    );

    gst_object_unref(pipeline);
    
    if (seen_animal_ids)
{
    g_hash_table_destroy(seen_animal_ids);
    seen_animal_ids = NULL;
}



    return 0;
}
