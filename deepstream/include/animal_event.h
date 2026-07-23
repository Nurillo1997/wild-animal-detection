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
    time_t detected_at;
} AnimalDetectionEvent;

/*
 * Converts an AnimalDetectionEvent into a JSON string.
 *
 * The returned string is dynamically allocated
 * and must be freed with g_free().
 */
char *animal_event_to_json(
    const AnimalDetectionEvent *event
);

/*
 * Handles a completed animal detection event.
 */
void handle_animal_event(
    const AnimalDetectionEvent *event
);

#endif