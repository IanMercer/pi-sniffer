/*
    Access Points
*/

#include "device.h"
#include "utility.h"
#include <glib.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/types.h>



struct AccessPoint *add_access_point(struct AccessPoint** access_point_list, char *client_id,
                                     const char *description, const char *platform,
                                     float x, float y, float z, int rssi_one_meter, float rssi_factor, float people_distance)
{
    //g_debug("Check for new access point '%s'\n", client_id);
    bool created;
    struct AccessPoint* ap = get_or_create_access_point(access_point_list, client_id, &created);
    if (ap == NULL) return NULL;   // full

    if (created)
    {
        //g_debug("  initializing %i %s", ap->id, client_id);
        strncpy(ap->client_id, client_id, META_LENGTH);
        strncpy(ap->description, description, META_LENGTH);
        strncpy(ap->platform, platform, META_LENGTH);
        ap->x = x;
        ap->y = y;
        ap->z = z;
        ap->rssi_one_meter = rssi_one_meter;
        ap->rssi_factor = rssi_factor;
        ap->people_distance = people_distance;
        ap->people_closest_count = 0.0;
        ap->people_in_range_count = 0.0;
    }
    time(&ap->last_seen);

    return ap;
}

void print_access_points(struct AccessPoint* access_points_list)
{
    time_t now;
    time(&now);
    float people_total = 0.0;
    g_info("ACCESS POINTS          Platform       Close Range (x,y,z)                 Parameters         Last Seen");
    for (struct AccessPoint* ap = access_points_list; ap != NULL; ap = ap->next)
    {
        int delta_time = difftime(now, ap->last_seen);
        g_info("%20s %16s (%4.1f %4.1f) (%6.1f,%6.1f,%6.1f) (%3i, %.1f, %.1fm) %is",
        ap->client_id, ap->platform,
        ap->people_closest_count, ap->people_in_range_count,
        ap->x, ap->y, ap->z, ap->rssi_one_meter, ap->rssi_factor, ap->people_distance,
        delta_time);
        //g_print("              %16s %s\n", ap->platform, ap->description);
        people_total += ap->people_closest_count;
    }
    g_info("Total people = %.1f", people_total);
}

/*
    Identity map access point and update values
*/
struct AccessPoint *update_accessPoints(struct AccessPoint** access_point_list, struct AccessPoint access_point)
{
    struct AccessPoint* ap = add_access_point(access_point_list, access_point.client_id,
                            access_point.description, access_point.platform,
                            access_point.x, access_point.y, access_point.z,
                            access_point.rssi_one_meter, access_point.rssi_factor, access_point.people_distance);
    ap->people_closest_count = access_point.people_closest_count;
    ap->people_in_range_count = access_point.people_in_range_count;
    strncpy(ap->description, access_point.description, META_LENGTH);
    strncpy(ap->platform, access_point.platform, META_LENGTH);
    time(&access_point.last_seen);
    return ap;
}

/*
    Get access point by id
*/
struct AccessPoint *get_access_point(struct AccessPoint* access_point_list, int id)
{
    for (struct AccessPoint* current = access_point_list; current != NULL; current = current->next)
    {
        if (current->id == id)
        {
            return current;
        }
    }
    return NULL;
}

static int access_point_id_generator = 0;

/*
    Get or add an access point
*/
struct AccessPoint* get_or_create_access_point(struct AccessPoint** access_points_list, const char* client_id, bool* created)
{
    *created = FALSE;
    for (struct AccessPoint* current = *access_points_list; current != NULL; current = current->next)
    {
        if (strcmp(current->client_id, client_id) == 0) return current;
    }

    *created = TRUE;

    // Otherwise add a new one
    struct AccessPoint* ap = g_malloc(sizeof(struct AccessPoint));
    strncpy(ap->client_id, client_id, META_LENGTH);
    strncpy(ap->description, "Not known yet", META_LENGTH);
    ap->next = NULL;
    ap->id = access_point_id_generator++;

    if (*access_points_list == NULL)
    {
        *access_points_list = ap;
        return ap;
    }
    else // scan to find position
    {
        struct AccessPoint* previous = NULL;
        struct AccessPoint* current = *access_points_list;

        while (strcmp(current->client_id, client_id) < 0)
        {
            previous = current;
            current = current->next;
            if (current == NULL) break;
        }

        if (previous == NULL)
        {
            // insert at start of list
            ap->next = *access_points_list;
            *access_points_list = ap;
            return ap;
        }
        else
        {
            // insert into list
            previous->next = ap;
            ap->next = current;
        }
        return ap;
    }
}


/*
    Convert linked list to array with value
*/
int get_augmented_access_points(struct AccessPointWithValue a[], int array_size, struct AccessPoint* head)
{
    int count = 0;
    for (struct AccessPoint* current = head; current != NULL; current = current->next)
    {
        a[count].access_point = current;
        a[count].value = 0.0;
        count++;
        if (count > array_size){
            g_error("Array not large enough to hold all access points");
        }
    }
    return count;
}


/*
    Convert an id into an index
*/
int get_index(struct AccessPoint* head, int id)
{
    int count = 0;
    for (struct AccessPoint* current = head; current != NULL; current = current->next)
    {
        if (current->id == id) return count;
        count++;
    }
    return -1;
}