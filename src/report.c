// OBSOLETE - reporting will all come off DBUS

/*
 *  CROWD ALERT - REPORT SERVICE
 *
 *  Receives bluetooth information from multiple SCAN components distributed on multiple raspberry pi
 *  (or, coming soon, other microcontrollers). Computes an aggregate view of what is happening
 *  placing signals received into patches, rooms and groups. Communicates the aggregated view out
 *  over UDP (to networked signs), HTTP POST, Webhook, MQTT, and/or DBUS.
 *   
 *  See Makefile, build.sh and Github
 *
 */
#include "core/utility.h"
#ifdef MQTT
#include "mqtt_send.h"
#endif
#include "udp.h"
#include "device.h"
#include "influx.h"
#include "rooms.h"
#include "accesspoints.h"
#include "closest.h"
#include "sniffer-generated.h"
#include "webhook.h"
#include "state.h"

#define G_LOG_USE_STRUCTURED 1
#include <glib.h>
#include <gio/gio.h>
#include <stdbool.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <net/if.h>
#include <time.h>
#include <math.h>
#include <signal.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/types.h>
#include <string.h>
#include <udp.h>
#include <argp.h>
#include <unistd.h>    // gethostname

// For LED flash
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <cJSON.h>

static struct OverallState state;
// contains ... static struct Device devices[N];

static bool starting = TRUE;

// delta distance x delta time threshold for sending an update
// e.g. 5m after 2s or 1m after 10s
// prevents swamping MQTT with very small changes
// but also guarantees an occasional update to indicate still alive

#define THRESHOLD 10.0

// Handle Ctrl-c
void int_handler(int);

bool logTable = FALSE; // set to true each time something changes

/*
    Connection to DBUS
*/
GDBusConnection *conn;

/*
    Do these two devices overlap in time? If so they cannot be the same device
    (allowed to touch given granularity of time)
*/
bool overlaps(struct Device *a, struct Device *b)
{
    if (a->earliest >= b->latest)
        return FALSE; // a is entirely after b
    if (b->earliest >= a->latest)
        return FALSE; // b is entirely after a
    return TRUE;      // must overlap if not entirely after or before
}

/*
    Compute the minimum number of devices present by assigning each in a non-overlapping manner to columns
*/
void pack_columns()
{
    // Push every device back to column zero as category may have changed
    for (int i = 0; i < state.n; i++)
    {
        struct Device *a = &state.devices[i];
        a->column = 0;
    }

    for (int k = 0; k < state.n; k++)
    {
        bool changed = false;

        for (int i = 0; i < state.n; i++)
        {
            for (int j = i + 1; j < state.n; j++)
            {
                struct Device *a = &state.devices[i];
                struct Device *b = &state.devices[j];

                if (a->column != b->column)
                    continue;

                bool over = overlaps(a, b);

                // cannot be the same device if either has a public address (or we don't have an address type yet)
                bool haveDifferentAddressTypes = (a->addressType > 0 && b->addressType > 0 && a->addressType != b->addressType);

                // cannot be the same if they both have names and the names are different
                // but don't reject _ names as they are temporary and will get replaced
                bool haveDifferentNames = (strlen(a->name) > 0) && (strlen(b->name) > 0) 
                    // can reject as soon as they both have a partial name that is same type but doesn't match
                    && (a->name_type == b->name_type)               
                    && (g_strcmp0(a->name, b->name) != 0);

                // cannot be the same if they both have known categories and they are different
                // Used to try to blend unknowns in with knowns but now we get category right 99.9% of the time, no longer necessary
                bool haveDifferentCategories = (a->category != b->category); // && (a->category != CATEGORY_UNKNOWN) && (b->category != CATEGORY_UNKNOWN);

                bool haveDifferentMacAndPublic = (a->addressType == PUBLIC_ADDRESS_TYPE && strcmp(a->mac, b->mac)!=0);

                if (over || haveDifferentAddressTypes || haveDifferentNames || haveDifferentCategories || haveDifferentMacAndPublic)
                {
                    b->column++;
                    changed = true;
                    // g_print("Compare %i to %i and bump %4i %s to %i, %i %i %i\n", i, j, b->id, b->mac, b->column, over, haveDifferentAddressTypes, haveDifferentNames);
                }
            }
        }
        if (!changed)
            break;
    }

    for (int i = state.n - 1; i > 0; i--)
    {
        struct Device* current = &state.devices[i];

        // Can't mark superseded until we know for sure it's a phone etc.
        if (current->category == CATEGORY_UNKNOWN) continue;

        int64_t mac64 = mac_string_to_int_64(state.devices[i].mac);
        for (int j = i - 1; j >= 0; j--)
        {
            struct Device* earlier = &state.devices[j];

            if (current->column == earlier->column)
            {
                bool send_update = (earlier->supersededby == 0);
                earlier->supersededby = mac64;
                if (send_update)
                {
                    g_info("%s has been superseded by %s", earlier->mac, current->mac);
                    send_device_udp(&state, earlier);
                    update_superseded(&state, earlier);
                }
            }
            // If earlier was supersededby this one but now it isn't we need to report that
            else if (earlier->supersededby == mac64)
            {
                // Not in same column but has a supersededby value
                // This device used to be superseded by the new one, but now we know it isn't
                earlier->supersededby = 0;
                g_info("%s IS NO LONGER superseded by %s", earlier->mac, current->mac);
                send_device_udp(&state, earlier);
                update_superseded(&state, earlier);
            }
        }
    }
}


