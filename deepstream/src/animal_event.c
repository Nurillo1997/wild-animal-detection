#include "animal_event.h"

#include <glib.h>
#include <time.h>
#include <curl/curl.h>

/*
 * FastAPI backend endpoint.
 * Change this to your server's IP/port if needed.
 */
#define FASTAPI_ENDPOINT "http://127.0.0.1:8000/events"

/*
 * libcurl response body is written here.
 * We discard it — only the HTTP status matters.
 */
static size_t
discard_response(
    void   *data,
    size_t  size,
    size_t  nmemb,
    void   *userp)
{
    (void)data;
    (void)userp;
    return size * nmemb;
}


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
        "%Y-%m-%dT%H:%M:%S",
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


gboolean
animal_event_post(const char *json)
{
    if (!json)
    {
        return FALSE;
    }

    CURL    *curl = curl_easy_init();
    gboolean ok   = FALSE;

    if (!curl)
    {
        g_printerr("animal_event_post: curl_easy_init() failed\n");
        return FALSE;
    }

    struct curl_slist *headers = NULL;

    headers = curl_slist_append(
        headers,
        "Content-Type: application/json"
    );

    curl_easy_setopt(curl, CURLOPT_URL,            FASTAPI_ENDPOINT);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER,     headers);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS,     json);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION,  discard_response);

    /*
     * 3-second timeout so a slow/offline backend
     * does not block the DeepStream pipeline.
     */
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 3L);

    CURLcode res = curl_easy_perform(curl);

    if (res != CURLE_OK)
    {
        g_printerr(
            "animal_event_post: curl error: %s\n",
            curl_easy_strerror(res)
        );
    }
    else
    {
        long http_status = 0;

        curl_easy_getinfo(
            curl,
            CURLINFO_RESPONSE_CODE,
            &http_status
        );

        if (http_status == 200 || http_status == 201)
        {
            ok = TRUE;
        }
        else
        {
            g_printerr(
                "animal_event_post: unexpected HTTP %ld\n",
                http_status
            );
        }
    }

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    return ok;
}


void
handle_animal_event(const AnimalDetectionEvent *event)
{
    if (!event)
    {
        return;
    }

    char *json = animal_event_to_json(event);

    if (!json)
    {
        return;
    }

    /* Log to stdout for debugging */
    g_print(
        "\n=== ANIMAL DETECTION EVENT ===\n"
        "%s\n"
        "==============================\n",
        json
    );

    /* POST to FastAPI backend */
    gboolean sent = animal_event_post(json);

    if (sent)
    {
        g_print("animal_event_post: OK -> %s\n", FASTAPI_ENDPOINT);
    }

    g_free(json);
}