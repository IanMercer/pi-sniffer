// UUID heuristics

#include "utility.h"
#include "device.h"
#include "heuristics.h"
#include "bluetooth.h"

#include <glib.h>
#include <stdbool.h>
#include <stdio.h>
#include <sys/types.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/types.h>
#include <string.h>
#include <glib.h>

void handle_appearance(struct Device *existing, const uint16_t appearance)
{
    if (appearance == BLE_APPEARANCE_GENERIC_PHONE)
    {
        soft_set_category(&existing->category, CATEGORY_PHONE);
    }
    else if (appearance == BLE_APPEARANCE_GENERIC_COMPUTER)
    {
        soft_set_category(&existing->category, CATEGORY_COMPUTER);
    }
    else if (appearance == BLE_APPEARANCE_GENERIC_WATCH)
    {
        soft_set_category(&existing->category, CATEGORY_WATCH);
    }
    else if (appearance == BLE_APPEARANCE_WATCH_SPORTS_WATCH)
    {
        soft_set_category(&existing->category, CATEGORY_WATCH);
    }
    else if (appearance == BLE_APPEARANCE_GENERIC_TAG)
    {
        // DeWalt sends this
        soft_set_category(&existing->category, CATEGORY_BEACON);
    }
    // iPad, Watch, ... seem to do 640 so not useful
}