/*
  Use a known beacon name for a device if present
*/
void apply_known_beacons(struct Device* device)
{
    for (struct Beacon* b = state.beacons; b != NULL; b = b->next)
    {
        if (strcmp(b->name, device->name) == 0 || b->mac64 == device->mac64)
        {
            set_name(device, b->alias, nt_alias);
        }
    }
}

#define MAX_TIME_AGO_COUNTING_MINUTES 5
#define MAX_TIME_AGO_LOGGING_MINUTES 10
#define MAX_TIME_AGO_CACHE 60

// Updated before any function that needs to calculate relative time
time_t now;

// Largest number of devices that can be tracked at once
#define N_COLUMNS 500

struct ColumnInfo
{
    time_t latest;   // latest observation in this column
    float distance;  // distance of latest observation in this column
    int8_t category; // category of device in this column (phone, computer, ...)
    bool isClosest;  // is this device closest to us not some other sensor
};

struct ColumnInfo columns[N_COLUMNS];

/*
    Find latest observation in each column and the distance for that
*/
void find_latest_observations()
{
    for (uint i = 0; i < N_COLUMNS; i++)
    {
        columns[i].distance = -1.0;
        columns[i].category = CATEGORY_UNKNOWN;
    }
    for (int i = 0; i < state.n; i++)
    {
        struct Device *a = &state.devices[i];
        int col = a->column;
        if (columns[col].distance < 0.0 || columns[col].latest < a->latest)
        {
            // Issue here, a single later 10.0m distance was moving a column
            // beyond the range allowed

            if (a->count > 1 || 
                columns[col].distance < 0.0 || 
                a->distance < columns[col].distance)
            {
                // distance only replaces old distance if >2 count or it's closer
                // otherwise one random far distance can move a device out of range
                columns[col].distance = a->distance;
            }
            if (a->category != CATEGORY_UNKNOWN)
            {
                // a later unknown does not override an actual phone category nor extend it
                // This if is probably not necessary now as overlap tests for this
                columns[col].category = a->category;
                columns[col].latest = a->latest;
                // Do we 'own' this device or does someone else
                columns[col].isClosest = true;
                struct ClosestTo *closest = get_closest_64(&state, a->mac64);
                columns[col].isClosest = closest != NULL && closest->access_point != NULL && closest->access_point->id == state.local->id;
            }
        }
    }
}

#define N_RANGES 10
static int32_t ranges[N_RANGES] = {1, 2, 5, 10, 15, 20, 25, 30, 35, 100};
static int8_t reported_ranges[N_RANGES] = {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1};


/*
    Find best packing of device time ranges into columns
    //Set every device to column zero
    While there is any overlap, find the overlapping pair, move the second one to the next column
*/

