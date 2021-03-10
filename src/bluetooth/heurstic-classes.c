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

void handle_class(struct Device *existing, uint32_t deviceclass)
{
    // 7936 was a heart-rate monitor
    if (deviceclass == 0x200404 || 
        deviceclass == 0x240404 ||
        deviceclass == 0x340404)
    {
            // Wearable headset device

        // 21 | 10 | 2Major Service Class
        // CoD Bit 21: Audio (Speaker, Microphone, Headset service, …)
        // Major Device Class
        // CoD Bits 10: Audio/Video (headset,speaker,stereo, video display, vcr…)
        // Minor Device Class
        // CoD Bits 2: Wearable Headset Device
        soft_set_category(&existing->category, CATEGORY_HEADPHONES);
    }
    else if (deviceclass == 0x240408)
    {
        // handsfree car kit
        soft_set_category(&existing->category, CATEGORY_CAR);
    }
    else if (deviceclass == 0x043c || deviceclass == 0x8043c)
    {
        soft_set_category(&existing->category, CATEGORY_TV);
    }
    else if (deviceclass == 0x2a010c)  // MacBook Pro
    {
        soft_set_category(&existing->category, CATEGORY_COMPUTER);
    }
    else if (deviceclass == 0x5a020c)
    {
        soft_set_category(&existing->category, CATEGORY_PHONE);
    }
    else if (deviceclass == 0x5a020c)
    {
        // ANDROID BT device ?
    }
    else if (deviceclass == 0x60680)
    {
        // TBD Zebra beacon? checkout scanner?
    }
}