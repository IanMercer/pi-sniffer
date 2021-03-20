#include "../model/device.h"
#include "utility.h"
#include "../model/accesspoints.h"
#include "cJSON.h"
#include <glib.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/types.h>
#include <math.h>

#include "serialization.h"


/*
*  Send full access point statistics occasionally to all other mesh devices
*/
char* access_point_to_json (struct AccessPoint* a)
{
    char *string = NULL;
    cJSON *j = cJSON_CreateObject();

    // AccessPoint details
    cJSON_AddStringToObject(j, CJ_FROM, a->client_id);
    cJSON_AddStringToObject(j, CJ_SHORT, a->short_client_id);
    cJSON_AddStringToObject(j, CJ_DESCRIPTION, a->description);
    cJSON_AddStringToObject(j, CJ_PLATFORM, a->platform);
    cJSON_AddRounded(j, CJ_RSSI_ONE_METER, a->rssi_one_meter);
    cJSON_AddRounded(j, CJ_RSSI_FACTOR, a->rssi_factor);
    cJSON_AddRounded(j, CJ_PEOPLE_DISTANCE, a->people_distance);
    if (a->ap_class != ap_class_unknown)
    {
        cJSON_AddNumberToObject(j, CJ_AP_CLASS, a->ap_class);
    }
    if (!isnan(a->temperature)) cJSON_AddRounded(j, CJ_TEMPERATURE, a->temperature);
    if (!isnan(a->internal_temperature)) cJSON_AddRounded(j, CJ_INTERNAL_TEMPERATURE, a->internal_temperature);
    if (!isnan(a->humidity)) cJSON_AddRounded(j, CJ_HUMIDITY, a->humidity);
    if (!isnan(a->brightness)) cJSON_AddRounded(j, CJ_BRIGHTNESS, a->brightness);
    if (a->carbon_dioxide != 0) cJSON_AddRounded(j, CJ_CARBON_DIOXIDE, a->carbon_dioxide);
    if (a->wifi_signal != 0) cJSON_AddRounded(j, CJ_WIFI, a->wifi_signal);

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

    // Minimal AccessPoint details
    cJSON_AddStringToObject(j, CJ_FROM, a->client_id);
    //cJSON_AddStringToObject(j, CJ_SHORT, a->short_client_id);
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
    if (device->known_interval > 0)
    {
        cJSON_AddNumberToObject(j, CJ_KNOWN_INTERVAL, device->known_interval);
    }
    if (device->is_training_beacon)
    {
        cJSON_AddNumberToObject(j, CJ_TRAINING, 1);
    }

    string = cJSON_PrintUnformatted(j);
    cJSON_Delete(j);
    return string;
}



struct AccessPoint* device_from_json(const char* json, struct OverallState* state, struct Device* device)
{
    cJSON *djson = cJSON_Parse(json);

    // ACCESS POINT
    struct AccessPoint* ap = NULL;

    if (djson == NULL)
    {
        g_warning("Failed to parse %s", json);
        return NULL;
    }