void report_devices_count()
{
    //g_debug("report_devices_count\n");

    if (starting)
        return; // not during first 30s startup time

    // Initialize time and columns
    time(&now);

    // Allocate devices to coluumns so that there is no overlap in time
    pack_columns();

    // Find the latest device record in each column
    find_latest_observations();

    // Calculate for each range how many devices are inside that range at the moment
    // Ignoring any that are potential MAC address randomizations of others
    int previous = 0;
    bool made_changes = FALSE;
    for (int i = 0; i < N_RANGES; i++)
    {
        int range = ranges[i];

        int min = 0;

        for (int col = 0; col < N_COLUMNS; col++)
        {
            if (columns[col].category != CATEGORY_PHONE)
                continue; // only counting phones now not beacons, laptops, ...
            if (columns[col].distance < 0.01)
                continue; // not allocated

            int delta_time = difftime(now, columns[col].latest);
            if (delta_time > MAX_TIME_AGO_COUNTING_MINUTES * 60)
                continue;

            if (columns[col].distance < range)
                min++;
        }

        int just_this_range = min - previous;
        if (reported_ranges[i] != just_this_range)
        {
            //g_print("Devices present at range %im %i    \n", range, just_this_range);
            reported_ranges[i] = just_this_range;
            made_changes = TRUE;
        }
        previous = min;
    }

    if (made_changes)
    {
        char line[128];
        snprintf(line, sizeof(line), "Devices by range:");
        for (int i = 0; i < N_RANGES; i++)
        {
            snprintf(line + strlen(line), sizeof(line) - strlen(line), " %i", reported_ranges[i]);
        }
        g_info("%s", line);
#ifdef MQTT
        send_to_mqtt_distances((unsigned char *)reported_ranges, N_RANGES * sizeof(int8_t));
#endif
    }

    int range_limit = 5;
    int range = ranges[range_limit];

    // Expected value of number of people here (decays if not detected recently)
    float people_in_range = 0.0;
    float people_closest = 0.0;

    for (int col = 0; col < N_COLUMNS; col++)
    {
        if (columns[col].distance >= range)
            continue;
        if (columns[col].category != CATEGORY_PHONE)
            continue; // only counting phones now not beacons, laptops, ...
        if (columns[col].distance < 0.01)
            continue; // not allocated

        int delta_time = difftime(now, columns[col].latest);
        if (delta_time > MAX_TIME_AGO_COUNTING_MINUTES * 60)
            continue;

        // double score = 0.55 - atan(delta_time/40.0  - 4.0) / 3.0; -- left some spikes in the graph, dropped too quickly
        // double score = 0.55 - atan(delta_time / 45.0 - 4.0) / 3.0; -- maybe too much?
        double score = 0.55 - atan(delta_time / 42.0 - 4.0) / 3.0;
        // A curve that stays a 1.0 for a while and then drops rapidly around 3 minutes out
        if (score > 0.99) score = 1.0;
        if (score < 0.0) score = 0.0;

        // Expected value E[x] = i x p(i) so sum p(i) for each column which is one person
        people_in_range += score;
        if (columns[col].isClosest)
            people_closest += score;
    }

    if (fabs(people_in_range - state.local->people_in_range_count) > 0.1 || fabs(people_closest - state.local->people_closest_count) > 0.1)
    {
        state.local->people_closest_count = people_closest;
        state.local->people_in_range_count = people_in_range;
        g_debug("Local people count = %.2f (%.2f in range)", people_closest, people_in_range);

        // And send access point to everyone over UDP - now sent only on tick event
        //send_access_point_udp(&state);
    }

// TEST TEST TEST TEST

    GVariantBuilder *b = g_variant_builder_new(G_VARIANT_TYPE_VARDICT);
    g_variant_builder_add(b, "{sv}", "PeopleClosest", g_variant_new_double(people_closest));
    g_variant_builder_add(b, "{sv}", "PeopleInRange", g_variant_new_double(people_in_range));
    GVariant *dict = g_variant_builder_end(b);
    g_variant_builder_unref(b);

    GVariant* parameters = g_variant_new_tuple(&dict, 1);
    //rc = bluez_adapter_call_method(conn, "SetDiscoveryFilter", g_variant_new_tuple(&device_dict, 1), NULL);
    // no need to ... g_variant_unref(dict);

    GError *error = NULL;
    gboolean ret = g_dbus_connection_emit_signal(conn, 
        NULL,                           // 
        "/com/signswift/sniffer",       // path
        "com.signswift.sniffer",        // interface name
        "People",                       // signal_name
        parameters, &error);

    if (ret)
    {
        print_and_free_error(error);
    }
}

