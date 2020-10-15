/*
    Calls DBUS to get status in JSON format
*/

#include "utility.h"
#include "sniffer-generated.h"

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
#include <signal.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <argp.h>

/*
      Connection to DBUS
*/
GDBusConnection *conn;

// Handle Ctrl-c
void int_handler(int);

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    //int rc;

    g_info("Query json status");

    signal(SIGINT, int_handler);
    signal(SIGTERM, int_handler);

    g_info("Starting");

    conn = g_bus_get_sync(G_BUS_TYPE_SYSTEM, NULL, NULL);
    if (conn == NULL)
    {
        g_warning("Not able to get connection to system bus");
        return 1;
    }


    piSniffer* proxy;
    GError *error;

    error = NULL;
    proxy = pi_sniffer_proxy_new_for_bus_sync (
                G_BUS_TYPE_SYSTEM,
                G_DBUS_PROXY_FLAGS_NONE,
                "com.signswift.sniffer",
                "/com/signswift/sniffer",      /* object */
                NULL,                          /* GCancellable* */
                &error);
    /* do stuff with proxy */

    char* json;

    if (pi_sniffer_call_status_sync(proxy, &json, NULL, &error))
    {
        g_print("%s", json);
    }
    else
    {
        g_warning("%s", error->message);
    }

    g_free(json);
    g_object_unref (proxy);
    g_dbus_connection_close_sync(conn, NULL, NULL);
    g_object_unref(conn);

    exit(EXIT_SUCCESS);
}

void int_handler(int dummy)
{
    g_info("Int handler %i", dummy);

    g_dbus_connection_close_sync(conn, NULL, NULL);
    g_object_unref(conn);

    g_info("Clean exit");

    exit(0);
}