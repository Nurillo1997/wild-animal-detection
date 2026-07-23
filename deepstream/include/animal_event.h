#ifndef ANIMAL_EVENT_H
#define ANIMAL_EVENT_H

#include <glib.h>
#include <time.h>

typedef struct {
    const char *animal_name;
    guint64 tracker_id;
    gfloat confidence;
    guint source_id;
    guint64 frame_number;
    gdouble video_timestamp; /* seconds */
    time_t detected_at;
} AnimalDetectionEvent;

/* Serializes an event to JSON. Caller must g_free() the result. */
char *animal_event_to_json(const AnimalDetectionEvent *event);

/* POSTs a JSON payload to the backend's /events endpoint. */
gboolean animal_event_post(const char *json);

/* Builds JSON from an event, logs it, and POSTs it to the backend. */
void handle_animal_event(const AnimalDetectionEvent *event);

/* Notifies the backend that a DeepStream session has started/stopped. */
gboolean animal_stream_start(void);
gboolean animal_stream_stop(void);

#endif