/*
   Read byte array from GVariant
*/
unsigned char *read_byte_array(GVariant *s_value, int *actualLength, uint8_t *hash)
{
    unsigned char byteArray[2048];
    int len = 0;

    GVariantIter *iter_array;
    guchar str;

    //g_debug("START read_byte_array\n");

    g_variant_get(s_value, "ay", &iter_array);

    while (g_variant_iter_loop(iter_array, "y", &str))
    {
        byteArray[len++] = str;
    }

    g_variant_iter_free(iter_array);

    unsigned char *allocdata = g_malloc(len);
    memcpy(allocdata, byteArray, len);

    *hash = 0;
    for (int i = 0; i < len; i++)
    {
        *hash += allocdata[i];
    }

    *actualLength = len;

    //g_debug("END read_byte_array\n");
    return allocdata;
}


#define BT_ADDRESS_STRING_SIZE 18


// Every 10s we need to let MQTT send and receive messages
int mqtt_refresh(void *parameters)
{
    (void)parameters;
    //GMainLoop *loop = (GMainLoop *)parameters;
    // Send any MQTT messages
#ifdef MQTT
    mqtt_sync();
#endif
    return TRUE;
}

// Time when service started running (used to print a delta time)
static time_t started;

bool webhook_is_configured()
{
    if (state.webhook_domain == NULL) return FALSE;
    if (state.webhook_path == NULL) return FALSE;
    if (strlen(state.webhook_domain)==0) return FALSE;  
    return TRUE;
}

bool influx_is_configured()
{
    if (state.influx_server == NULL) return FALSE;
    if (strlen(state.influx_server)==0) return FALSE;
    return TRUE;
}

/*
    Report summaries to HttpPost
*/
int report_to_http_post_tick()
{
    if (!webhook_is_configured()) return FALSE;
    post_to_webhook(&state);
    return TRUE;
}


/*
    Report summaries to InfluxDB
*/
int report_to_influx_tick(struct OverallState* state)
{
    if (!influx_is_configured()) return FALSE;

    char body[4096];
    body[0] = '\0';

    bool ok = TRUE;

    time_t now = time(0);

    struct summary* summary = NULL;
    summarize_by_room(state->patches, &summary);

    // Clean out a stuck signal on InfluxDB
    //ok = ok && append_influx_line(body, sizeof(body), "<Group>", "room=<room>", "beacon=0.0,computer=0.0,phone=0.0,tablet=0.0,watch=0.0,wear=0.0", now);

    for (struct summary* s = summary; s != NULL; s = s->next)
    {
        char tags[120];
        char field[120];

        snprintf(field, sizeof(field), "beacon=%.1f,computer=%.1f,phone=%.1f,tablet=%.1f,watch=%.1f,wear=%.1f,oth=%.1f",
            s->beacon_total, s->computer_total, s->phone_total, s->tablet_total, s->watch_total, s->wearable_total, s->other_total);

        snprintf(tags, sizeof(tags), "room=%s", s->category);

        ok = ok && append_influx_line(body, sizeof(body), s->extra, tags, field, now);

        if (strlen(body) + 5 * 100 > sizeof(body)){
            //g_debug("%s", body);
            post_to_influx(state, body, strlen(body));
            body[0] = '\0';
        }

        //g_debug("INFLUX: %s %s %s", s->extra, tags, field);
    }

    free_summary(&summary);

    if (strlen(body) > 0)
    {
        //g_debug("%s", body);
        post_to_influx(state, body, strlen(body));
    }

    if (!ok)
    {
        g_warning("Influx messages was truncated");
    }
    return TRUE;
}

/*
    COMMUNICATION WITH DISPLAYS OVER UDP
    NB this needs to a smaller message to go over UDP
*/
void send_to_udp_display(struct OverallState *state)
{
    if (state->network_up && state->udp_sign_port > 0)
    {
        cJSON *jobject = cJSON_CreateObject();

        struct summary* summary = NULL;

        summarize_by_group(state->patches, &summary);

        for (struct summary* s=summary; s!=NULL; s=s->next)
        {
            cJSON_AddRounded(jobject, s->category, s->phone_total);
        }
        free_summary(&summary);

        // Add metadata for the sign to consume (so that signage can be adjusted remotely)
        // TODO: More levels etc. settable remotely
        cJSON_AddRounded(jobject, "sf", state->udp_scale_factor);

        char* json = cJSON_PrintUnformatted(jobject);
        cJSON_Delete(jobject);

        //g_warning("%s", json);

        // +1 for the NULL terminator
        udp_send(state->udp_sign_port, json, strlen(json)+1);        
    }
}


