/*
    Rooms
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
struct group* get_or_add_group(struct group** group_list, char* group_name)
{
    for (struct group* current = *group_list; current != NULL; current = current->next)
    {
        if (strcmp(current->name, group_name) == 0) return current;
    }
    // Otherwise add a new group
    struct group* group = g_malloc(sizeof(struct group));
    group->group_total = 0.;
    group->name = strdup(group_name);
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
    Initalize the rooms database (linked list)
*/
void read_configuration_file(const char* path, struct room** room_list, struct group** group_list, struct AccessPoint** access_points_list)
{
    (void)access_points_list; // unused now
    // Read from file ...

    FILE *fp;

    fp = fopen(path, "r");

    if (fp == NULL)
    {
        // If no file, calculate from access points
        g_warning("Please create a file 'rooms.json' containing the mapping from sensors to rooms");
        return;
    }

    fseek (fp, 0, SEEK_END);
    long length = ftell (fp);

    if (length < 1){
        g_error("The 'rooms.json' file must contain entries for each room mapping to distances");
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
            g_error("Error reading 'rooms.json' before: %s", error_ptr);
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
                add_access_point(access_points_list, name->valuestring, "not seen yet", "not seen yet", 0.0, 0.0, 0.0, 64, 2.8, 7.5);
            }
            else
            {
                g_warning("Missing name field on sensor object");
            }
        }
    }


    // ------------------- rooms ---------------------

    struct room* current_room = NULL;   // current pointer

    cJSON* rooms = cJSON_GetObjectItemCaseSensitive(json, "rooms");
    if (!cJSON_IsArray(rooms)){
        g_error("Could not parse rooms[] from 'rooms.json'");
        exit(EXIT_FAILURE);
    }

    cJSON* room = NULL;
    cJSON_ArrayForEach(room, rooms)
    {
        // Parse room from 'room'

        bool ok = TRUE;
        struct room* r = g_malloc(sizeof(struct room));
        r->next = NULL;

        cJSON* name = cJSON_GetObjectItemCaseSensitive(room, "name");
        if (cJSON_IsString(name) && (name->valuestring != NULL))
        {
            r->name = strdup(url_slug(name->valuestring));
        }
        else
        {
            g_error("Missing name on room object");
            ok = FALSE;
        }

        cJSON* group = cJSON_GetObjectItemCaseSensitive(room, "group");
        if (cJSON_IsString(group) && (group->valuestring != NULL))
        {
            // no strdup here, get_or_add_group handles that
            r->group = get_or_add_group(group_list, url_slug(group->valuestring));
        }
        else
        {
            g_error("Missing group on room '%s' object", r->name);
            ok = FALSE;
        }

        //g_debug("Parsed room %s in %s", r->name, r->group);
        if (ok)
        {
          g_debug("Added room %s", r->name);
          if (*room_list == NULL)
          {
              *room_list = r;
          }
          else
          {
              current_room->next = r;
          }
          current_room = r;
        }
        else
        {
            free(r);
        }
        
    }

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
            if (cJSON_IsString(name) && (name->valuestring != NULL))
            {
                g_warning("TODO: Add beacon `%s` to list", name->valuestring);
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
    Get top k rooms sorted by total, return count found
*/
int top_k_by_room_score(struct room* result[], int k, struct room* room_list)
{
    for (int i = 0; i < k; i++)
    {
        result[i] = NULL;
    }

    int count = 0;
    for (struct room* room = room_list; room != NULL; room = room->next)
    {
        struct room* current = room;  // take a copy before we mangle it
        for (int i = 0; i < k; i++)
        {
            if (i == count)
            {
                // Off the end, but still < k, so add the item here
                count++;
                result[i] = current;
                break;
            }
            else if (result[i]->room_score < current->room_score)
            {
                // Insert at this position, pick up current and move it down
                struct room* temp = result[i];
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
