#include "device.h"
#include "cJSON.h"
#include <glib.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/types.h>

// These must be in same order as enum values
char* categories[] = { "unknown", "phone", "watch", "tablet", "headphones", "computer", "tv", "fixed", "beacon" };

int category_to_int(char* category)
{
    for (uint i = 0; i < sizeof(categories); i++) {
      if (strcmp(category, categories[i]) == 0) return i;
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

void soft_set(int8_t* category, int8_t category_new)
{
    if (*category == CATEGORY_UNKNOWN) *category = category_new;
}

void merge(struct Device* local, struct Device* remote)
{
   optional_set(local->name, remote->name);
   optional_set(local->alias, remote->alias);
   soft_set(&local->category, remote->category);
   // TODO: Other fields that we can transfer over
}

char* device_to_json (struct Device* device, const char* from)
{
    char *string = NULL;
    cJSON *j = cJSON_CreateObject();

    cJSON_AddStringToObject(j, "from", from);
    cJSON_AddStringToObject(j, "mac", device->mac);
    cJSON_AddStringToObject(j, "name", device->name);
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

bool device_from_json(const char* json, struct Device* device, char* from, int from_length)
{
    cJSON *djson = cJSON_Parse(json);

    cJSON *fromj = cJSON_GetObjectItemCaseSensitive(djson, "from");
    if (cJSON_IsString(fromj) && (fromj->valuestring != NULL))
    {
        strncpy(from, fromj->valuestring, from_length);
    }

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