static int report_count = 0;

/*
    Report access point counts to InfluxDB, Web, UDP
    Called every 20s but Web hook only called once a minute and Influx once every five minutes
*/
int report_counts(void *parameters)
{
    (void)parameters;

    if (!state.network_up) return TRUE;
    if (starting) return TRUE;

    report_count++;

    if (influx_is_configured() || webhook_is_configured() || state.web_polling || state.udp_sign_port > 0)
    {
        time(&now);
        // Set JSON for all ways to receive it (GET, POST, INFLUX, MQTT)
        bool changed = print_counts_by_closest(&state);

        int influx_seconds = difftime(now, state.influx_last_sent);

        if (influx_seconds > state.influx_max_period_seconds || (changed && (influx_seconds > state.influx_min_period_seconds)))
        {
            g_debug("Sending to influx %is since last", influx_seconds);
            state.influx_last_sent = now;
            report_to_influx_tick(&state);
        }

        int webhook_seconds = difftime(now, state.webhook_last_sent);

        if (webhook_seconds > state.webhook_max_period_seconds || (changed && (webhook_seconds > state.webhook_min_period_seconds)))
        {
            g_debug("Sending to webhook %is since last", webhook_seconds);
            state.webhook_last_sent = now;
            report_to_http_post_tick();
        }

        // Every 20s
        send_to_udp_display(&state);
        return TRUE;
    }
    else
    {
        return FALSE;  // remove from loop
    }
}

/*
    Print access point metadata
*/
int print_access_points_tick(void *parameters)
{
    (void)parameters;
    print_access_points(state.access_points);

    // And send access point to everyone over UDP - so that even if no activity everyone gets a list of active access points
    send_access_point_udp(&state);

    return TRUE;
}

/*
    Dump all devices present
*/
int dump_all_devices_tick(void *parameters)
{
    starting = FALSE;
    state.network_up = is_any_interface_up();

    (void)parameters; // not used
    if (starting)
        return TRUE; // not during first 30s startup time
    if (!logTable)
        return TRUE; // no changes since last time
    logTable = FALSE;

    unsigned long total_minutes = (now - started) / 60; // minutes
    unsigned int minutes = total_minutes % 60;
    unsigned int hours = (total_minutes / 60) % 24;
    unsigned int days = (total_minutes) / 60 / 24;

    float people_closest = state.local->people_closest_count;
    float people_in_range = state.local->people_in_range_count;

    const char* connected = is_any_interface_up() ? "" : "NETWORK DOWN ";
#ifdef MQTT    
    const char* m_state = mqtt_state();
#else
    const char* m_state = "";
#endif

    if (days > 1)
        g_info("People %.2f (%.2f in range) Uptime: %i days %02i:%02i %s%s", people_closest, people_in_range, days, hours, minutes, connected, m_state);
    else if (days == 1)
        g_info("People %.2f (%.2f in range) Uptime: 1 day %02i:%02i %s%s", people_closest, people_in_range, hours, minutes, connected, m_state);
    else
        g_info("People %.2f (%.2f in range) Uptime: %02i:%02i %s%s", people_closest, people_in_range, hours, minutes, connected, m_state);

    // Bluez eventually seems to stop sending us data, so for now, just restart every few hours
    if (hours > 2)
    {
        g_warning("*** RESTARTING AFTER 2 HOURS RUNNING");
        int_handler(0);
    }

    return TRUE;
}


static int led_state = 0;
// TODO: Suppress this on non-Pi platforms

int flash_led(void *parameters)
{
    (void)parameters;                // not used
    // Every other cycle turn the LED on or off
    if (led_state < state.local->people_in_range_count * 2) {
        char d =  (led_state & 1) == 0 ? '1' : '0';
        int fd = open("/sys/class/leds/led0/brightness", O_WRONLY);
        write (fd, &d, 1);
        close(fd);
    }

    led_state = (led_state + 1);
    // 4s past end, restart
    if (led_state > state.local->people_in_range_count * 2 + 4){
        led_state = 0;
    }

    return TRUE;
}


static char client_id[META_LENGTH];


