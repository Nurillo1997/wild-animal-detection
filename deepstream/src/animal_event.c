#include "animal_event.h"

#include <glib.h>
#include <time.h>
#include <curl/curl.h>

#define FASTAPI_EVENTS_ENDPOINT       "http://127.0.0.1:8000/events"
#define FASTAPI_STREAM_START_ENDPOINT "http://127.0.0.1:8000/stream/start"
#define FASTAPI_STREAM_STOP_ENDPOINT  "http://127.0.0.1:8000/stream/stop"

static size_t discard_response(void *data, size_t size, size_t nmemb, void *userp)
{
    (void)data;
    (void)userp;
    return size * nmemb;
}

static gboolean http_post(const char *url, const char *json)
{
    if (!url) {
        return FALSE;
    }

    CURL *curl = curl_easy_init();
    if (!curl) {
        g_printerr("http_post: curl_easy_init() failed\n");
        return FALSE;
    }

    struct curl_slist *headers = curl_slist_append(NULL, "Content-Type: application/json");

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json ? json : "{}");
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, discard_response);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 3L); /* never block the pipeline */

    gboolean ok = FALSE;
    CURLcode result = curl_easy_perform(curl);

    if (result != CURLE_OK) {
        g_printerr("http_post: curl error for %s: %s\n", url, curl_easy_strerror(result));
    } else {
        long status = 0;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);

        if (status >= 200 && status < 300) {
            ok = TRUE;
        } else {
            g_printerr("http_post: unexpected HTTP %ld from %s\n", status, url);
        }
    }

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    return ok;
}

char *animal_event_to_json(const AnimalDetectionEvent *event)
{
    if (!event) {
        return NULL;
    }

    struct tm local_time;
    localtime_r(&event->detected_at, &local_time);

    char time_buffer[64];
    strftime(time_buffer, sizeof(time_buffer), "%Y-%m-%dT%H:%M:%S", &local_time);

    return g_strdup_printf(
        "{"
        "\"event_type\":\"animal_detected\","
        "\"animal\":\"%s\","
        "\"tracker_id\":%" G_GUINT64_FORMAT ","
        "\"confidence\":%.2f,"
        "\"source_id\":%u,"
        "\"frame_number\":%" G_GUINT64_FORMAT ","
        "\"video_timestamp\":%.3f,"
        "\"detected_at\":\"%s\""
        "}",
        event->animal_name,
        event->tracker_id,
        event->confidence,
        event->source_id,
        event->frame_number,
        event->video_timestamp,
        time_buffer);
}

gboolean animal_event_post(const char *json)
{
    return json ? http_post(FASTAPI_EVENTS_ENDPOINT, json) : FALSE;
}

void handle_animal_event(const AnimalDetectionEvent *event)
{
    if (!event) {
        return;
    }

    char *json = animal_event_to_json(event);
    if (!json) {
        return;
    }

    g_print("\n=== ANIMAL DETECTION EVENT ===\n%s\n==============================\n", json);

    if (animal_event_post(json)) {
        g_print("animal_event_post: OK -> %s\n", FASTAPI_EVENTS_ENDPOINT);
    }

    g_free(json);
}

gboolean animal_stream_start(void)
{
    g_print("\n=== STREAM LIFECYCLE ===\nSending stream_started...\n");

    gboolean sent = http_post(FASTAPI_STREAM_START_ENDPOINT, "{}");

    if (sent) {
        g_print("stream_started: OK -> %s\n========================\n", FASTAPI_STREAM_START_ENDPOINT);
    } else {
        g_printerr("stream_started: FAILED -> %s\n", FASTAPI_STREAM_START_ENDPOINT);
    }

    return sent;
}

gboolean animal_stream_stop(void)
{
    g_print("\n=== STREAM LIFECYCLE ===\nSending stream_stopped...\n");

    gboolean sent = http_post(FASTAPI_STREAM_STOP_ENDPOINT, "{}");

    if (sent) {
        g_print("stream_stopped: OK -> %s\n========================\n", FASTAPI_STREAM_STOP_ENDPOINT);
    } else {
        g_printerr("stream_stopped: FAILED -> %s\n", FASTAPI_STREAM_STOP_ENDPOINT);
    }

    return sent;
}