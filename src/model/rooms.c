/*
    Rooms aka Patches
*/

#include "rooms.h"
#include "accesspoints.h"
#include "cJSON.h"
#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <glib.h>
#include <string.h>
#include "utility.h"

/*
    Get or add a group
*/
struct group* get_or_add_area(struct group** group_list, char* group_name, char* tags)
{
    for (struct group* current = *group_list; current != NULL; current = current->next)
    {
        if (strcmp(current->name, group_name) == 0 &&
            strcmp(current->tags, tags) == 0) return current;
    }
    // Otherwise add a new group
    struct group* group = g_malloc(sizeof(struct group));
    group->name = strdup(group_name);
    group->tags = strdup(tags);
    group->next = NULL;

    if (*group_list == NULL)
    {
        *group_list = group;
        return group;
    }
    else
    {
        // Insert it at front (easier than traversing to end)
        group->next = *group_list;
        *group_list = group;
        return group;
    }
}


/*
   get or create a room and update any existing group also
*/
struct patch* get_or_create_patch(char* patch_name, char* room_name, char* group_name, char* tags,
    struct patch** patch_list, struct group** groups_list)
{
    g_assert(tags != NULL);
    g_assert(patch_name != NULL);

    if (string_contains_insensitive(tags, " "))
    {
        g_warning("Spaces not allowed in tags for area '%s' (removed)", patch_name);
        url_slug(tags);
    }

    if (string_contains_insensitive(patch_name, " "))
    {
        g_warning("Spaces not allowed in patch names '%s' (removed)", patch_name);
        url_slug(patch_name);   // destructive
    }

    struct patch* found = NULL;
    for (struct patch* r = *patch_list; r != NULL; r=r->next)
    {
        if (strcmp(r->name, patch_name) == 0)
        {
            found = r;
            break;
        }
    }

    if (found == NULL)
    {
        found = g_malloc(sizeof(struct patch));
        found->name = strdup(patch_name);
        found->next = NULL;
        found->group = NULL;
        found->room = strdup(url_slug(room_name));
        // no strdup here, get_or_add_group handles that
        found->group = get_or_add_area(groups_list, url_slug(group_name), tags);
        g_info("Added patch %s in %s with tags %s", found->name, group_name, tags);

        if (*patch_list == NULL)
        {
            // First item in chain
            *patch_list = found;
        }
        else
        {
            // Insert at front of chain
            found->next = *patch_list;
            *patch_list = found;
        }
    }
    else
    {
        if (strcmp(found->group->name, group_name) != 0 || strcmp(found->group->tags, tags) != 0)
        {
            g_warning("Patch '%s' changing group from '%s' to '%s'", patch_name, found->group->name, group_name);
            struct group* group = get_or_add_area(groups_list, group_name, tags);
            found->group = group;
        }
    }

    return found;
}