/*
    Initialize state from environment
*/
void initialize_state()
{
    // Default values if not set in environment variables
    const char *description = "Please set a HOST_DESCRIPTION in the environment variables";
    const char *platform = "Please set a HOST_PLATFORM in the environment variables";
    int rssi_one_meter = -64;    // fairly typical RPI3 and iPhone
    float rssi_factor = 3.5;     // fairly cluttered indoor default
    float people_distance = 7.0; // 7m default range
    state.udp_mesh_port = 7779;
    state.udp_sign_port = 0; // 7778;
    state.access_points = NULL; // linked list
    state.patches = NULL;         // linked list
    state.groups = NULL;        // linked list
    state.closest_n = 0;        // count of closest
    state.patch_hash = 0;       // hash to detect changes
    state.beacons = NULL;       // linked list
    state.json = NULL;          // DBUS JSON message
    time(&state.influx_last_sent);
    time(&state.webhook_last_sent);

    // no devices yet
    state.n = 0;

    if (gethostname(client_id, META_LENGTH) != 0)
    {
        g_error("Could not get host name");
        exit(EXIT_FAILURE);
    }
    g_debug("Host name: %s", client_id);

    state.network_up = FALSE; // is_any_interface_up();
    state.web_polling = FALSE; // until first request

    // Optional metadata about the access point for dashboard
    const char *s_client_id = getenv("HOST_NAME");
    const char *s_client_description = getenv("HOST_DESCRIPTION");
    const char *s_client_platform = getenv("HOST_PLATFORM");

    if (s_client_id != NULL)
    {
        strncpy(client_id, s_client_id, META_LENGTH);
        g_debug("Client id: %s", client_id);
    }

    // These two can just be pointers to the constant strings or the supplied metadata
    if (s_client_description != NULL)
        description = s_client_description;
    if (s_client_platform != NULL)
        platform = s_client_platform;

    g_debug("Get RSSI factors");
    const char *s_rssi_one_meter = getenv("RSSI_ONE_METER");
    const char *s_rssi_factor = getenv("RSSI_FACTOR");
    const char *s_people_distance = getenv("PEOPLE_DISTANCE");

    if (s_rssi_one_meter != NULL)
        rssi_one_meter = atoi(s_rssi_one_meter);
    if (s_rssi_factor != NULL)
        rssi_factor = strtof(s_rssi_factor, NULL);
    if (s_people_distance != NULL)
        people_distance = strtof(s_people_distance, NULL);

    g_debug("Add self as access point");

    if (rssi_one_meter > -50) g_warning("Unlikely setting for RSSI_ONE_METER=%i", rssi_one_meter);
    if (rssi_one_meter < -150) g_warning("Unlikely setting for RSSI_ONE_METER=%i", rssi_one_meter);
    if (rssi_factor < 1.0) g_warning("Unlikely setting for RSSI_FACTOR=%.2f", rssi_factor);
    if (rssi_factor > 5.0) g_warning("Unlikely setting for RSSI_FACTOR=%.2f", rssi_factor);

    state.local = add_access_point(&state.access_points, client_id, description, platform,
                                   rssi_one_meter, rssi_factor, people_distance);

    // UDP Settings

    get_int_env("UDP_MESH_PORT", &state.udp_mesh_port, 0);
    get_int_env("UDP_SIGN_PORT", &state.udp_sign_port, 0);
    // Metadata passed to the display to adjust how it displays the values sent
    // TODO: Expand this to an arbitrary JSON blob
    get_float_env("UDP_SCALE_FACTOR", &state.udp_scale_factor, 1.0);

    // MQTT Settings

    get_string_env("MQTT_TOPIC", &state.mqtt_topic, "BLF");  // sorry, historic name
    get_string_env("MQTT_SERVER", &state.mqtt_server, "");
    get_string_env("MQTT_USERNAME", &state.mqtt_username, "");
    get_string_env("MQTT_PASSWORD", &state.mqtt_password, "");

    // INFLUX DB

    get_int_env("INFLUX_MIN_PERIOD", &state.influx_min_period_seconds, 5 *60);      // at most every 5 min
    get_int_env("INFLUX_MAX_PERIOD", &state.influx_max_period_seconds, 60 *60);     // at least every 60 min

    state.influx_server = getenv("INFLUX_SERVER");
    if (state.influx_server == NULL) state.influx_server = "";

    get_int_env("INFLUX_PORT", &state.influx_port, 8086);

    state.influx_database = getenv("INFLUX_DATABASE");
    state.influx_username = getenv("INFLUX_USERNAME");
    state.influx_password = getenv("INFLUX_PASSWORD");

    // WEBHOOK

    get_int_env("WEBHOOK_MIN_PERIOD", &state.webhook_min_period_seconds, 5 *60);      // at most every 5 min
    get_int_env("WEBHOOK_MAX_PERIOD", &state.webhook_max_period_seconds, 60 *60);     // at least every 60 min

    state.webhook_domain = getenv("WEBHOOK_DOMAIN");
    get_int_env("WEBHOOK_PORT", &state.webhook_port, 80);
    state.webhook_path = getenv("WEBHOOK_PATH");
    state.webhook_username = getenv("WEBHOOK_USERNAME");
    state.webhook_password = getenv("WEBHOOK_PASSWORD");

    state.configuration_file_path = getenv("CONFIG");
    if (state.configuration_file_path == NULL) state.configuration_file_path = "/etc/signswift/config.json";

    state.verbosity = Distances; // default verbosity
    char* verbosity = getenv("VERBOSITY");
    if (verbosity){
        if (strcmp(verbosity, "counts")) state.verbosity = Counts;
        if (strcmp(verbosity, "distances")) state.verbosity = Distances;
        if (strcmp(verbosity, "details")) state.verbosity = Details;
    }
   
    read_configuration_file(state.configuration_file_path, &state.access_points, &state.beacons);

    g_debug("Completed read of configuration file");
}

