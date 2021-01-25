#include <glib.h>
#include <gio/gio.h>
#include <stdbool.h>
#include <stdio.h>
#include <time.h>
#include "rooms.h"

#include "state.h"

static char client_id[META_LENGTH];

/*
    Initialize state from environment
*/
void initialize_state(struct OverallState* state)
{
    // Default values if not set in environment variables
    const char *description = "Please set a HOST_DESCRIPTION in the environment variables";
    const char *platform = "Please set a HOST_PLATFORM in the environment variables";
    int rssi_one_meter = -64;    // fairly typical RPI3 and iPhone
    float rssi_factor = 3.5;     // fairly cluttered indoor default
    float people_distance = 7.0; // 7m default range
    state->udp_mesh_port = 7779;
    state->udp_sign_port = 0;    // 7778;
    state->reboot_hour = 7;      // reboot after 7 hours (TODO: Make this time of day)
    state->access_points = NULL; // linked list
    state->patches = NULL;       // linked list
    state->groups = NULL;        // linked list
    state->closest_n = 0;        // count of closest
    state->patch_hash = 0;       // hash to detect changes
    state->beacons = NULL;       // linked list
    state->json = NULL;          // DBUS JSON message
    time(&state->influx_last_sent);
    time(&state->webhook_last_sent);
    time(&state->dbus_last_sent);
    state->dbus_last_sent = state->dbus_last_sent - 12 * 60 * 60;  // so it fires on startup
    state->min_gap_seconds = 300;           // min time for DBus message, starts at 5 min
    state->max_gap_seconds = 12 * 60 * 60;  // twelve hours max

    // no devices yet
    state->n = 0;

    if (gethostname(client_id, META_LENGTH) != 0)
    {
        g_error("Could not get host name");
        exit(EXIT_FAILURE);
    }
    g_debug("Host name: %s", client_id);

    state->network_up = FALSE; // is_any_interface_up();
    state->web_polling = FALSE; // until first request

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

    state->local = add_access_point(&state->access_points, client_id, description, platform,
                                   rssi_one_meter, rssi_factor, people_distance);

    // Reboot nightly 
    get_int_env("REBOOT_HOUR", &state->reboot_hour, 0);

    // UDP Settings
    get_int_env("UDP_MESH_PORT", &state->udp_mesh_port, 0);
    get_int_env("UDP_SIGN_PORT", &state->udp_sign_port, 0);
    // Metadata passed to the display to adjust how it displays the values sent
    // TODO: Expand this to an arbitrary JSON blob
    get_float_env("UDP_SCALE_FACTOR", &state->udp_scale_factor, 1.0);

    // MQTT Settings

    get_string_env("MQTT_TOPIC", &state->mqtt_topic, "BLF");  // sorry, historic name
    get_string_env("MQTT_SERVER", &state->mqtt_server, "");
    get_string_env("MQTT_USERNAME", &state->mqtt_username, "");
    get_string_env("MQTT_PASSWORD", &state->mqtt_password, "");

    // INFLUX DB

    get_int_env("INFLUX_MIN_PERIOD", &state->influx_min_period_seconds, 5 *60);      // at most every 5 min
    get_int_env("INFLUX_MAX_PERIOD", &state->influx_max_period_seconds, 60 *60);     // at least every 60 min

    state->influx_server = getenv("INFLUX_SERVER");
    if (state->influx_server == NULL) state->influx_server = "";

    get_int_env("INFLUX_PORT", &state->influx_port, 8086);

    state->influx_database = getenv("INFLUX_DATABASE");
    state->influx_username = getenv("INFLUX_USERNAME");
    state->influx_password = getenv("INFLUX_PASSWORD");

    // WEBHOOK

    get_int_env("WEBHOOK_MIN_PERIOD", &state->webhook_min_period_seconds, 5 *60);      // at most every 5 min
    get_int_env("WEBHOOK_MAX_PERIOD", &state->webhook_max_period_seconds, 60 *60);     // at least every 60 min

    get_string_env("WEBHOOK_DOMAIN", &state->webhook_domain, NULL);
    get_int_env("WEBHOOK_PORT", &state->webhook_port, 80);
    get_string_env("WEBHOOK_PATH", &state->webhook_path, "/api/bluetooth");
    get_string_env("WEBHOOK_USERNAME", &state->webhook_username, "");
    get_string_env("WEBHOOK_PASSWORD", &state->webhook_password, "");

    get_string_env("CONFIG", &state->configuration_file_path, "/etc/sniffer/config.json");

    state->verbosity = Distances; // default verbosity
    char* verbosity = getenv("VERBOSITY");
    if (verbosity){
        if (strcmp(verbosity, "counts")) state->verbosity = Counts;
        if (strcmp(verbosity, "distances")) state->verbosity = Distances;
        if (strcmp(verbosity, "details")) state->verbosity = Details;
    }

    read_configuration_file(state->configuration_file_path, &state->access_points, &state->beacons);

    g_debug("Completed read of configuration file");
}