    cJSON *fromj = cJSON_GetObjectItemCaseSensitive(djson, CJ_FROM);
    if (cJSON_IsString(fromj) && (fromj->valuestring != NULL))
    {
        char* apname = fromj->valuestring;

        int64_t maybeMac = is_mac(apname) ? mac_string_to_int_64(apname) : 0;

        // Use beacon array to also map sensor names to better names
        // As ESP32 sensors will not have nice names
        for (struct AccessMapping* mapping = state->access_mappings; mapping != NULL; mapping = mapping->next)
        {
            if (g_ascii_strcasecmp(mapping->name, apname) == 0 || (maybeMac != 0 && mapping->mac64 == maybeMac))
            {
                apname = mapping->alias;
                break;
            }
        }

        bool created = FALSE;
        ap = get_or_create_access_point(state, apname, &created);

        if (ap == NULL)
        {
            g_warning("Why is ap null for %s", apname);
            return NULL;
        }

        time(&ap->last_seen);
    }
    else
    {
        g_warning("Did not find from field in json");
        return NULL;
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

    cJSON *ap_class_j = cJSON_GetObjectItemCaseSensitive(djson, CJ_AP_CLASS);
    if (ap != NULL && cJSON_IsNumber(ap_class_j))
    {
        ap->ap_class = (enum ap_class)ap_class_j->valueint;
    }

    // ----------------

    cJSON *j_internal_temperature = cJSON_GetObjectItemCaseSensitive(djson, CJ_INTERNAL_TEMPERATURE);
    if (ap != NULL && cJSON_IsNumber(j_internal_temperature))
    {
        ap->internal_temperature = (float)j_internal_temperature->valuedouble;
        if (ap->internal_temperature > 99) ap->internal_temperature = 99.0; // 99c
        if (ap->internal_temperature < 0) ap->internal_temperature = 0.0;
    }

    cJSON *j_temperature = cJSON_GetObjectItemCaseSensitive(djson, CJ_TEMPERATURE);
    if (ap != NULL && cJSON_IsNumber(j_temperature))
    {
        ap->temperature = (float)j_temperature->valuedouble;
        if (ap->temperature > 99) ap->temperature = 99.0;
        if (ap->temperature < -40) ap->temperature = -40.0;
    }

    cJSON *j_humidity = cJSON_GetObjectItemCaseSensitive(djson, CJ_HUMIDITY);
    if (ap != NULL && cJSON_IsNumber(j_humidity))
    {
        ap->humidity = (float)j_humidity->valuedouble;
    }

    cJSON *j_pressure = cJSON_GetObjectItemCaseSensitive(djson, CJ_PRESSURE);
    if (ap != NULL && cJSON_IsNumber(j_pressure))
    {
        ap->pressure = (float)j_pressure->valuedouble;
    }

    cJSON *j_co2 = cJSON_GetObjectItemCaseSensitive(djson, CJ_CARBON_DIOXIDE);
    if (ap != NULL && cJSON_IsNumber(j_co2))
    {
        ap->carbon_dioxide = j_co2->valueint;
    }

    cJSON *j_voc = cJSON_GetObjectItemCaseSensitive(djson, CJ_VOC);
    if (ap != NULL && cJSON_IsNumber(j_voc))
    {
        ap->voc = (float)j_voc->valuedouble;
    }

    cJSON *j_wifi = cJSON_GetObjectItemCaseSensitive(djson, CJ_WIFI);
    if (ap != NULL && cJSON_IsNumber(j_wifi))
    {
        ap->wifi_signal = j_wifi->valueint;
    }

    // ----------------

    cJSON *sequence = cJSON_GetObjectItemCaseSensitive(djson, CJ_SEQ);
    if (ap != NULL && cJSON_IsNumber(sequence))
    {
        int64_t seq = (int64_t)sequence->valuedouble;

        // Make sure we aren't dropping too many messages
        if (ap->sequence !=0 && 
            (seq - ap->sequence) > 1 &&
            (seq - ap->sequence) < 1E6)
        {
            g_warning("Missed %li messages from %s", (long)((seq - ap->sequence) - 1), ap->short_client_id);
            state->messagesMissed += (long)((seq - ap->sequence) - 1);
        }
        state->messagesReceived++;
        ap->sequence = seq;
    }

    // DEVICE

    // ESP32 doesn't send name or count or category
    device->name[0] = '\0';
    device->name_type = nt_initial;
    device->count = 0;
    device->try_connect_attempts = 0;
    device->try_connect_state = TRY_CONNECT_ZERO;

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

    cJSON *latestj = cJSON_GetObjectItemCaseSensitive(djson, CJ_LATEST);
    if (cJSON_IsNumber(latestj))
    {
        // TODO: Full date time serialization and deserialization
        device->latest_local = latestj->valueint;
        device->latest_any = latestj->valueint;
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

    cJSON *countj = cJSON_GetObjectItemCaseSensitive(djson, CJ_COUNT);
    if (cJSON_IsNumber(countj))
    {
        device->count = countj->valueint;
    }

    cJSON *tryConnectj = cJSON_GetObjectItemCaseSensitive(djson, CJ_TRY_CONNECT_STATE);
    if (cJSON_IsNumber(tryConnectj))
    {
        // This will only be accepted if it's complete (see merge method)
        device->try_connect_state = tryConnectj->valueint;
    }

    cJSON *intervalj = cJSON_GetObjectItemCaseSensitive(djson, CJ_KNOWN_INTERVAL);
    if (cJSON_IsNumber(intervalj))
    {
        device->known_interval = intervalj->valueint;
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