void display_state()
{
    g_info("HOST_NAME = %s", state.local->client_id);
    g_info("HOST_DESCRIPTION = %s", state.local->description);
    g_info("HOST_PLATFORM = %s", state.local->platform);

    g_info("RSSI_ONE_METER Power at 1m : %i", state.local->rssi_one_meter);
    g_info("RSSI_FACTOR to distance : %.1f   (typically 2.0 (indoor, cluttered) to 4.0 (outdoor, no obstacles)", state.local->rssi_factor);
    g_info("PEOPLE_DISTANCE : %.1fm (cutoff)", state.local->people_distance);

    g_info("UDP_MESH_PORT=%i", state.udp_mesh_port);
    g_info("UDP_SIGN_PORT=%i", state.udp_sign_port);
    g_info("UDP_SCALE_FACTOR=%.1f", state.udp_scale_factor);

    g_info("VERBOSITY=%i", state.verbosity);

    g_info("MQTT_TOPIC='%s'", state.mqtt_topic);
    g_info("MQTT_SERVER='%s'", state.mqtt_server);
    g_info("MQTT_USERNAME='%s'", state.mqtt_username);
    g_info("MQTT_PASSWORD='%s'", state.mqtt_password == NULL ? "(null)" : "*****");

    g_info("INFLUX_SERVER='%s'", state.influx_server);
    g_info("INFLUX_PORT=%i", state.influx_port);
    g_info("INFLUX_DATABASE='%s'", state.influx_database);
    g_info("INFLUX_USERNAME='%s'", state.influx_username);
    g_info("INFLUX_PASSWORD='%s'", state.influx_password == NULL ? "(null)" : "*****");
    g_info("INFLUX_MIN_PERIOD='%i'", state.influx_min_period_seconds);
    g_info("INFLUX_MAX_PERIOD='%i'", state.influx_max_period_seconds);

    g_info("WEBHOOK_URL='%s:%i%s'", state.webhook_domain == NULL ? "(null)" : state.webhook_domain, state.webhook_port, state.webhook_path);
    g_info("WEBHOOK_USERNAME='%s'", state.webhook_username == NULL ? "(null)" : "*****");
    g_info("WEBHOOK_PASSWORD='%s'", state.webhook_password == NULL ? "(null)" : "*****");
    g_info("WEBHOOK_MIN_PERIOD='%i'", state.webhook_min_period_seconds);
    g_info("WEBHOOK_MAX_PERIOD='%i'", state.webhook_max_period_seconds);

    g_info("CONFIG='%s'", state.configuration_file_path == NULL ? "** Please set a path to config.json **" : state.configuration_file_path);

    int count = 0;
    for (struct AccessPoint* ap = state.access_points; ap != NULL; ap=ap->next){ count ++; }
    g_info("ACCESS_POINTS: %i", count);
}

guint prop_changed;
guint iface_added;
guint iface_removed;
GMainLoop *loop;

GCancellable *socket_service;

