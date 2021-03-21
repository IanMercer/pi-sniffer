#include "device.h"
#include "utility.h"
#include "accesspoints.h"
#include "cJSON.h"
#include <glib.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/types.h>

// These must be in same order as enum values
char* categories[] = { "unknown", "phone", "wearable", "tablet", "headphones", "computer", 
    "tv", "fixed", "beacon", "car", "audio", "lighting", "sprinklers", "sales", 
    "appliance", "security", "fitness", "printer",
    "speakers", "camera", "watch", 
    "covid", "health", "tooth", "pencil", "accessory" };

int category_values[] = { CATEGORY_UNKNOWN, CATEGORY_PHONE, CATEGORY_WEARABLE, CATEGORY_TABLET, CATEGORY_HEADPHONES, CATEGORY_COMPUTER, 
    CATEGORY_TV, CATEGORY_FIXED, CATEGORY_BEACON, CATEGORY_CAR, CATEGORY_AUDIO_CARD, CATEGORY_LIGHTING, CATEGORY_SPRINKLERS, CATEGORY_POS, 
    CATEGORY_APPLIANCE, CATEGORY_SECURITY, CATEGORY_FITNESS, CATEGORY_PRINTER, 
    CATEGORY_SPEAKERS, CATEGORY_CAMERA, CATEGORY_WATCH, 
    CATEGORY_COVID, CATEGORY_HEALTH, CATEGORY_TOOTHBRUSH, CATEGORY_PENCIL, CATEGORY_ACCESSORY };

int category_to_int(char* category)
{
    for (uint8_t i = 0; i < sizeof(categories); i++) {
       if (category_values[i] != (int)i) g_info("Category does not match %i %i", i, category_values[i]);
       if (strcmp(category, categories[i]) == 0) return category_values[i];
    }
    return 0;
}

char* category_from_int(uint8_t i)
{
  if (i >= sizeof(categories)) return categories[0];
  return categories[i];
}

/*
   merge
*/
void merge(struct Device* local, struct Device* remote, char* access_name, bool safe, struct AccessPoint* ap)
{
    local->is_training_beacon = local->is_training_beacon || remote->is_training_beacon;

    // Remote name wins if it's a "stronger type"
    set_name(local, remote->name, remote->name_type, ap->short_client_id);
    // TODO: All the NAME rules should be applied here too (e.g. privacy)

    //optional_set(local->name, remote->name, NAME_LENGTH);
    optional_set_alias(local->alias, remote->alias, NAME_LENGTH);
    soft_set_8(&local->address_type, remote->address_type);

    if (remote->category != CATEGORY_UNKNOWN)
    {
        if (local->category == CATEGORY_UNKNOWN)
        {
            g_info("  %s Set category to '%s' was unknown (%s)", local->mac, category_from_int(remote->category), access_name);
            local->category = remote->category;
        }
        else if (local->category == CATEGORY_PHONE && (remote->category == CATEGORY_TABLET || remote->category == CATEGORY_WATCH))
        {
            // TABLET/WATCH overrides PHONE because we assume phone when someone unlocks or uses an Apple device
            g_info("  %s Override category from '%s' to '%s' (%s)", local->mac, category_from_int(local->category), category_from_int(remote->category), access_name);
            local->category = remote->category;
        }
        else if (local->category != remote->category) 
        {
            if (local->category == CATEGORY_PHONE && remote->category == CATEGORY_TV)
            {
                // Apple device, originally thought to be phone but is actually a TV
                local->category = CATEGORY_TV;
                g_debug("  %s Changed category from '%s' to '%s' (%s)", local->mac, category_from_int(local->category), category_from_int(remote->category), access_name);
            }
            else if (local->category == CATEGORY_PHONE && remote->category == CATEGORY_COMPUTER)
            {
                // Apple device, originally thought to be phone but is actually a macbook
                local->category = CATEGORY_COMPUTER;
                g_debug("  %s Changed category from '%s' to '%s' (%s)", local->mac, category_from_int(local->category), category_from_int(remote->category), access_name);
            }
            else
            {
                // messages wearable->phone should be ignored
                // watch->wearable should be ignored
                g_debug("  %s MAYBE change category from '%s' to '%s' (%s)", local->mac, category_from_int(local->category), category_from_int(remote->category), access_name);
                // TODO: Check any here
            }
        }
    }

    soft_set_u16(&local->appearance, remote->appearance);  // not used ?
    if (remote->try_connect_state >= TRY_CONNECT_COMPLETE) local->try_connect_state = TRY_CONNECT_COMPLETE;  // already connected once

    if (remote->known_interval > local->known_interval) local->known_interval = remote->known_interval;

    // TODO: Other fields that we can transfer over

    if (safe)  // i.e. difference between our clock and theirs was zero
    {
        if (remote->latest_any > local->latest_any)
        {
            //g_debug("Bumping %s '%s' by %.1fs from %s", local->mac, local->name, difftime(remote->latest, local->latest), access_name);
            local->latest_any = remote->latest_any;
        }
    }

}


/*
   Set name and name type if an improvement
*/
void set_name(struct Device* d, const char*value, enum name_type name_type, char* reason)
{
    if (value)
    {
        if (d->name_type < name_type)
        {
            if (d->name_type == nt_initial)
            {
                g_info("  %s Set name to '%s' (%i) %s", d->mac, value, name_type, reason);
            }
            else
            {
                g_info("  %s Name '%s' to '%s' (%i->%i) %s", d->mac, d->name, value, d->name_type, name_type, reason);
            }
                
            d->name_type = name_type;
            g_strlcpy(d->name, value, NAME_LENGTH);
        }
    }
    else {
        g_warning("value was null for set_name");
    }
}

