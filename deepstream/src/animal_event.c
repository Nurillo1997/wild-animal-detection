#include "animal_event.h"

#include <glib.h>
#include <time.h>


char *
animal_event_to_json(const AnimalDetectionEvent *event)
{
    if (!event)
    {
        return NULL;
    }

    struct tm local_time;

    localtime_r(
        &event->detected_at,
        &local_time
    );

    char time_buffer[64];

    strftime(
        time_buffer,
        sizeof(time_buffer),
        "%Y-%m-%d %H:%M:%S",
        &local_time
    );

    char *json = g_strdup_printf(
        "{"
        "\"event_type\":\"animal_detected\","
        "\"animal\":\"%s\","
        "\"tracker_id\":%" G_GUINT64_FORMAT ","
        "\"confidence\":%.2f,"
        "\"source_id\":%u,"
        "\"frame_number\":%" G_GUINT64_FORMAT ","
        "\"detected_at\":\"%s\""
        "}",
        event->animal_name,
        event->tracker_id,
        event->confidence,
        event->source_id,
        event->frame_number,
        time_buffer
    );

    return json;
}


void
handle_animal_event(const AnimalDetectionEvent *event)
{
    if (!event)
    {
        return;
    }

    char *json =
        animal_event_to_json(event);

    if (!json)
    {
        return;
    }

    /*
     * For now, output the event to stdout.
     *
     * Later this JSON can be sent to:
     * - RabbitMQ
     * - REST API
     * - Database
     */
    g_print(
        "\n=== ANIMAL DETECTION EVENT ===\n"
        "%s\n"
        "==============================\n",
        json
    );

    /*
     * animal_event_to_json() allocates memory
     * using g_strdup_printf().
     */
    g_free(json);
}