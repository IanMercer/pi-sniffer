// UUID heuristics

#include "utility.h"
#include "device.h"
#include "heuristics.h"

#include <glib.h>
#include <stdbool.h>
#include <stdio.h>
#include <sys/types.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/types.h>
#include <string.h>
#include <glib.h>

void handle_icon(struct Device *existing, const char* icon)
{
    if (strcmp(icon, "computer") == 0)
        soft_set_category(&existing->category, CATEGORY_COMPUTER);
    else if (strcmp(icon, "phone") == 0)
        soft_set_category(&existing->category, CATEGORY_PHONE);
    else if (strcmp(icon, "multimedia-player") == 0)
        soft_set_category(&existing->category, CATEGORY_TV);
    else if (strcmp(icon, "audio-card") == 0)
        soft_set_category(&existing->category, CATEGORY_AUDIO_CARD);
}