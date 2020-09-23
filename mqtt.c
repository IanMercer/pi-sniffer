/*
   MQTT Sender for Pi-sniffer

   Receives messages over DBUS from sniffer, sends them over MQTT, displays etc.
   Decoupled this way means 
       * the core code has no dependencies
       * MQTT driver can restart if it has issues without affecting core
       * core can have multiple senders: MQTT, HTTP, Paper-white display, ...
*/

#include "utility.h"
#include "mqtt_send.h"
#include "udp.h"
#include "kalman.h"
#include "device.h"
#include "bluetooth.h"
#include "influx.h"

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
#include <string.h>
#include <udp.h>
#include <argp.h>

// For LED flash
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>

static char client_id[META_LENGTH];
static struct OverallState state;
/*
      Connection to DBUS
*/
GDBusConnection *conn;
// Time when service started running (used to print a delta time)
static time_t started;

// Handle Ctrl-c
void int_handler(int);

void initialize_state()
{
    // Default values if not set in environment variables
    const char *description = "Please set a HOST_DESCRIPTION in the environment variables";
    const char *platform = "Please set a HOST_PLATFORM in the environment variables";
    float position_x = -1.0;
    float position_y = -1.0;
    float position_z = -1.0;
    int rssi_one_meter = -64;    // fairly typical RPI3 and iPhone
    float rssi_factor = 3.5;     // fairly cluttered indoor default
    float people_distance = 7.0; // 7m default range
    state.udp_mesh_port = 7779;
    state.udp_sign_port = 0; // 7778;

    // no devices yet
    state.n = 0;

    gethostname(client_id, META_LENGTH);

    // Optional metadata about the access point for dashboard
    const char *s_client_id = getenv("HOST_NAME");
    const char *s_client_description = getenv("HOST_DESCRIPTION");
    const char *s_client_platform = getenv("HOST_PLATFORM");

    if (s_client_id != NULL)
        strncpy(client_id, s_client_id, META_LENGTH);
    // These two can just be pointers to the constant strings or the supplied metadata
    if (s_client_description != NULL)
        description = s_client_description;
    if (s_client_platform != NULL)
        platform = s_client_platform;

    const char *s_position_x = getenv("POSITION_X");
    const char *s_position_y = getenv("POSITION_Y");
    const char *s_position_z = getenv("POSITION_Z");

    if (s_position_x != NULL)
        position_x = (float)atof(s_position_x);
    if (s_position_y != NULL)
        position_y = (float)atof(s_position_y);
    if (s_position_z != NULL)
        position_z = (float)atof(s_position_z);

    const char *s_rssi_one_meter = getenv("RSSI_ONE_METER");
    const char *s_rssi_factor = getenv("RSSI_FACTOR");
    const char *s_people_distance = getenv("PEOPLE_DISTANCE");

    if (s_rssi_one_meter != NULL)
        rssi_one_meter = atoi(s_rssi_one_meter);
    if (s_rssi_factor != NULL)
        rssi_factor = atof(s_rssi_factor);
    if (s_people_distance != NULL)
        people_distance = atof(s_people_distance);

    state.local = add_access_point(client_id, description, platform,
                                   position_x, position_y, position_z,
                                   rssi_one_meter, rssi_factor, people_distance);

    // UDP Settings

    const char *s_udp_mesh_port = getenv("UDP_MESH_PORT");
    const char *s_udp_sign_port = getenv("UDP_SIGN_PORT");
    const char *s_udp_scale_factor = getenv("UDP_SCALE_FACTOR");

    if (s_udp_mesh_port != NULL)
        state.udp_mesh_port = atoi(s_udp_mesh_port);
    if (s_udp_sign_port != NULL)
        state.udp_sign_port = atoi(s_udp_sign_port);
    if (s_udp_scale_factor != NULL)
        state.udp_scale_factor = atoi(s_udp_scale_factor);

    // MQTT Settings

    state.mqtt_topic = getenv("MQTT_TOPIC");
    if (state.mqtt_topic == NULL)
        state.mqtt_topic = "BLF"; // sorry, historic name
    state.mqtt_server = getenv("MQTT_SERVER");
    if (state.mqtt_server == NULL)
        state.mqtt_server = "";
    state.mqtt_username = getenv("MQTT_USERNAME");
    state.mqtt_password = getenv("MQTT_PASSWORD");

    state.verbosity = Distances; // default verbosity
    char* verbosity = getenv("VERBOSITY");
    if (verbosity){
        if (strcmp(verbosity, "counts")) state.verbosity = Counts;
        if (strcmp(verbosity, "distances")) state.verbosity = Distances;
        if (strcmp(verbosity, "details")) state.verbosity = Details;
    }
}

