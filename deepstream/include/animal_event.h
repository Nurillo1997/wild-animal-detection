#ifndef ANIMAL_EVENT_H
#define ANIMAL_EVENT_H

#include <glib.h>
#include <time.h>


typedef struct
{
    const char *animal_name;

    guint64 tracker_id;

    gfloat confidence;

    guint source_id;

    guint64 frame_number;

    /*
     * Video presentation timestamp
     * in seconds.
     */
    gdouble video_timestamp;

    time_t detected_at;

} AnimalDetectionEvent;


/*
 * Converts an AnimalDetectionEvent
 * into a JSON string.
 *
 * The returned string is dynamically
 * allocated and must be freed
 * with g_free().
 */
char *
animal_event_to_json(
    const AnimalDetectionEvent *event
);


/*
 * Sends detection event JSON
 * to FastAPI POST /events.
 *
 * Returns TRUE on success.
 */
gboolean
animal_event_post(
    const char *json
);


/*
 * Handles a completed animal
 * detection event.
 *
 * Event
 *   ↓
 * JSON
 *   ↓
 * POST /events
 */
void
handle_animal_event(
    const AnimalDetectionEvent *event
);


/*
 * Notify FastAPI that a new
 * DeepStream streaming session
 * has started.
 *
 * POST /stream/start
 */
gboolean
animal_stream_start(void);


/*
 * Notify FastAPI that the current
 * DeepStream streaming session
 * has stopped.
 *
 * POST /stream/stop
 */
gboolean
animal_stream_stop(void);


#endif