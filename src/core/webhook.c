/*
   Implements a webhook push of changed room counts and beacon locations
*/

#include "webhook.h"
#include "http.h"
#include <string.h>
#include <glib.h>
#include <stdio.h>
#include <stdlib.h>

#define BUFSIZE 4096

void post_to_webhook (struct OverallState* state)
{
    if (state->webhook_domain == NULL) return;
    if (strlen(state->webhook_domain) == 0) return;

    char* auth = ""; // TODO: Username and password or token

    char result[BUFSIZE];
    char* body = state->json;

    if (body == NULL) return;

    //g_debug("%s", state->json);

    http_post(state->webhook_domain, state->webhook_port, state->webhook_path, auth, body, strlen(body), result, sizeof(result));
}


// {
//  "deviceStateReason": "Reboot",
//  "saltMinionId": "samplesite",
//  "deviceId": "29e81edc-5d99-4b4b-b660-5b3ae7bf0ceb",
//  "freeDiskSpace": 123456,
//  "runtimeSeconds": 20000,
//  "peoplePresent": [
//    {
//      "roomName": "kitchen",
//      "count": 4
//    }
//  ],
//  "localTimeZoneOffset": -8
//}

