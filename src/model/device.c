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
    "tv", "fixed", "beacon", "car", "audio", "lighting", "sprinklers", "sales", "appliance", "security", "fitness", "printer",
    "speakers", "camera", "watch", "covid", "health", "tooth", "fitness", "pencil", "accessory" };

int category_values[] = { CATEGORY_UNKNOWN, CATEGORY_PHONE, CATEGORY_WEARABLE, CATEGORY_TABLET, CATEGORY_HEADPHONES, CATEGORY_COMPUTER, 
    CATEGORY_TV, CATEGORY_FIXED, CATEGORY_BEACON, CATEGORY_CAR, CATEGORY_AUDIO_CARD, CATEGORY_LIGHTING, CATEGORY_SPRINKLERS, CATEGORY_POS, 
    CATEGORY_APPLIANCE, CATEGORY_SECURITY, CATEGORY_FITNESS, CATEGORY_PRINTER, CATEGORY_SPEAKERS, CATEGORY_CAMERA, CATEGORY_WATCH, 
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
void merge(struct Device* local, struct Device* remote, char* access_name, bool safe)
{
    local->is_training_beacon = local->is_training_beacon || remote->is_training_beacon;

    // Remote name wins if it's a "stronger type"
    set_name(local, remote->name, remote->name_type);
    // TODO: All the NAME rules should be applied here too (e.g. privacy)

    //optional_set(local->name, remote->name, NAME_LENGTH);
    optional_set_alias(local->alias, remote->alias, NAME_LENGTH);
    soft_set_8(&local->address_type, remote->address_type);

    if (remote->category != CATEGORY_UNKNOWN)
    {
        if (local->category == CATEGORY_UNKNOWN)
        {
            g_info("  %s Set category to '%s', message from %s", local->mac, category_from_int(remote->category), access_name);
            local->category = remote->category;
        }
        else if (local->category == CATEGORY_PHONE && (remote->category == CATEGORY_TABLET || remote->category == CATEGORY_WATCH))
        {
            // TABLET/WATCH overrides PHONE because we assume phone when someone unlocks or uses an Apple device
            g_info("  %s Override category from '%s' to '%s', message from %s", local->mac, category_from_int(local->category), category_from_int(remote->category), access_name);
            local->category = remote->category;
        }
        else if (local->category != remote->category) 
        {
            if (local->category == CATEGORY_PHONE && remote->category == CATEGORY_TV)
            {
                // Apple device, originally thought to be phone but is actually a TV
                local->category = CATEGORY_TV;
                g_debug("  %s Changed category from '%s' to '%s', message from %s", local->mac, category_from_int(local->category), category_from_int(remote->category), access_name);
            }
            else if (local->category == CATEGORY_PHONE && remote->category == CATEGORY_COMPUTER)
            {
                // Apple device, originally thought to be phone but is actually a macbook
                local->category = CATEGORY_COMPUTER;
                g_debug("  %s Changed category from '%s' to '%s', message from %s", local->mac, category_from_int(local->category), category_from_int(remote->category), access_name);
            }
            else
            {
                // messages wearable->phone should be ignored
                // watch->wearable should be ignored
                g_debug("  %s MAYBE change category from '%s' to '%s', message from %s", local->mac, category_from_int(local->category), category_from_int(remote->category), access_name);
                // TODO: Check any here
            }
        }
    }

    soft_set_u16(&local->appearance, remote->appearance);  // not used ?
    if (remote->try_connect_state >= TRY_CONNECT_COMPLETE) local->try_connect_state = TRY_CONNECT_COMPLETE;  // already connected once
    // TODO: Other fields that we can transfer over

    if (safe)  // i.e. difference between our clock and theirs was zero
    {
        if (remote->latest_local > local->latest_any)
        {
            //g_debug("Bumping %s '%s' by %.1fs from %s", local->mac, local->name, difftime(remote->latest, local->latest), access_name);
            local->latest_any = remote->latest_any;
        }
    }

}

/*
*  Send full access point statistics occasionally to all other mesh devices
*/
char* access_point_to_json (struct AccessPoint* a)
{
    char *string = NULL;
    cJSON *j = cJSON_CreateObject();

    // AccessPoint details
    cJSON_AddStringToObject(j, CJ_FROM, a->client_id);
    cJSON_AddStringToObject(j, CJ_DESCRIPTION, a->description);
    cJSON_AddStringToObject(j, CJ_PLATFORM, a->platform);
    cJSON_AddRounded(j, CJ_RSSI_ONE_METER, a->rssi_one_meter);
    cJSON_AddRounded(j, CJ_RSSI_FACTOR, a->rssi_factor);
    cJSON_AddRounded(j, CJ_PEOPLE_DISTANCE, a->people_distance);

    string = cJSON_PrintUnformatted(j);
    cJSON_Delete(j);
    return string;
}

/*
*  Send minimal access point information and minimal device information over mesh
*/
char* device_to_json (struct AccessPoint* a, struct Device* device)
{
    char *string = NULL;
    cJSON *j = cJSON_CreateObject();

    // AccessPoint details
    cJSON_AddStringToObject(j, CJ_FROM, a->client_id);
    //cJSON_AddStringToObject(j, CJ_DESCRIPTION, a->description);
    //cJSON_AddStringToObject(j, CJ_PLATFORM, a->platform);
    //cJSON_AddRounded(j, CJ_RSSI_ONE_METER, a->rssi_one_meter);
    //cJSON_AddRounded(j, CJ_RSSI_FACTOR, a->rssi_factor);
    //cJSON_AddRounded(j, CJ_PEOPLE_DISTANCE, a->people_distance);
    cJSON_AddNumberToObject(j, CJ_SEQ, a->sequence);

    // Device details
    cJSON_AddStringToObject(j, CJ_MAC, device->mac);
    cJSON_AddStringToObject(j, CJ_NAME, device->name);

    cJSON_AddStringToObject(j, CJ_ALIAS, device->alias);
    cJSON_AddNumberToObject(j, CJ_ADDRESS_TYPE, device->address_type);
    cJSON_AddStringToObject(j, CJ_CATEGORY, category_from_int(device->category));
//    cJSON_AddBoolToObject(j, CJ_PAIRED, device->paired);
//    cJSON_AddBoolToObject(j, CJ_CONNECTED, device->connected);
//    cJSON_AddBoolToObject(j, CJ_TRUSTED, device->trusted);
//    cJSON_AddNumberToObject(j, CJ_DEVICE_CLASS, device->deviceclass);
//    cJSON_AddNumberToObject(j, CJ_APPEARANCE, device->appearance);
//    cJSON_AddNumberToObject(j, CJ_MANUFACTURER_DATA_HASH, device->manufacturer_data_hash);
//    cJSON_AddNumberToObject(j, CJ_SERVICE_DATA_HASH, device->service_data_hash);
//    cJSON_AddNumberToObject(j, CJ_UUIDS_LENGTH, device->uuids_length);
//    cJSON_AddNumberToObject(j, CJ_UUIDS_HASH, device->uuid_hash);
//    cJSON_AddNumberToObject(j, CJ_TXPOWER, device->txpower);
    cJSON_AddNumberToObject(j, CJ_LAST_SENT, device->last_sent);
    cJSON_AddRounded3(j, CJ_DISTANCE, device->distance);
    cJSON_AddNumberToObject(j, CJ_EARLIEST, device->earliest);
    cJSON_AddNumberToObject(j, CJ_LATEST, device->latest_local);
    cJSON_AddNumberToObject(j, CJ_COUNT, device->count);  // integer
    cJSON_AddRounded3(j, CJ_FILTERED_RSSI, device->filtered_rssi.current_estimate);
    cJSON_AddNumberToObject(j, CJ_RAW_RSSI, device->raw_rssi);  // integer
    cJSON_AddNumberToObject(j, CJ_TRY_CONNECT_STATE, device->try_connect_state);
    cJSON_AddNumberToObject(j, CJ_NAME_TYPE, device->name_type);
    cJSON_AddNumberToObject(j, CJ_ADDRESS_TYPE, device->address_type);
    if (device->is_training_beacon)
    {
        cJSON_AddNumberToObject(j, CJ_TRAINING, 1);
    }

    string = cJSON_PrintUnformatted(j);
    cJSON_Delete(j);
    return string;
}



struct AccessPoint* device_from_json(const char* json, struct AccessPoint** access_point_list, struct Device* device)
{
    cJSON *djson = cJSON_Parse(json);

    // ACCESS POINT
    struct AccessPoint* ap = NULL;

    cJSON *fromj = cJSON_GetObjectItemCaseSensitive(djson, CJ_FROM);
    if (cJSON_IsString(fromj) && (fromj->valuestring != NULL))
    {
        bool created = FALSE;
        ap = get_or_create_access_point(access_point_list, fromj->valuestring, &created);

        time(&ap->last_seen);
    }

    cJSON *descriptionj = cJSON_GetObjectItemCaseSensitive(djson, CJ_DESCRIPTION);
    if (ap != NULL && cJSON_IsString(descriptionj) && (descriptionj->valuestring != NULL))
    {
        strncpy(ap->description, descriptionj->valuestring, META_LENGTH);
    }

    cJSON *platformj = cJSON_GetObjectItemCaseSensitive(djson, CJ_PLATFORM);
    if (ap != NULL && cJSON_IsString(platformj) && (platformj->valuestring != NULL))
    {
        strncpy(ap->platform, platformj->valuestring, META_LENGTH);
    }

    cJSON *rssi_one_meter = cJSON_GetObjectItemCaseSensitive(djson, CJ_RSSI_ONE_METER);
    if (ap != NULL && cJSON_IsNumber(rssi_one_meter))
    {
        ap->rssi_one_meter = rssi_one_meter->valueint;
    }

    cJSON *rssi_factor = cJSON_GetObjectItemCaseSensitive(djson, CJ_RSSI_FACTOR);
    if (ap != NULL && cJSON_IsNumber(rssi_one_meter))
    {
        ap->rssi_factor = (float)rssi_factor->valuedouble;
    }

    cJSON *people_distance = cJSON_GetObjectItemCaseSensitive(djson, CJ_PEOPLE_DISTANCE);
    if (ap != NULL && cJSON_IsNumber(people_distance))
    {
        ap->people_distance = (float)people_distance->valuedouble;
    }

    cJSON *sequence = cJSON_GetObjectItemCaseSensitive(djson, CJ_SEQ);
    if (ap != NULL && cJSON_IsNumber(sequence))
    {
        int64_t seq = (int64_t)sequence->valuedouble;

        // Make sure we aren't dropping too many messages
        if (ap->sequence !=0 && 
            (seq - ap->sequence) > 1 &&
            (seq - ap->sequence) < 1E6)
        {
            g_warning("Missed %li messages from %s", (long)((seq - ap->sequence) - 1), ap->client_id);
        }
        ap->sequence = seq;
    }

    // DEVICE

    cJSON *mac = cJSON_GetObjectItemCaseSensitive(djson, CJ_MAC);
    if (cJSON_IsString(mac) && (mac->valuestring != NULL))
    {
        strncpy(device->mac, mac->valuestring, 18);
        int64_t mac64 = mac_string_to_int_64(mac->valuestring);
        device->mac64 = mac64;
    }

    cJSON *name = cJSON_GetObjectItemCaseSensitive(djson, CJ_NAME);
    if (cJSON_IsString(name) && (name->valuestring != NULL))
    {
        strncpy(device->name, name->valuestring, NAME_LENGTH);
    }

    cJSON *latest = cJSON_GetObjectItemCaseSensitive(djson, CJ_LATEST);
    if (cJSON_IsNumber(latest))
    {
        // TODO: Full date time serialization and deserialization
        device->latest_local = latest->valueint;
        device->latest_any = latest->valueint;
    }

    cJSON *distance = cJSON_GetObjectItemCaseSensitive(djson, CJ_DISTANCE);
    if (cJSON_IsNumber(distance))
    {
        device->distance = (float)distance->valuedouble;
    }

    cJSON *filtered_rssi = cJSON_GetObjectItemCaseSensitive(djson, CJ_FILTERED_RSSI);
    if (cJSON_IsNumber(filtered_rssi))
    {
        device->filtered_rssi.current_estimate = (float)filtered_rssi->valuedouble;
        device->filtered_rssi.last_estimate = (float)filtered_rssi->valuedouble;
    }

    cJSON *raw_rssi = cJSON_GetObjectItemCaseSensitive(djson, CJ_RAW_RSSI);
    if (cJSON_IsNumber(raw_rssi))
    {
        device->raw_rssi = (float)raw_rssi->valuedouble;
    }

    cJSON *count = cJSON_GetObjectItemCaseSensitive(djson, CJ_COUNT);
    if (cJSON_IsNumber(count))
    {
        device->count = count->valueint;
    }

    cJSON *earliest = cJSON_GetObjectItemCaseSensitive(djson, CJ_EARLIEST);
    if (cJSON_IsNumber(earliest))
    {
        // TODO: Full date time serialization and deserialization
        device->earliest = earliest->valueint;
    }

    cJSON *training = cJSON_GetObjectItemCaseSensitive(djson, CJ_TRAINING);
    device->is_training_beacon = cJSON_IsNumber(training);

    device->category = CATEGORY_UNKNOWN;
    cJSON *category = cJSON_GetObjectItemCaseSensitive(djson, CJ_CATEGORY);
    if (cJSON_IsString(category) && (category->valuestring != NULL))
    {
        device->category = category_to_int(category->valuestring);
    }

    device->address_type = RANDOM_ADDRESS_TYPE;
    cJSON *addrType = cJSON_GetObjectItemCaseSensitive(djson, CJ_ADDRESS_TYPE);
    if (cJSON_IsNumber(addrType))
    {
        device->address_type = addrType->valueint;
    }

    cJSON *temp = cJSON_GetObjectItemCaseSensitive(djson, CJ_NAME_TYPE);
    if (cJSON_IsNumber(temp))
    {
        device->name_type = (enum name_type)(temp->valueint);
    }
    else
    {
        device->name_type = nt_initial;
    }

    cJSON_Delete(djson);

    return ap;
}


/*
   Set name and name type if an improvement
*/
void set_name(struct Device* d, const char*value, enum name_type name_type)
{
    if (value)
    {
        if (d->name_type < name_type)
        {
            if (d->name_type == nt_initial)
            {
                g_info("  %s Set name to '%s' (%i)", d->mac, value, name_type);
            }
            else
            {
                g_info("  %s Upgraded name from '%s' to '%s' (%i->%i)", d->mac, d->name, value, d->name_type, name_type);
            }
                
            d->name_type = name_type;
            g_strlcpy(d->name, value, NAME_LENGTH);
        }
    }
    else {
        g_warning("value was null for set_name");
    }
}