/*
    Initalize the patches database (linked lists)
*/
void read_configuration_file(const char* path, struct AccessPoint** accesspoint_list, struct patch** patch_list, struct group** area_list, struct Beacon** beacon_list)
{
    (void)patch_list;
    (void)area_list;

    // Read from file ...

    FILE *fp;

    fp = fopen(path, "r");

    if (fp == NULL)
    {
        g_warning("Did not find configuration file '%s'", path);
        // If no file, calculate from access points
        g_warning("Please create a configuration file 'rooms.json' (configured path using systemctl edit) mapping patch names to groups");
        return;
    }

    fseek (fp, 0, SEEK_END);
    long length = ftell (fp);

    if (length < 1){
        g_error("The '%s' file must contain entries for each patch, sensor and beacon", path);
        exit(EXIT_FAILURE);
    }

    fseek (fp, 0, SEEK_SET);
    char* buffer = g_malloc (length+1);

    long count = fread (buffer, 1, length, fp);
    g_assert(count == length);
    buffer[count] = '\0';

    cJSON *json = cJSON_Parse(buffer);
    if (json == NULL)
    {
        const char *error_ptr = cJSON_GetErrorPtr();
        if (error_ptr != NULL)
        {
            g_error("Error reading '%s' before: %s", path, error_ptr);
        }
        exit(EXIT_FAILURE);
    }


    // -------------- access points ---------------

    cJSON* accesspoints = cJSON_GetObjectItemCaseSensitive(json, "sensors");
    if (!cJSON_IsArray(accesspoints)){
        g_warning("Could not parse sensors[] from configuration file");
        // non-fatal
    }
    else
    {
        cJSON* accesspoint = NULL;
        cJSON_ArrayForEach(accesspoint, accesspoints)
        {
            cJSON* name = cJSON_GetObjectItemCaseSensitive(accesspoint, "name");
            if (cJSON_IsString(name) && (name->valuestring != NULL))
            {
                g_debug("Added sensor %s", name->valuestring);
                add_access_point(accesspoint_list, name->valuestring, "not seen yet", "not seen yet", 64, 2.8, 7.5);
            }
            else
            {
                g_warning("Missing name field on sensor object");
            }
        }
    }


    // // ------------------- patches ---------------------

    // cJSON* patches = cJSON_GetObjectItemCaseSensitive(json, "patches");
    // if (!cJSON_IsArray(patches)){
    //     g_error("Could not parse patches[] from configuration file '%s'", path);
    //     exit(EXIT_FAILURE);
    // }

    // cJSON* patch = NULL;
    // cJSON_ArrayForEach(patch, patches)
    // {
    //     cJSON* name = cJSON_GetObjectItemCaseSensitive(patch, "name");
    //     if (!cJSON_IsString(name) || (name->valuestring == NULL))
    //     {
    //         if (cJSON_GetObjectItemCaseSensitive(patch, "comment")) continue;
    //         g_error("Missing 'name' on patch object");
    //         continue;
    //     }

    //     cJSON* category = cJSON_GetObjectItemCaseSensitive(patch, "category");
    //     if (!cJSON_IsString(category) || (category->valuestring == NULL))
    //     {
    //         g_error("Missing 'category' on patch '%s'", name->valuestring);
    //         continue;
    //     }

    //     cJSON* tags = cJSON_GetObjectItemCaseSensitive(patch, "tags");
    //     if (!cJSON_IsString(tags) || (tags->valuestring == NULL))
    //     {
    //         g_warning("Missing 'tags' on patch '%s'", name->valuestring);
    //         continue;
    //     }

    //     if (string_contains_insensitive(tags->valuestring, " "))
    //     {
    //         g_warning("Spaces not allowed in tags for patch '%s'", name->valuestring);
    //         continue;
    //     }

    //     g_debug("Get or create patch '%s', '%s', '%s'", name->valuestring, category->valuestring, tags->valuestring);
    //     get_or_create_patch(name->valuestring, room_name->valuestring, category->valuestring, tags->valuestring, patch_list, area_list);        
    // }

    // ------------------- beacons --------------------

    cJSON* beacons = cJSON_GetObjectItemCaseSensitive(json, "beacons");
    if (!cJSON_IsArray(beacons)){
        g_warning("Could not parse beacons[] from configuration file");
        // non-fatal
    }
    else
    {
        cJSON* beacon = NULL;
        cJSON_ArrayForEach(beacon, beacons)
        {
            cJSON* name = cJSON_GetObjectItemCaseSensitive(beacon, "name");
            cJSON* mac = cJSON_GetObjectItemCaseSensitive(beacon, "mac");
            cJSON* alias = cJSON_GetObjectItemCaseSensitive(beacon, "alias");
            if (cJSON_IsString(name) && name->valuestring != NULL && 
                cJSON_IsString(mac) && mac->valuestring != NULL &&
                cJSON_IsString(alias) && alias->valuestring != NULL)
            {
                struct Beacon* beacon = malloc(sizeof(struct Beacon));
                beacon->name = strdup(name->valuestring);
                beacon->mac64 = mac_string_to_int_64(mac->valuestring);
                beacon->alias = strdup(alias->valuestring);
                beacon->last_seen = 0;
                beacon->patch = NULL;
                beacon->next = *beacon_list;
                *beacon_list = beacon;
                g_warning("Added beacon `%s` = '%s' to list", beacon->name, beacon->alias);
            }
            else
            {
                g_warning("Missing name field on beacon object in configuration file");
            }
        }
    }

    cJSON_Delete(json);

    g_debug("free buffer");
    g_free(buffer);

    fclose(fp);
}


/*
    Get top k patches sorted by total, return count found
*/
int top_k_by_patch_score(struct patch* result[], int k, struct patch* patch_list)
{
    for (int i = 0; i < k; i++)
    {
        result[i] = NULL;
    }

    int count = 0;
    for (struct patch* patch = patch_list; patch != NULL; patch = patch->next)
    {
        struct patch* current = patch;  // take a copy before we mangle it
        for (int i = 0; i < k; i++)
        {
            if (i == count)
            {
                // Off the end, but still < k, so add the item here
                count++;
                result[i] = current;
                break;
            }
            else if (result[i]->knn_score < current->knn_score)
            {
                // Insert at this position, pick up current and move it down
                struct patch* temp = result[i];
                result[i] = current;
                current = temp;
            }
            else
            {
                // keep going
            }
        }
    }
    return count;
}

/*
    summarize_by_room
*/
void summarize_by_room(struct patch* patches, struct summary** summary)
{
    for (struct patch* p = patches; p != NULL; p = p->next)
    {
        update_summary(summary, p->room, p->group->tags, p->phone_total, p->tablet_total, p->computer_total, p->watch_total, p->wearable_total, p->beacon_total);
    }
}


/*
    summarize_by_area
*/
void summarize_by_group(struct patch* patches, struct summary** summary)
{
    for (struct patch* p = patches; p != NULL; p = p->next)
    {
        update_summary(summary, p->group->name, p->group->tags, p->phone_total, p->tablet_total, p->computer_total, p->watch_total, p->wearable_total, p->beacon_total);
    }
}