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
struct area* get_or_add_area(struct area** group_list, char* group_name, char* tags)
{
    for (struct area* current = *group_list; current != NULL; current = current->next)
    {
        if (strcmp(current->category, group_name) == 0 &&
            strcmp(current->tags, tags) == 0) return current;
    }
    // Otherwise add a new group
    struct area* group = g_malloc(sizeof(struct area));
    group->category = strdup(group_name);
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
struct room* get_or_create_room(char* room_name, char* group_name, char* tags, struct room** rooms_list, struct area** groups_list)
{
    if (string_contains_insensitive(tags, " "))
    {
        g_warning("Spaces not allowed in tags for area '%s' (removed)", room_name);
        url_slug(tags);
    }

    if (string_contains_insensitive(room_name, " "))
    {
        g_warning("Spaces not allowed in room name '%s' (removed)", room_name);
        url_slug(room_name);   // destructive
    }

    struct room* found = NULL;
    for (struct room* r = *rooms_list; r != NULL; r=r->next)
    {
        if (strcmp(r->name, room_name) == 0)
        {
            found = r;
            break;
        }
    }

    if (found == NULL)
    {
        found = g_malloc(sizeof(struct room));
        found->name = strdup(room_name);
        found->next = NULL;
        found->area = NULL;
        g_info("Added room %s in %s with tags %s", found->name, group_name, tags);

        if (*rooms_list == NULL)
        {
            // First item in chain
            *rooms_list = found;
        }
        else
        {
            // Insert at front of chain
            found->next = *rooms_list;
            *rooms_list = found;
        }

        // no strdup here, get_or_add_group handles that
        found->area = get_or_add_area(groups_list, url_slug(group_name), tags);
    }
    else
    {
        if (strcmp(found->area->category, group_name) != 0)
        {
            g_warning("TODO: Room '%s' changing group from '%s' to '%s'", room_name, found->area->category, group_name);
        }

        if (strcmp(found->area->tags, tags) != 0)
        {
            g_warning("TODO: Room '%s' changing tags from '%s' to '%s'", room_name, found->area->tags, tags);
        }
    }

    return found;
}


/*
    Initalize the rooms database (linked list)
*/
void read_configuration_file(const char* path, struct room** room_list, struct area** group_list, struct AccessPoint** access_points_list)
{
    (void)access_points_list; // unused now
    // Read from file ...

    FILE *fp;

    fp = fopen(path, "r");

    if (fp == NULL)
    {
        // If no file, calculate from access points
        g_warning("Please create a file 'rooms.json' (configured path using systemctl edit) mapping room names to groups");
        return;
    }

    fseek (fp, 0, SEEK_END);
    long length = ftell (fp);

    if (length < 1){
        g_error("The 'rooms.json' file must contain entries for each room");
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

    cJSON* rooms = cJSON_GetObjectItemCaseSensitive(json, "areas");
    if (!cJSON_IsArray(rooms)){
        g_error("Could not parse rooms[] from 'rooms.json'");
        exit(EXIT_FAILURE);
    }

    cJSON* room = NULL;
    cJSON_ArrayForEach(room, rooms)
    {
        // Parse room from 'room'

        cJSON* name = cJSON_GetObjectItemCaseSensitive(room, "name");
        if (!cJSON_IsString(name) || (name->valuestring == NULL))
        {
            if (cJSON_GetObjectItemCaseSensitive(room, "comment")) continue;
            g_error("Missing 'name' on room object");
            continue;
        }

        cJSON* category = cJSON_GetObjectItemCaseSensitive(room, "group");
        if (!cJSON_IsString(category) || (category->valuestring == NULL))
        {
            g_error("Missing 'group' on area '%s'", name->valuestring);
            continue;
        }

        cJSON* tags = cJSON_GetObjectItemCaseSensitive(room, "tags");
        if (!cJSON_IsString(tags) || (tags->valuestring == NULL))
        {
            g_warning("Missing 'tags' on area '%s'", name->valuestring);
            continue;
        }

        if (string_contains_insensitive(tags->valuestring, " "))
        {
            g_warning("Spaces not allowed in tags for area '%s'", name->valuestring);
            continue;
        }

        g_debug("Get or create room '%s', '%s', '%s'", name->valuestring, category->valuestring, tags->valuestring);
        get_or_create_room(name->valuestring, category->valuestring, tags->valuestring, room_list, group_list);        
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
