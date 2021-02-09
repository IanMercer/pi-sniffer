/*
    Access Points
*/

#include "accesspoints.h"
#include "state.h"
#include "utility.h"
#include <glib.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/types.h>
#include <time.h>

struct AccessPoint *add_access_point(struct AccessPoint** access_point_list, char *client_id,
                                     const char *description, const char *platform,
                                     int rssi_one_meter, float rssi_factor, float people_distance)
{
    //g_debug("Check for new access point '%s'\n", client_id);
    bool created;
    struct AccessPoint* ap = get_or_create_access_point(access_point_list, client_id, &created);
    g_assert(ap != NULL);
    if (ap == NULL) return NULL;   // full

    g_assert(strcmp(ap->client_id, client_id) == 0);

    // have to assign it always if changed
    if (strlen(description)>0 && strcmp(ap->description, description) != 0)
    {
        g_utf8_strncpy(ap->description, description, META_LENGTH);
        g_utf8_strncpy(ap->platform, platform, META_LENGTH);
        //strncpy(ap->client_id, client_id, META_LENGTH);
    }
    ap->rssi_one_meter = rssi_one_meter;
    ap->rssi_factor = rssi_factor;
    ap->people_distance = people_distance;
    ap->sequence = 0;
    time(&ap->last_seen);

    return ap;
}

void print_access_points(struct AccessPoint* access_points_list)
{
    time_t now;
    time(&now);
    g_info("ACCESS POINTS          Platform       Parameters   Last Seen");
    for (struct AccessPoint* ap = access_points_list; ap != NULL; ap = ap->next)
    {
        int delta_time = difftime(now, ap->last_seen);
        g_info("%20s %16s (%3i, %.1f) %is",
        ap->client_id, ap->platform,
        ap->rssi_one_meter, ap->rssi_factor,
        delta_time);
    }
}

/*
*  Explore mesh network by calculating minimum distances between each pair of APs
*/
void print_min_distance_matrix(struct OverallState* state)
{
    double min_dist[N_ACCESS_POINTS][N_ACCESS_POINTS];

    for (int i = 0; i < N_ACCESS_POINTS; i++)
    for (int j = 0; j < N_ACCESS_POINTS; j++)
    {
        min_dist[i][j] = 10 * EFFECTIVE_INFINITE;
    }

    // Triangular matrix looking only at phones (similar transmit power)
    for (int i = state->closest_n - 1; i > 0; i--)
    {
        struct ClosestTo a = state->closest[i];
        if (a.category != CATEGORY_PHONE && a.category != CATEGORY_COVID) continue;
        for (int j = i - 1; j >= 0; j--)
        {
            struct ClosestTo b = state->closest[j];
            if (b.category != CATEGORY_PHONE && b.category != CATEGORY_COVID) continue;
            if (a.device_64 != b.device_64) continue;       // same device only

            int delta = abs(difftime(a.latest, b.latest));
            if (delta > 6) continue;                       // must have occurred close in time (despite clock skew)

            int aid = a.access_point->id;
            int bid = b.access_point->id;

            if (aid < N_ACCESS_POINTS && bid < N_ACCESS_POINTS)
            {
                double d = a.distance + b.distance; // same ping ... + 1.4 * delta;   // average walking speed is 1.4 m/s
                if (min_dist[aid][bid] > EFFECTIVE_INFINITE)
                {
                    min_dist[bid][aid] = min_dist[aid][bid] = d;
                }
                else if (d < min_dist[aid][bid])
                {
                    // smoothing function faster on way down
                    min_dist[bid][aid] = min_dist[aid][bid] = d * 0.5 + min_dist[aid][bid] * 0.5;
                }
                else
                {
                    // smoothing function up way up
                    min_dist[bid][aid] = min_dist[aid][bid] = d * 0.1 + min_dist[aid][bid] * 0.9;
                }
            }
        }
    }

    // HEADER
    char header[120];
    header[0] = '\0';
    append_text(header, sizeof(header), "         |");

    char header2[120];
    header2[0] = '\0';
    append_text(header2, sizeof(header2), "         .");

    for (struct AccessPoint* ap = state->access_points; ap != NULL; ap = ap->next)
    {
        append_text(header, sizeof(header), "%7.7s|", ap->short_client_id);
        append_text(header2, sizeof(header2), "---------");
    }

    g_debug("%s", header2);
    g_debug("%s", header);
    g_debug("%s", header2);

    for (struct AccessPoint* ap = state->access_points; ap != NULL; ap = ap->next)
    {
        char line[120];
        line[0] = '\0';

        append_text(line, sizeof(line), "%8.8s|", ap->short_client_id);

        for (struct AccessPoint* bp = state->access_points; bp != NULL; bp = bp->next)
        {
            double d = min_dist[ap->id][bp->id];
            if (d < 9 * EFFECTIVE_INFINITE)
                append_text(line, sizeof(line), "    %4.1f", d);
            else
                append_text(line, sizeof(line), "       -");
        }
        g_debug("%s", line);
    }
}


/*
    Identity map access point and update values
*/
struct AccessPoint *update_accessPoints(struct AccessPoint** access_point_list, struct AccessPoint access_point)
{
    struct AccessPoint* ap = add_access_point(access_point_list, access_point.client_id,
                            access_point.description, access_point.platform,
                            access_point.rssi_one_meter, access_point.rssi_factor, access_point.people_distance);
    strncpy(ap->description, access_point.description, META_LENGTH);
    strncpy(ap->platform, access_point.platform, META_LENGTH);
    // TODO: Only if later
    time(&access_point.last_seen);

    // Make sure we aren't dropping too many messages
    if (ap->sequence !=0 && 
        (access_point.sequence - ap->sequence) > 1 &&
        (access_point.sequence - ap->sequence) < 1E6)
    {
        g_warning("Missed %li messages from %s", (long)((access_point.sequence - ap->sequence) - 1), ap->client_id);
    }
    ap->sequence = access_point.sequence;

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
    Get or add an access point (THIS IS THE ONLY PLACE WE ADD AN AP)
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
    strncpy(ap->platform, "Not known yet", META_LENGTH);

    if (string_starts_with(ap->client_id, "crowd-")) ap->short_client_id = &ap->client_id[6];
    else if (string_starts_with(ap->client_id, "m-")) ap->short_client_id = &ap->client_id[2];
    else ap->short_client_id = ap->client_id;

    ap->rssi_factor = 0.0;
    ap->rssi_one_meter = 0.0;
    ap->sequence = 0;

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