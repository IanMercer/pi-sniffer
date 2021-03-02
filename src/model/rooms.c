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
    // tags should not be slugged, it contains = and ,

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
        //g_info("Added patch %s in %s with tags %s", found->name, group_name, tags);

        found->phone_total = 0;
        found->beacon_total = 0;
        found->computer_total = 0;
        found->tablet_total = 0;
        found->watch_total = 0;
        found->wearable_total = 0;
        found->covid_total = 0;
        found->other_total = 0;

        // Insert at front of chain
        found->next = *patch_list;
        *patch_list = found;

        g_debug("Added patch %s", found->name);
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

#define CONFIG_DIR "/etc/sniffer/"


void handle_access_translation_jsonl(const char * line, void* params)
{
    struct OverallState* state = (struct OverallState*) params;

    cJSON *acccess_translation = cJSON_Parse(line);
    if (acccess_translation == NULL)
    {
        const char *error_ptr = cJSON_GetErrorPtr();
        if (error_ptr != NULL)
        {
            g_error("Error reading '%s' before: %s", line, error_ptr);
        }
        return;
    }

    cJSON* name = cJSON_GetObjectItemCaseSensitive(acccess_translation, "name");
    cJSON* mac = cJSON_GetObjectItemCaseSensitive(acccess_translation, "mac");
    cJSON* alias = cJSON_GetObjectItemCaseSensitive(acccess_translation, "alias");
    if (cJSON_IsString(name) && name->valuestring != NULL && 
        cJSON_IsString(mac) && mac->valuestring != NULL &&
        cJSON_IsString(alias) && alias->valuestring != NULL)
    {
        struct AccessMapping* apt = malloc(sizeof(struct AccessMapping));
        apt->name = strdup(name->valuestring);
        apt->mac64 = mac_string_to_int_64(mac->valuestring);
        apt->alias = strdup(alias->valuestring);

        // Insertion sort into beacon list
        struct AccessMapping* previous = NULL;

        for (struct AccessMapping* b = state->access_mappings; b != NULL; b=b->next)
        {
            if (strcmp(b->alias, apt->alias) == 0)
            {
                // No duplicate access mappings, just ignore them == idempotent for adds anyway
                return;
            }

            if (strcmp(b->alias, apt->alias) > 0)
            {
                // Insert here after previous
                break;
            }
            previous = b;
        }

        if (previous == NULL)
        {
            // Chain onto start of list
            apt->next = state->access_mappings;
            state->beacons = apt;
        }
        else
        {
            // Insert it in order
            apt->next = previous->next;
            previous->next = apt;
        }

        g_debug("Added access mapping `%s` = '%s' to list", apt->name, apt->alias);
    }
    else
    {
        g_warning("Missing name field on access mapping '%s'", line);
    }

    cJSON_Delete(acccess_translation);
}

void handle_beacon_jsonl(const char * line, void* params)
{
    struct OverallState* state = (struct OverallState*) params;

    cJSON *beacon = cJSON_Parse(line);
    if (beacon == NULL)
    {
        const char *error_ptr = cJSON_GetErrorPtr();
        if (error_ptr != NULL)
        {
            g_error("Error reading '%s' before: %s", line, error_ptr);
        }
        return;
    }

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

        // Insertion sort into beacon list
        struct Beacon* previous = NULL;

        for (struct Beacon* b = state->beacons; b != NULL; b=b->next)
        {
            if (strcmp(b->alias, beacon->alias) == 0)
            {
                // No duplicate beacons, just ignore them == idempotent for adds anyway
                return;
            }

            if (strcmp(b->alias, beacon->alias) > 0)
            {
                // Insert here after previous
                break;
            }
            previous = b;
        }

        if (previous == NULL)
        {
            // Chain onto start of list
            beacon->next = state->beacons;
            state->beacons = beacon;
        }
        else
        {
            // Insert it in order
            beacon->next = previous->next;
            previous->next = beacon;
        }

        g_debug("Added beacon `%s` = '%s' to list", beacon->name, beacon->alias);
    }
    else
    {
        g_warning("Missing name field on beacon object '%s'", line);
    }

    cJSON_Delete(beacon);
}


/*
    Initalize the patches database (linked lists)
*/
void read_accesspoint_name_translations(struct OverallState* state)
{
    struct Beacon** access_mappings = &state->access_mappings;
    read_all_lines(CONFIG_DIR, "access.jsonl", &handle_access_translation_jsonl, (void*)state);
}

/*
    Initalize the patches database (linked lists)
*/
void read_configuration_files(struct OverallState* state)
{
    bool ok = read_all_lines(CONFIG_DIR, "config.json", &handle_beacon_jsonl, (void*)state);
    g_debug("%i", ok);

    // New file name
    ok = read_all_lines(CONFIG_DIR, "beacons.jsonl", &handle_beacon_jsonl, (void*)state);
    g_debug("%i", ok);

    ok = read_all_lines(CONFIG_DIR, "access_mappings.json", &handle_access_translation_jsonl, (void*)state);
    g_debug("%i", ok);
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
            // group tags are fairly useless
            update_summary(summary, p->group->name, "", p->phone_total, p->tablet_total, p->computer_total, p->watch_total, 
                p->wearable_total, p->beacon_total, p->covid_total, p->other_total);
        }
    }
}