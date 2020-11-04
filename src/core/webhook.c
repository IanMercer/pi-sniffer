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
    char* auth = ""; // TODO: Username and password or token

    char result[BUFSIZE];
    char* body = state->json;

    if (body == NULL) return;

    g_debug("%s", state->json);

    http_post(state->webhook_domain, state->webhook_port, state->webhook_path, auth, body, strlen(body), result, sizeof(result));
}