/*
    incoming request on DBUS, probably from web CGI
*/
static gboolean on_handle_status_request (piSniffer *interface,
                       GDBusMethodInvocation  *invocation,
                       gpointer                user_data)
{
    struct OverallState* state = (struct OverallState*)user_data;
    gchar *response;
    state->web_polling = TRUE;
    response = state->json;
    if (response == NULL) response = "NOT READY";
    pi_sniffer_complete_status(interface, invocation, response);
    // g_free (response);
    return TRUE;
}

/*
   DBUS name acquired
*/
static void on_name_acquired (GDBusConnection *connection, const gchar *name, gpointer user_data)
{
    (void)connection;
    (void)user_data;
    g_warning("DBUS name acquired '%s'", name);
}

/*
   DBUS name lost
   TODO: Should we restart if this happens?
*/
static void on_name_lost (GDBusConnection *connection, const gchar *name, gpointer user_data)
{
    (void)connection;
    (void)user_data;
    g_warning("DBUS name lost '%s'", name);
}

/*
    MAIN
*/
int main(int argc, char **argv)
{
    (void)argv;
    (void)argc;

    if (pthread_mutex_init(&state.lock, NULL) != 0)
    {
        g_error("mutex init failed");
        exit(123);
    }

    g_info("initialize_state()");
    initialize_state();

    signal(SIGINT, int_handler);
    signal(SIGTERM, int_handler);

    // Create a UDP listener for mesh messages about devices connected to other access points in same LAN
    g_info("create_socket_service()");
    socket_service = create_socket_service(&state);

    g_info("\n\nStarting\n\n");

    conn = g_bus_get_sync(G_BUS_TYPE_SYSTEM, NULL, NULL);
    if (conn == NULL)
    {
        g_warning("Not able to get connection to system bus\n");
        return 1;
    }

    // Grab zero time = time when started
    time(&started);

    g_warning("calling g_bus_own_name");

    piSniffer * sniffer = pi_sniffer_skeleton_new ();

    // TODO: Sample of setting properties
    pi_sniffer_set_distance_limit(sniffer, 7.5);

    // TODO: Experimental DBus
    g_signal_connect(sniffer, "handle-status", G_CALLBACK(on_handle_status_request), &state);

    GError* error = NULL;
    if (!g_dbus_interface_skeleton_export (G_DBUS_INTERFACE_SKELETON (sniffer),
                                         conn,
                                         "/com/signswift/reporter",
                                         &error))
    {
        g_warning("Failed to export skeleton");
    }
    else
    {
        g_warning("Exported skeleton, DBUS ready!");
    }

    guint name_connection_id = g_bus_own_name_on_connection(conn,
        "com.signswift.reporter",
        G_BUS_NAME_OWNER_FLAGS_ALLOW_REPLACEMENT,
        on_name_acquired,
        on_name_lost,
        NULL,
        NULL);

    loop = g_main_loop_new(NULL, FALSE);

#ifdef MQTT
    prepare_mqtt(state.mqtt_server, state.mqtt_topic, 
        state.local->client_id, 
        "",  // no suffix on client id for MQTT
        state.mqtt_username, state.mqtt_password);
#endif
    // MQTT send
    g_timeout_add_seconds(5, mqtt_refresh, loop);

    // Every 59s dump all devices
    // Also clear starting flag
    g_timeout_add_seconds(59, dump_all_devices_tick, loop);

    // Every 30s report counts
    g_timeout_add_seconds(20, report_counts, loop);

    // Every 5 min dump access point metadata
    g_timeout_add_seconds(301, print_access_points_tick, loop);

    // Flash the led N times for N people present
    g_timeout_add(300, flash_led, loop);

    g_info(" ");
    g_info(" ");
    g_info(" ");

    display_state();

    g_info("Start main loop\n");

    g_main_loop_run(loop);

    g_info("END OF MAIN LOOP RUN\n");

//fail:
    g_bus_unown_name(name_connection_id);

    g_dbus_connection_close_sync(conn, NULL, NULL);
    g_object_unref(conn);
    return 0;
}

void int_handler(int dummy)
{
    (void)dummy;

    g_main_loop_quit(loop);
    g_main_loop_unref(loop);

    g_dbus_connection_close_sync(conn, NULL, NULL);
    g_object_unref(conn);

    //g_bus_unown_name(name_connection_id);

#ifdef MQTT
    exit_mqtt();
#endif
    close_socket_service(socket_service);

    pthread_mutex_destroy(&state.lock);

    g_info("Clean exit\n");

    exit(0);
}
