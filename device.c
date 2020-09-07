#include "device.h"
#include "utility.h"
#include "cJSON.h"
#include <glib.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/types.h>

// These must be in same order as enum values
char* categories[] = { "unknown", "phone", "watch", "tablet", "headphones", "computer", "tv", "fixed", "beacon", "car", "audio" };
int category_values[] = { CATEGORY_UNKNOWN, CATEGORY_PHONE, CATEGORY_WATCH, CATEGORY_TABLET, CATEGORY_HEADPHONES, CATEGORY_COMPUTER, CATEGORY_TV, CATEGORY_FIXED, CATEGORY_BEACON, CATEGORY_CAR, CATEGORY_AUDIO_CARD };

int category_to_int(char* category)
{
    for (uint i = 0; i < sizeof(categories); i++) {
       if (category_values[i] != (int)i) g_warning("Category does not match %i %i", i, category_values[i]);
       if (strcmp(category, categories[i]) == 0) return category_values[i];
    }
    return 0;
}

char* category_from_int(uint i)
{
  if (i >= sizeof(categories)) return categories[0];
  return categories[i];
}

void optional_set(char* name, char* value) {
  if (strlen(name)) return;
  g_strlcpy(name, value, NAME_LENGTH);
}

void soft_set_8(int8_t* field, int8_t field_new)
{
    // CATEGORY_UNKNOWN, UNKNOWN_ADDRESS_TYPE = 0
    if (*field == 0) *field = field_new;
}

void soft_set_u16(uint16_t* field, uint16_t field_new)
{
    // CATEGORY_UNKNOWN, UNKNOWN_ADDRESS_TYPE = 0
    if (*field == 0) *field = field_new;
}

void merge(struct Device* local, struct Device* remote, char* access_name)
{
   optional_set(local->name, remote->name);
   optional_set(local->alias, remote->alias);
   soft_set_8(&local->addressType, remote->addressType);
   if (local->category == 0 && remote->category != 0) g_info("  %s Changed category to '%s', message from %s", local->mac, category_from_int(remote->category), access_name);
   soft_set_8(&local->category, remote->category);
   soft_set_u16(&local->appearance, remote->appearance);  // not used ?
   if (remote->try_connect_state == 2) local->try_connect_state = 2;  // already connected once
   // TODO: Other fields that we can transfer over
}

char* access_point_to_json (struct AccessPoint* a)
{
    char *string = NULL;
    cJSON *j = cJSON_CreateObject();

    // AccessPoint details
    cJSON_AddStringToObject(j, "from", a->client_id);
    cJSON_AddStringToObject(j, "description", a->description);
    cJSON_AddStringToObject(j, "platform", a->platform);
    cJSON_AddNumberToObject(j, "from_x", a->x);
    cJSON_AddNumberToObject(j, "from_y", a->y);
    cJSON_AddNumberToObject(j, "from_z", a->z);
    cJSON_AddNumberToObject(j, "rssi_one_meter", a->rssi_one_meter);
    cJSON_AddNumberToObject(j, "rssi_factor", a->rssi_factor);
    cJSON_AddNumberToObject(j, "people_distance", a->people_distance);
    cJSON_AddNumberToObject(j, "people_closest_count", a->people_closest_count);
    cJSON_AddNumberToObject(j, "people_in_range_count", a->people_in_range_count);

    string = cJSON_Print(j);
    cJSON_Delete(j);
    return string;
}

char* device_to_json (struct AccessPoint* a, struct Device* device)
{
    char *string = NULL;
    cJSON *j = cJSON_CreateObject();

    // AccessPoint details
    cJSON_AddStringToObject(j, "from", a->client_id);
    cJSON_AddStringToObject(j, "description", a->description);
    cJSON_AddStringToObject(j, "platform", a->platform);
    cJSON_AddNumberToObject(j, "from_x", a->x);
    cJSON_AddNumberToObject(j, "from_y", a->y);
    cJSON_AddNumberToObject(j, "from_z", a->z);
    cJSON_AddNumberToObject(j, "rssi_one_meter", a->rssi_one_meter);
    cJSON_AddNumberToObject(j, "rssi_factor", a->rssi_factor);
    cJSON_AddNumberToObject(j, "people_distance", a->people_distance);
    cJSON_AddNumberToObject(j, "people_closest_count", a->people_closest_count);
    cJSON_AddNumberToObject(j, "people_in_range_count", a->people_in_range_count);

    // Device details
    cJSON_AddStringToObject(j, "mac", device->mac);
    cJSON_AddStringToObject(j, "name", device->name);

    char mac_super[18];
    mac_64_to_string(mac_super, 18, device->superceeds);
    cJSON_AddStringToObject(j, "superceeds", mac_super);

    cJSON_AddStringToObject(j, "alias", device->alias);
    cJSON_AddNumberToObject(j, "addressType", device->addressType);
    cJSON_AddStringToObject(j, "category", category_from_int(device->category));
    cJSON_AddNumberToObject(j, "manufacturer", device->manufacturer);
    cJSON_AddBoolToObject(j, "paired", device->paired);
    cJSON_AddBoolToObject(j, "connected", device->connected);
    cJSON_AddBoolToObject(j, "trusted", device->trusted);
    cJSON_AddNumberToObject(j, "deviceclass", device->deviceclass);
    cJSON_AddNumberToObject(j, "appearance", device->appearance);
    cJSON_AddNumberToObject(j, "manufacturer_data_hash", device->manufacturer_data_hash);
    cJSON_AddNumberToObject(j, "service_data_hash", device->service_data_hash);
    cJSON_AddNumberToObject(j, "uuids_length", device->uuids_length);
    cJSON_AddNumberToObject(j, "uuid_hash", device->uuid_hash);
    cJSON_AddNumberToObject(j, "txpower", device->txpower);
    cJSON_AddNumberToObject(j, "last_sent", device->last_sent);
    cJSON_AddNumberToObject(j, "distance", device->distance);
    cJSON_AddNumberToObject(j, "earliest", device->earliest);
    cJSON_AddNumberToObject(j, "latest", device->latest);
    cJSON_AddNumberToObject(j, "count", device->count);
    cJSON_AddNumberToObject(j, "column", device->column);
    cJSON_AddNumberToObject(j, "try_connect_state", device->try_connect_state);

    string = cJSON_Print(j);
    cJSON_Delete(j);
    return string;
}

