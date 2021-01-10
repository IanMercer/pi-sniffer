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
struct group* get_or_add_group(struct group** group_list, const char* group_name, const char* tags)
{
    char* group_name_m = strdup(group_name);
    url_slug(group_name_m);  // destructive

    char* tags_m = strdup(tags);
    url_slug(tags_m);       // destructive

    for (struct group* current = *group_list; current != NULL; current = current->next)
    {
        if (strcmp(current->name, group_name) == 0 &&
            strcmp(current->tags, tags) == 0)
            {
                free(group_name_m);
                free(tags_m);
                return current;
            }
    }
    // Otherwise add a new group
    struct group* group = g_malloc(sizeof(struct group));
    group->name = group_name_m;
    group->tags = tags_m;
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
struct patch* get_or_create_patch(const char* patch_name, const char* room_name, const char* group_name, const char* tags,
    struct patch** patch_list, struct group** groups_list, bool confirmed)
{
    g_assert(patch_name != NULL);
    g_assert(group_name != NULL);
    g_assert(tags != NULL);

    // Work with a copy because we are destructive
    char* patch_name_m = g_strdup(patch_name);
    url_slug(patch_name_m);   // destructive

    struct patch* found = NULL;
    for (struct patch* r = *patch_list; r != NULL; r=r->next)
    {
        if (strcmp(r->name, patch_name_m) == 0)
        {
            found = r;
            break;
        }
    }

    if (found == NULL)
    {
        found = g_malloc(sizeof(struct patch));
        found->name = patch_name_m;
        found->next = NULL;
        found->group = NULL;
        found->room = url_slug(strdup(room_name));
        found->confirmed = confirmed;
        // no strdup here, get_or_add_group handles that
        found->group = get_or_add_group(groups_list, group_name, tags);
        g_info("Added patch %s in %s with tags %s", found->name, group_name, tags);

        found->phone_total = 0;
        found->beacon_total = 0;
        found->computer_total = 0;
        found->tablet_total = 0;
        found->watch_total = 0;
        found->wearable_total = 0;
        found->covid_total = 0;
        found->other_total = 0;

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
        // These have all been 'slugged', need a slugged_equals method
        // // If a definition is split across two files everything must align
        // if (strcmp(found->room, room_name) != 0)
        // {
        //     g_warning("Patch '%s' found in two rooms: '%s' and '%s', ignoring latter", patch_name, found->room, room_name);
        // }
        // if (strcmp(found->group->name, group_name) != 0)
        // {
        //     g_warning("Patch '%s' found in two groups: '%s' and '%s'", patch_name, found->group->name, group_name);
        // }
        // if (strcmp(found->group->tags, tags) != 0)
        // {
        //     g_warning("Patch '%s' found in group: '%s' has different tags, ignoring '%s'", patch_name, found->group->name, tags);
        // }
        free(patch_name_m);
    }

    return found;
}


/*
    Initalize the patches database (linked lists)
*/
void read_configuration_file(const char* path, struct AccessPoint** accesspoint_list, struct Beacon** beacon_list)
{
    (void)accesspoint_list; // no longer used
    // Read from file ...

    FILE *fp;

    fp = fopen(path, "r");

    if (fp == NULL)
    {
        g_warning("Did not find configuration file '%s'", path);
        // If no file, calculate from access points
        g_warning("Please create a configuration file '/etc/sniffer/config.json'");
        g_warning("You can change the location using systemctl edit if necessary.");
        g_warning("This file contains information about all the other sensors in the system and any named beacons");
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

    // cJSON* accesspoints = cJSON_GetObjectItemCaseSensitive(json, "sensors");
    // if (!cJSON_IsArray(accesspoints)){
    //     g_warning("Could not parse sensors[] from configuration file");
    //     // non-fatal
    // }
    // else
    // {
    //     cJSON* accesspoint = NULL;
    //     cJSON_ArrayForEach(accesspoint, accesspoints)
    //     {
    //         cJSON* name = cJSON_GetObjectItemCaseSensitive(accesspoint, "name");
    //         if (cJSON_IsString(name) && (name->valuestring != NULL))
    //         {
    //             g_debug("Added sensor %s", name->valuestring);
    //             add_access_point(accesspoint_list, name->valuestring, "not seen yet", "not seen yet", 64, 2.8, 7.5);
    //         }
    //         else
    //         {
    //             g_warning("Missing name field on sensor object");
    //         }
    //     }
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
                //g_warning("Added beacon `%s` = '%s' to list", beacon->name, beacon->alias);
            }
            else
            {
                g_warning("Missing name field on beacon object in configuration file");
            }
        }
    }

    cJSON_Delete(json);
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
        if (p->confirmed)
        {
            update_summary(summary, p->room, p->group->name, p->phone_total, p->tablet_total, p->computer_total, p->watch_total, 
                p->wearable_total, p->beacon_total, p->covid_total, p->other_total);
        }
    }
}


/*
    summarize_by_area
*/
void summarize_by_group(struct patch* patches, struct summary** summary)
{
    for (struct patch* p = patches; p != NULL; p = p->next)
    {
        if (p->confirmed)
        {
            update_summary(summary, p->group->name, p->group->tags, p->phone_total, p->tablet_total, p->computer_total, p->watch_total, 
                p->wearable_total, p->beacon_total, p->covid_total, p->other_total);
        }
    }
}