void display_state(struct OverallState* state)
{
    g_info("HOST_NAME = %s", state->local->client_id);
    g_info("HOST_DESCRIPTION = %s", state->local->description);
    g_info("HOST_PLATFORM = %s", state->local->platform);

    g_info("REBOOT_HOUR = %i", state->reboot_hour);

    g_info("RSSI_ONE_METER Power at 1m : %i", state->local->rssi_one_meter);
    g_info("RSSI_FACTOR to distance : %.1f   (typically 2.0 (indoor, cluttered) to 4.0 (outdoor, no obstacles)", state->local->rssi_factor);
    g_info("PEOPLE_DISTANCE : %.1fm (cutoff)", state->local->people_distance);

    g_info("UDP_MESH_PORT=%i", state->udp_mesh_port);
    g_info("UDP_SIGN_PORT=%i", state->udp_sign_port);
    g_info("UDP_SCALE_FACTOR=%.1f", state->udp_scale_factor);

    g_info("VERBOSITY=%i", state->verbosity);

    g_info("MQTT_TOPIC='%s'", state->mqtt_topic);
    g_info("MQTT_SERVER='%s'", state->mqtt_server);
    g_info("MQTT_USERNAME='%s'", state->mqtt_username);
    g_info("MQTT_PASSWORD='%s'", state->mqtt_password == NULL ? "(null)" : "*****");

    g_info("INFLUX_SERVER='%s'", state->influx_server);
    g_info("INFLUX_PORT=%i", state->influx_port);
    g_info("INFLUX_DATABASE='%s'", state->influx_database);
    g_info("INFLUX_USERNAME='%s'", state->influx_username);
    g_info("INFLUX_PASSWORD='%s'", state->influx_password == NULL ? "(null)" : "*****");
    g_info("INFLUX_MIN_PERIOD='%i'", state->influx_min_period_seconds);
    g_info("INFLUX_MAX_PERIOD='%i'", state->influx_max_period_seconds);

    g_info("WEBHOOK_DOMAIN/PORT/PATH='%s:%i%s'", state->webhook_domain == NULL ? "(null)" : state->webhook_domain, state->webhook_port, state->webhook_path);
    g_info("WEBHOOK_USERNAME='%s'", state->webhook_username == NULL ? "(null)" : "*****");
    g_info("WEBHOOK_PASSWORD='%s'", state->webhook_password == NULL ? "(null)" : "*****");
    g_info("WEBHOOK_MIN_PERIOD='%i'", state->webhook_min_period_seconds);
    g_info("WEBHOOK_MAX_PERIOD='%i'", state->webhook_max_period_seconds);

    g_info("CONFIG='%s'", state->configuration_file_path == NULL ? "** Please set a path to config.json **" : state->configuration_file_path);

    int count = 0;
    for (struct AccessPoint* ap = state->access_points; ap != NULL; ap=ap->next){ count ++; }
    g_info("ACCESS_POINTS: %i", count);
}