bool device_from_json(const char* json, struct AccessPoint* access_point, struct Device* device)
{
    cJSON *djson = cJSON_Parse(json);

    // ACCESS POINT

    cJSON *from_x = cJSON_GetObjectItemCaseSensitive(djson, "from_x");
    if (cJSON_IsNumber(from_x))
    {
        access_point->x = (float)from_x->valuedouble;
    }

    cJSON *from_y = cJSON_GetObjectItemCaseSensitive(djson, "from_y");
    if (cJSON_IsNumber(from_y))
    {
        access_point->y = (float)from_y->valuedouble;
    }

    cJSON *from_z = cJSON_GetObjectItemCaseSensitive(djson, "from_z");
    if (cJSON_IsNumber(from_z))
    {
        access_point->z = (float)from_z->valuedouble;
    }

    cJSON *fromj = cJSON_GetObjectItemCaseSensitive(djson, "from");
    if (cJSON_IsString(fromj) && (fromj->valuestring != NULL))
    {
        strncpy(access_point->client_id, fromj->valuestring, META_LENGTH);
    }

    cJSON *descriptionj = cJSON_GetObjectItemCaseSensitive(djson, "description");
    if (cJSON_IsString(descriptionj) && (descriptionj->valuestring != NULL))
    {
        strncpy(access_point->description, descriptionj->valuestring, META_LENGTH);
    }

    cJSON *platformj = cJSON_GetObjectItemCaseSensitive(djson, "platform");
    if (cJSON_IsString(platformj) && (platformj->valuestring != NULL))
    {
        strncpy(access_point->platform, platformj->valuestring, META_LENGTH);
    }

    cJSON *rssi_one_meter = cJSON_GetObjectItemCaseSensitive(djson, "rssi_one_meter");
    if (cJSON_IsNumber(rssi_one_meter))
    {
        access_point->rssi_one_meter = rssi_one_meter->valueint;
    }

    cJSON *rssi_factor = cJSON_GetObjectItemCaseSensitive(djson, "rssi_factor");
    if (cJSON_IsNumber(rssi_one_meter))
    {
        access_point->rssi_factor = (float)rssi_factor->valuedouble;
    }

    cJSON *people_distance = cJSON_GetObjectItemCaseSensitive(djson, "people_distance");
    if (cJSON_IsNumber(people_distance))
    {
        access_point->people_distance = (float)people_distance->valuedouble;
    }

    cJSON *people_closest_count = cJSON_GetObjectItemCaseSensitive(djson, "people_closest_count");
    if (cJSON_IsNumber(people_closest_count))
    {
        access_point->people_closest_count = (float)people_closest_count->valuedouble;
    }

    cJSON *people_in_range_count = cJSON_GetObjectItemCaseSensitive(djson, "people_in_range_count");
    if (cJSON_IsNumber(people_in_range_count))
    {
        access_point->people_in_range_count = (float)people_in_range_count->valuedouble;
    }

    // DEVICE

    cJSON *mac = cJSON_GetObjectItemCaseSensitive(djson, "mac");
    if (cJSON_IsString(mac) && (mac->valuestring != NULL))
    {
        strncpy(device->mac, mac->valuestring, 18);
    }

    cJSON *name = cJSON_GetObjectItemCaseSensitive(djson, "name");
    if (cJSON_IsString(name) && (name->valuestring != NULL))
    {
        strncpy(device->name, name->valuestring, NAME_LENGTH);
    }

    cJSON *supercedes = cJSON_GetObjectItemCaseSensitive(djson, "supercedes");
    if (cJSON_IsString(supercedes))
    {
        // TODO: ulong serialization with cJSON
        device->superceeds = mac_string_to_int_64(supercedes->valuestring);
    }

    cJSON *latest = cJSON_GetObjectItemCaseSensitive(djson, "latest");
    if (cJSON_IsNumber(latest))
    {
        // TODO: Full date time serialization and deserialization
        device->latest = latest->valueint;
    }

    cJSON *distance = cJSON_GetObjectItemCaseSensitive(djson, "distance");
    if (cJSON_IsNumber(distance))
    {
        device->distance = (float)distance->valuedouble;
    }

    cJSON *earliest = cJSON_GetObjectItemCaseSensitive(djson, "earliest");
    if (cJSON_IsNumber(earliest))
    {
        // TODO: Full date time serialization and deserialization
        device->earliest = earliest->valueint;
    }

    device->category = CATEGORY_UNKNOWN;
    cJSON *category = cJSON_GetObjectItemCaseSensitive(djson, "category");
    if (cJSON_IsString(category) && (category->valuestring != NULL))
    {
        device->category = category_to_int(category->valuestring);
    }

   cJSON_Delete(djson);

   return true;
}