void display_state()
{
    g_info("HOST_NAME = %s", state.local->client_id);
    g_info("HOST_DESCRIPTION = %s", state.local->description);
    g_info("HOST_PLATFORM = %s", state.local->platform);
    g_info("Position: (%.1f,%.1f,%.1f)", state.local->x, state.local->y, state.local->z);

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
}


/*

                          ADAPTER CHANGED
*/

static void people_updated(GDBusConnection *conn,
                                         const gchar *sender,
                                         const gchar *path,
                                         const gchar *interface,
                                         const gchar *signal,
                                         GVariant *params,
                                         void *userdata)
{
    (void)conn;
    (void)sender;    // "1.4120"
    (void)path;      // "/org/..."
    (void)interface; // "org...."
    (void)userdata;

    //g_info("people_updated(sender=%s, path=%s, interface=%s)", sender, path, interface);
    //pretty_print("Params", params);

    const gchar *signature = g_variant_get_type_string(params);
    if (strcmp(signature, "(a{sv})") != 0)
    {
        g_warning("Invalid signature for %s: %s != %s", signal, signature, "(a{sv})");
        return;
    }

    // It's wrapped in a tuple
    GVariant *array = g_variant_get_child_value(params, 0);

    const gchar *property_name;
    GVariantIter i;
    GVariant *prop_val;
    g_variant_iter_init(&i, array);    // no need to free this
    while (g_variant_iter_next(&i, "{&sv}", &property_name, &prop_val))
    {
        double d = g_variant_get_double(prop_val);
        g_info("%s = %.2f", property_name, d);

        //post_to_influx(state, property_name, d);

        g_variant_unref(prop_val);
    }

    return;
}




guint prop_changed;
GMainLoop *loop;
static char mac_address[6];       // bytes
static char mac_address_text[18]; // string

GCancellable *socket_service;

int main(int argc, char **argv)
{
    (void)argv;
    //int rc;

    g_info("Starting MQTT service");

    if (pthread_mutex_init(&state.lock, NULL) != 0)
    {
        g_error("mutex init failed");
        exit(123);
    }

    initialize_state();

    signal(SIGINT, int_handler);
    signal(SIGTERM, int_handler);

    if (argc < 1)
    {
        g_print("MQTT Sender for Bluetooth scanner");
        g_print("   scan");
        g_print("   but first set all the environment variables according to README.md");
        return -1;
    }

    g_debug("Get mac address\n");
    get_mac_address(mac_address);
    mac_address_to_string(mac_address_text, sizeof(mac_address_text), mac_address);
    g_info("Local MAC address is: %s\n", mac_address_text);

    // Create a UDP listener for mesh messages about devices connected to other access points in same LAN
    //socket_service = create_socket_service(&state);

    g_info("Starting");

    conn = g_bus_get_sync(G_BUS_TYPE_SYSTEM, NULL, NULL);
    if (conn == NULL)
    {
        g_warning("Not able to get connection to system bus\n");
        return 1;
    }

    // Grab zero time = time when started
    time(&started);

    loop = g_main_loop_new(NULL, FALSE);

    prop_changed = g_dbus_connection_signal_subscribe(conn,
                                                      NULL,
                                                      "com.signswift.sniffer",
                                                      NULL, //"People",
                                                      "/com/signswift/sniffer",
                                                      NULL,
                                                      G_DBUS_SIGNAL_FLAGS_NONE,
                                                      people_updated,
                                                      NULL,
                                                      NULL);

    prepare_mqtt(state.mqtt_server, state.mqtt_topic, 
        state.local->client_id, "-mqtt", 
        mac_address, state.mqtt_username, state.mqtt_password);

    // MQTT send
    //g_timeout_add_seconds(5, mqtt_refresh, loop);

    // Flash the led N times for N people present
    //g_timeout_add(300, flash_led, loop);

    g_info(" ");
    g_info(" ");
    g_info(" ");

    display_state();

    g_info("Start main loop\n");

    g_main_loop_run(loop);

    g_info("END OF MAIN LOOP RUN\n");

    g_dbus_connection_signal_unsubscribe(conn, prop_changed);
    g_dbus_connection_close_sync(conn, NULL, NULL);
    g_object_unref(conn);
    return 0;
}

void int_handler(int dummy)
{
    g_info("Int handler %i", dummy);

    g_main_loop_quit(loop);
    g_main_loop_unref(loop);

    g_dbus_connection_signal_unsubscribe(conn, prop_changed);
    g_dbus_connection_close_sync(conn, NULL, NULL);
    g_object_unref(conn);

    g_info("Exit MQTT");
    g_usleep(1000);
    exit_mqtt();

    g_info("Close socket service");

    close_socket_service(socket_service);

    pthread_mutex_destroy(&state.lock);

    g_info("Clean exit");

    exit(0);
}
