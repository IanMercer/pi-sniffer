#include "device.h"
#include "cJSON.h"
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/types.h>


/*
    Convert device to JSON string
    See https://github.com/DaveGamble/cJSON
*/

char* device_to_json (struct Device* device, const char* from)
{
    char *string = NULL;
    cJSON *j = cJSON_CreateObject();

    cJSON_AddStringToObject(j, "from", from);
    cJSON_AddStringToObject(j, "mac", device->mac);
    cJSON_AddStringToObject(j, "name", device->name);
    cJSON_AddStringToObject(j, "alias", device->alias);
    cJSON_AddNumberToObject(j, "addressType", device->addressType);
    cJSON_AddStringToObject(j, "category", device->category);

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

   cJSON_Delete(djson);

   return true;
}
