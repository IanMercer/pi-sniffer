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
#include <math.h>

/*
* Used to create the local access point only
*/
struct AccessPoint *create_local_access_point(struct OverallState* state, char *client_id,
                                     const char *description, const char *platform,
                                     int rssi_one_meter, float rssi_factor, float people_distance)
{
    //g_debug("Check for new access point '%s'\n", client_id);
    bool created;
    struct AccessPoint* ap = get_or_create_access_point(state, client_id, &created);
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
    ap->ap_class = ap_class_smart_node;
    ap->rssi_one_meter = rssi_one_meter;
    ap->rssi_factor = rssi_factor;
    ap->people_distance = people_distance;
    ap->sequence = 0;
    ap->sensors = NULL;
    time(&ap->last_seen);

    return ap;
}

void print_access_points(struct AccessPoint* access_points_list)
{
    time_t now;
    time(&now);

    char title[80];
    g_snprintf(title, sizeof(title), "%6.6s %6.6s %5.5s %7.7s %4.4s %4.4s %4.4s",
            CJ_INTERNAL_TEMPERATURE, CJ_TEMPERATURE, CJ_HUMIDITY, CJ_PRESSURE, CJ_CARBON_DIOXIDE, CJ_VOC, CJ_WIFI);
 
    g_info("ACCESS POINTS                 Platform Parameter     %s Last Seen", title);
    for (struct AccessPoint* ap = access_points_list; ap != NULL; ap = ap->next)
    {
        char name[25];
        if (g_ascii_strcasecmp(ap->client_id, ap->short_client_id) == 0)
            g_snprintf(name, sizeof(name), "%s", ap->client_id);
        else
            g_snprintf(name, sizeof(name), "%s (%s)", ap->client_id, ap->short_client_id);

        int delta_time = difftime(now, ap->last_seen);

        char buffer[80];
        buffer[0]='\0';
        get_sensor_string(ap, buffer, sizeof(buffer), 7, CJ_INTERNAL_TEMPERATURE, CJ_TEMPERATURE, CJ_HUMIDITY,
            CJ_PRESSURE, CJ_CARBON_DIOXIDE, CJ_VOC, CJ_WIFI);

        g_info("%25.25s %2.2s %12.12s (%3i, %.1f) %s %is",
        name,
        ap->ap_class == ap_class_gateway_node ? "GW" : 
            ap->ap_class == ap_class_smart_node ? "SM" : 
            ap->ap_class == ap_class_dumb_node ? "DU" 
            : "--",
        ap->platform,
        ap->rssi_one_meter, ap->rssi_factor,
        buffer,
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
    {
        for (int j = 0; j < N_ACCESS_POINTS; j++)
        {
            min_dist[i][j] = 10 * EFFECTIVE_INFINITE;
        }
    }

    bool multiple_access_points = state->access_points != NULL && state->access_points->next != NULL;
    if (!multiple_access_points) return;  // Not useful on single sensor sites

    // Triangular matrix looking only at phones (similar transmit power)
    for (struct ClosestHead* ahead = state->closestHead; ahead != NULL; ahead = ahead->next)
    {
        if (ahead->category != CATEGORY_PHONE && ahead->category != CATEGORY_COVID) continue;

        for (struct ClosestTo* a = ahead->closest; a != NULL; a = a->next)
        for (struct ClosestTo* b = ahead->closest; b != NULL; b = b->next)
        {
            if (a->access_point->id == b->access_point->id) continue;  // same ap

            int delta = abs((int)difftime(a->latest, b->latest));
            if (delta > 6) continue;                       // must have occurred close in time (despite clock skew)

            int aid = a->access_point->id;
            int bid = b->access_point->id;

            if (aid < N_ACCESS_POINTS && bid < N_ACCESS_POINTS)
            {
                double d = a->distance + b->distance; // same ping ... + 1.4 * delta;   // average walking speed is 1.4 m/s
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
    append_text(header, sizeof(header), "        |");

    char header2[120];
    header2[0] = '\0';
    append_text(header2, sizeof(header2), "        .");

    for (struct AccessPoint* ap = state->access_points; ap != NULL; ap = ap->next)
    {
        append_text(header, sizeof(header), "%5.5s|", ap->short_client_id);
        append_text(header2, sizeof(header2), "------");
    }

    g_debug("%s", header2);
    g_debug("%s", header);
    g_debug("%s", header2);

    for (struct AccessPoint* ap = state->access_points; ap != NULL; ap = ap->next)
    {
        char line[120];
        line[0] = '\0';

        append_text(line, sizeof(line), "%7.7s|", ap->short_client_id);

        for (struct AccessPoint* bp = state->access_points; bp != NULL; bp = bp->next)
        {
            double d = min_dist[ap->id][bp->id];
            if (d < 9 * EFFECTIVE_INFINITE)
                append_text(line, sizeof(line), "  %4.1f", d);
            else
                append_text(line, sizeof(line), "     -");
        }
        g_debug("%s", line);
    }
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
struct AccessPoint* get_or_create_access_point(struct OverallState* state, 
    const char* client_id, 
    bool* created)
{
    *created = FALSE;
    for (struct AccessPoint* current = state->access_points; current != NULL; current = current->next)
    {
        if ((strcmp(current->client_id, client_id) == 0) ||
            (strcmp(current->short_client_id, client_id) == 0))
            {
                return current;
            }
    }

    *created = TRUE;

    // Otherwise add a new one
    struct AccessPoint* ap = g_malloc(sizeof(struct AccessPoint));
    ap->client_id = strdup(client_id);
    ap->alternate_name = "";
    ap->ap_class = ap_class_unknown;   // until told otherwise
    ap->sensors = NULL;

    strncpy(ap->description, "Not known yet", META_LENGTH);
    strncpy(ap->platform, "Not known yet", META_LENGTH);

    if (string_starts_with(ap->client_id, "crowd-")) ap->short_client_id = &ap->client_id[6];
    else if (string_starts_with(ap->client_id, "m-")) ap->short_client_id = &ap->client_id[2];
    else ap->short_client_id = ap->client_id;

    // Lookup aliases

    for (struct AccessMapping* am = state->access_mappings; am != NULL; am = am->next)
    {
        if (g_ascii_strcasecmp(ap->client_id, am->name) == 0)
        {
            ap->short_client_id = strdup(am->alias);
            ap->alternate_name = strdup(am->alternate);
            break;
        }
    }

    ap->rssi_factor = 0.0;
    ap->rssi_one_meter = 0.0;
    ap->sequence = 0;

    ap->id = access_point_id_generator++;

    // scan to find position
    struct AccessPoint* previous = NULL;
    struct AccessPoint* current = state->access_points;

    while (current != NULL && strcmp(current->client_id, client_id) < 0)
    {
        previous = current;
        current = current->next;
    }

    if (previous == NULL)
    {
        // insert at start of list
        ap->next = state->access_points;
        state->access_points = ap;
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

/**
 * Get or add a sensor by name belonging to a node
 */
struct Sensor* get_or_add_sensor(struct AccessPoint* ap, const char* id)
{
    // char* client_id = ap->client_id; // Will be MAC address for ESP32s
    for (struct Sensor* sensor = ap->sensors; sensor != NULL; sensor=sensor->next)
    {
        if (g_ascii_strcasecmp(sensor->id, id) == 0)
        {
            time(&sensor->latest);
            return sensor;
        }
    }
    struct Sensor* sensor = malloc(sizeof(struct Sensor));
    sensor->next = NULL;
    sensor->id = id;
    sensor->value_float = NAN;
    sensor->value_int = 0;
    time(&sensor->latest);

    // add to front of chain
    sensor->next = ap->sensors;
    ap->sensors = sensor;

    return sensor;
}

/**
 * add or update a sensor float
 */
void add_or_update_sensor_float (struct AccessPoint* ap, const char* id, float value)
{
    if (isnan(value)) return;
    if (value == 0.0) return; // for now, disallow 0.0
    struct Sensor* sensor = get_or_add_sensor(ap, id);
    sensor->value_float = value;
}

/**
 * add or update a sensor int
 */
void add_or_update_sensor_int (struct AccessPoint* ap, const char* id, int value)
{
    if (value == 0) return; // for now, disallow 0
    struct Sensor* sensor = get_or_add_sensor(ap, id);
    sensor->value_int = value;
}

void add_or_update_internal_temp (struct AccessPoint* ap, float value)
{
    add_or_update_sensor_float(ap, CJ_INTERNAL_TEMPERATURE, value);
}

void add_or_update_temperature (struct AccessPoint* ap, float value)
{
    add_or_update_sensor_float(ap, CJ_TEMPERATURE, value);
}

void add_or_update_humidity (struct AccessPoint* ap, float value)
{
    add_or_update_sensor_float(ap, CJ_HUMIDITY, value);
}

void add_or_update_pressure (struct AccessPoint* ap, float value)
{
    add_or_update_sensor_float(ap, CJ_PRESSURE, value);
}

void add_or_update_co2 (struct AccessPoint* ap, float value)
{
    add_or_update_sensor_int(ap, CJ_CARBON_DIOXIDE, value);
}

void add_or_update_brightness (struct AccessPoint* ap, float value)
{
    add_or_update_sensor_float(ap, CJ_BRIGHTNESS, value);
}

void add_or_update_voc (struct AccessPoint* ap, float value)
{
    add_or_update_sensor_int(ap, CJ_VOC, value);
}

void add_or_update_wifi (struct AccessPoint* ap, int value)
{
    add_or_update_sensor_int(ap, CJ_BRIGHTNESS, value);
}

void add_or_update_disk_space (struct AccessPoint* ap, int value)
{
    add_or_update_sensor_int(ap, CJ_FREE_MEGABYTES, value);
}

void get_sensor_string(struct AccessPoint* ap, char* buffer, int buffer_len, int num_args, ...)
{
   va_list valist;

   /* initialize valist for num number of arguments */
   va_start(valist, num_args);

   /* access all the arguments assigned to valist */
   for (int i = 0; i < num_args; i++) {
        const char* id = va_arg(valist, const char*);
        struct Sensor* found = NULL;
        for (struct Sensor* sensor = ap->sensors; sensor != NULL; sensor=sensor->next)
        {
            if (g_ascii_strcasecmp(id, sensor->id) == 0)
            {
                found = sensor;
                break;
            }
        }
        if (found == NULL)
        {
            append_text(buffer, buffer_len, "%s", "     ");
        } else
        {
            // TODO: Units of measure: %4.1f°C %4.1f°C %4.1f%% %4.1f KPa %4i %4.1f %i
            if (isnan(found->value_float))
                append_text(buffer, buffer_len, " %4i", found->value_int);
            else
                append_text(buffer, buffer_len, " %4.1f", found->value_float);
        }

        if ((strcmp(id, CJ_TEMPERATURE) == 0) || strcmp(id, CJ_INTERNAL_TEMPERATURE) == 0)
        {
            if (found == NULL) append_text(buffer, buffer_len, "%s", "  ");
            else append_text(buffer, buffer_len, "%s", "°C");
        }
        else if ((strcmp(id, CJ_HUMIDITY) == 0))
        {
            if (found == NULL) append_text(buffer, buffer_len, "%s", " ");
            else append_text(buffer, buffer_len, "%c", '%');
        }
        else if ((strcmp(id, CJ_PRESSURE) == 0))
        {
            if (found == NULL) append_text(buffer, buffer_len, "%s", "   ");
            else append_text(buffer, buffer_len, "%s", "KPa");
        }
   }
	
   /* clean memory reserved for valist */
   va_end(valist);
}
