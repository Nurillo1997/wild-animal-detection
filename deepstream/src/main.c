#include <gst/gst.h>
#include <stdio.h>

#include "gstnvdsmeta.h"
#include "nvdsmeta.h"


static void
pad_added_handler(GstElement *src, GstPad *new_pad, gpointer user_data)
{
    GstElement *streammux = GST_ELEMENT(user_data);
    GstCaps *caps = NULL;
    const GstStructure *structure = NULL;
    const gchar *name = NULL;
    GstPad *sink_pad = NULL;

    caps = gst_pad_get_current_caps(new_pad);

    if (!caps)
    {
        caps = gst_pad_query_caps(new_pad, NULL);
    }

    if (!caps)
    {
        g_printerr("Failed to get pad capabilities.\n");
        return;
    }

    structure = gst_caps_get_structure(caps, 0);
    name = gst_structure_get_name(structure);

    /*
     * uridecodebin may create both video and audio pads.
     * We only want the video stream.
     */
    if (!g_str_has_prefix(name, "video/"))
    {
        gst_caps_unref(caps);
        return;
    }

    g_print("Video pad detected: %s\n", name);

    /*
     * We have only one video source,
     * so explicitly request sink_0.
     */
    sink_pad = gst_element_request_pad_simple(
        streammux,
        "sink_0"
    );

    if (!sink_pad)
    {
        g_printerr(
            "Failed to get sink_0 pad from nvstreammux.\n"
        );

        gst_caps_unref(caps);
        return;
    }

    if (gst_pad_is_linked(sink_pad))
    {
        g_print("nvstreammux sink_0 is already linked.\n");

        gst_object_unref(sink_pad);
        gst_caps_unref(caps);

        return;
    }

    GstPadLinkReturn link_result =
        gst_pad_link(new_pad, sink_pad);

    if (link_result != GST_PAD_LINK_OK)
    {
        g_printerr(
            "Failed to link decoder to nvstreammux. Error: %d\n",
            link_result
        );
    }
    else
    {
        g_print(
            "Decoder linked to nvstreammux successfully.\n"
        );
    }

    gst_object_unref(sink_pad);
    gst_caps_unref(caps);
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

            if (animal_name != NULL){
                g_print(
                    "Animal Detected: %s | Confidence: %.2f\n",
                    animal_name,
                    object_meta->confidence);
            }
        }
    }

    return GST_PAD_PROBE_OK;
}

int main(int argc, char *argv[])
{
    GstElement *pipeline = NULL;
    GstElement *source = NULL;
    GstElement *streammux = NULL;
    GstElement *pgie = NULL;
    GstElement *sink = NULL;

    GstBus *bus = NULL;
    GstMessage *msg = NULL;

    gst_init(&argc, &argv);

    pipeline = gst_pipeline_new("wild-animal-detection-pipeline");

    source = gst_element_factory_make(
        "uridecodebin",
        "video-source"
    );

    streammux = gst_element_factory_make(
        "nvstreammux",
        "stream-muxer"
    );

    pgie = gst_element_factory_make(
        "nvinfer",
        "primary-inference"
    );

    sink = gst_element_factory_make(
        "fakesink",
        "sink"
    );

    if (!pipeline || !source || !streammux || !pgie || !sink)
    {
        g_printerr("Failed to create pipeline elements.\n");

        if (pipeline)
            gst_object_unref(pipeline);

        return -1;
    }

    /* Configure video source */
    g_object_set(
        G_OBJECT(source),
        "uri",
        "file:///home/zehnmindai/Developer/wild-animal-detection/videos/video2.mp4",
        NULL
    );

    /* Configure nvstreammux */
    g_object_set(
        G_OBJECT(streammux),
        "batch-size", 1,
        "width", 1280,
        "height", 720,
        "batched-push-timeout", 40000,
        NULL
    );

    /* Configure inference */
    g_object_set(
        G_OBJECT(pgie),
        "config-file-path",
        "config/pgie_yolo_config.txt",
        NULL
    );

    gst_bin_add_many(
        GST_BIN(pipeline),
        source,
        streammux,
        pgie,
        sink,
        NULL
    );

    if (!gst_element_link_many(
            streammux,
            pgie,
            sink,
            NULL))
    {
        g_printerr("Failed to link pipeline elements.\n");
        gst_object_unref(pipeline);

        return -1;
    }


    GstPad *pgie_src_pad =
    gst_element_get_static_pad(pgie, "src");

    if (!pgie_src_pad){
    g_printerr("Failed to get PGIE src pad.\n");
    }
    else
    {
    gst_pad_add_probe(
        pgie_src_pad,
        GST_PAD_PROBE_TYPE_BUFFER,
        pgie_src_pad_buffer_probe,
        NULL,
        NULL
    );

    gst_object_unref(pgie_src_pad);
}

    /*
     * uridecodebin creates its source pad dynamically.
     * When the pad becomes available, pad_added_handler()
     * connects it to nvstreammux.
     */
    g_signal_connect(
        source,
        "pad-added",
        G_CALLBACK(pad_added_handler),
        streammux
    );

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

    return 0;
}