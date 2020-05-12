/*
 * bluez_adapter_filter.c - Set discovery filter, Scan for bluetooth devices
 *  - Control three discovery filter parameter from command line,
 *      - auto/bredr/le
 *      - RSSI (0:very close range to -100:far away)
 *      - UUID (only one as of now)
 *  Example run: ./bin/bluez_adapter_filter bredr 100 00001105-0000-1000-8000-00805f9b34fb
 *  - This example scans for new devices after powering the adapter, if any devices
 *    appeared in /org/hciX/dev_XX_YY_ZZ_AA_BB_CC, it is monitered using "InterfaceAdded"
 *    signal and all the properties of the device is printed
 *  - Device will be removed immediately after it appears in InterfacesAdded signal, so
 *    InterfacesRemoved will be called which quits the main loop
 * gcc `pkg-config --cflags glib-2.0 gio-2.0` -Wall -Wextra -o ./bin/bluez_adapter_filter ./bluez_adapter_filter.c `pkg-config --libs glib-2.0 gio-2.0`
 */
#include <glib.h>
#include <gio/gio.h>
#include "mqtt.h"
#include "mqtt_pal.h"
#include "posix_sockets.h"
#include <stdbool.h>

/*
   Structure for reporting to MQTT
*/
struct DeviceReport {
    const char* address;
    const char* name;
    const char* addressType;
    int16_t rssi;
};


static void send_to_mqtt(const char *topic, struct DeviceReport deviceReport);

static void publish_callback(void **unused, struct mqtt_response_publish *published)
{
        (void) unused;
	/* note that published->topic_name is NOT null-terminated (here we'll change it to a c-string) */
	char *topic_name = (char *)malloc(published->topic_name_size + 1);
	memcpy(topic_name, published->topic_name, published->topic_name_size);
	topic_name[published->topic_name_size] = '\0';

	printf("Received publish('%s'): %s\n", topic_name, (const char *)published->application_message);

	free(topic_name);
}

static void *client_refresher(void *client)
{
	while (1)
	{
		mqtt_sync((struct mqtt_client *)client);
		usleep(100000U);
	}
	return NULL;
}

static void exit_example(int status, int sockfd, pthread_t *client_daemon)
{
	if (sockfd != -1)
		close(sockfd);
	if (client_daemon != NULL)
		pthread_cancel(*client_daemon);
	//return bt_shell_noninteractive_quit(EXIT_FAILURE);
	exit(status);
}

const char *addr = "192.168.0.120";
const char *port = "1883";
const char *topic = "BTLE";

static pthread_t client_daemon;
static int sockfd;
struct mqtt_client mqtt;
uint8_t sendbuf[2048]; /* sendbuf should be large enough to hold multiple whole mqtt messages */
uint8_t recvbuf[1024]; /* recvbuf should be large enough any whole mqtt message expected to be received */

/* Create an anonymous session */
const char *client_id = NULL;
/* Ensure we have a clean session */
uint8_t connect_flags = MQTT_CONNECT_CLEAN_SESSION;

static void prepare_mqtt()
{
	printf("Starting MQTT\n");

	/* open the non-blocking TCP socket (connecting to the broker) */
	sockfd = open_nb_socket(addr, port);
	if (sockfd == -1)
	{
		perror("Failed to open socket: ");
		exit_example(EXIT_FAILURE, sockfd, NULL);
	}

	printf("Opened socket\n");

	/* setup a client */
	mqtt_init(&mqtt, sockfd, sendbuf, sizeof(sendbuf), recvbuf, sizeof(recvbuf), publish_callback);
	//mqtt_connect(&mqtt, "publishing_client", NULL, NULL, 0, NULL, NULL, 0, 400);

	/* Send connection request to the broker. */
	mqtt_connect(&mqtt, client_id, NULL, NULL, 0, NULL, NULL, connect_flags, 400);

	/* check that we don't have any errors */
	if (mqtt.error != MQTT_OK)
	{
		fprintf(stderr, "error: %s\n", mqtt_error_str(mqtt.error));
		exit_example(EXIT_FAILURE, sockfd, NULL);
	}

	printf("Starting MQTT thread\n");

	/* start a thread to refresh the client (handle egress and ingree client traffic) */
	if (pthread_create(&client_daemon, NULL, client_refresher, &mqtt))
	{
		fprintf(stderr, "Failed to start client daemon.\n");
		exit_example(EXIT_FAILURE, sockfd, NULL);
	}
}

void send_to_mqtt(const char *topic, struct DeviceReport deviceReport)
{
	/* print a message */
	char application_message[256];
	snprintf(application_message, sizeof(application_message), "%s|%s|%s|%i", 
           deviceReport.address, deviceReport.addressType, deviceReport.name, deviceReport.rssi);

	mqtt_publish(&mqtt, topic, application_message, strlen(application_message) + 1, MQTT_PUBLISH_QOS_2);

	/* check for errors */
	if (mqtt.error != MQTT_OK)
	{
		fprintf(stderr, "error: %s\n", mqtt_error_str(mqtt.error));
		//exit_example(EXIT_FAILURE, sockfd, &client_daemon);
	}
}




GDBusConnection *con;
static void bluez_property_value(const gchar *key, GVariant *value)
{
    const gchar *type = g_variant_get_type_string(value);

    g_print("\t%s (%s): ", key, type);
    switch(*type) {
        case 'o':
        case 's':
            g_print("%s\n", g_variant_get_string(value, NULL));
            break;
        case 'b':
            g_print("%d\n", g_variant_get_boolean(value));
            break;
        case 'n':
            g_print("%d\n", g_variant_get_int16(value));
            break;
        case 'u':
            g_print("%d\n", g_variant_get_uint32(value));
            break;
        case 'a':
        /* TODO Handling only 'as', but not array of dicts */
            if(g_strcmp0(type, "as"))
                break;
            g_print("\n");
            const gchar *s_value;
            GVariantIter i;
            g_variant_iter_init(&i, value);
            while(g_variant_iter_next(&i, "s", &s_value)) {
                g_print("\t\t%s\n", s_value);
                //g_variant_unref(s_value); ?? how does this get free'd?
	    }
            break;
        default:
            g_print("Other (%s)\n", type);
            break;
    }
}

typedef void (*method_cb_t)(GObject *, GAsyncResult *, gpointer);

static int bluez_adapter_call_method(const char *method, GVariant *param, method_cb_t method_cb)
{
    GError *error = NULL;

    g_dbus_connection_call(con,
                 "org.bluez",
            /* TODO Find the adapter path runtime */
                 "/org/bluez/hci0",
                 "org.bluez.Adapter1",
                 method,
                 param,
                 NULL,
                 G_DBUS_CALL_FLAGS_NONE,
                 -1,
                 NULL,
                 method_cb,
                 &error);
    if(error != NULL)
        return 1;
    return 0;
}

static void bluez_get_discovery_filter_cb(GObject *con,
                      GAsyncResult *res,
                      gpointer data)
{
    (void)data;
    GVariant *result = NULL;
    result = g_dbus_connection_call_finish((GDBusConnection *)con, res, NULL);
    if(result == NULL)
        g_print("Unable to get result for GetDiscoveryFilter\n");

    if(result) {
        result = g_variant_get_child_value(result, 0);
        bluez_property_value("GetDiscoveryFilter", result);
    }
    g_variant_unref(result);
}

/*

           REPORT DEVICE TO MQTT

*/

static void report_device_to_MQTT(GVariant *properties) {
    struct DeviceReport report;
    report.address = "";
    report.name = "";
    report.addressType = "";
    report.rssi = 0;

    // DEBUG g_print("[ %s ]\n", object);
    const gchar *property_name;
    GVariantIter i;
    GVariant *prop_val;
    g_variant_iter_init(&i, properties);
    while(g_variant_iter_next(&i, "{&sv}", &property_name, &prop_val)) {

        // DEBUG bluez_property_value(property_name, prop_val);

        if (strcmp(property_name, "Address") == 0) {
            report.address = g_variant_get_string(prop_val, NULL);
        }

        if (strcmp(property_name, "Alias") == 0) {
            report.name = g_variant_get_string(prop_val, NULL);
        }

        if (strcmp(property_name, "AddressType") == 0) {
            report.addressType = g_variant_get_string(prop_val, NULL);
        }

        if (strcmp(property_name, "RSSI") == 0) {
            report.rssi = g_variant_get_int16(prop_val);
        }

        g_variant_unref(prop_val);  // Moved inside loop by Ian
    }

    g_print("Device %s %24s %8s RSSI %d\n", report.address, report.name, report.addressType, report.rssi);
    send_to_mqtt ("BTLE", report);
}

static void bluez_device_appeared(GDBusConnection *sig,
                const gchar *sender_name,
                const gchar *object_path,
                const gchar *interface,
                const gchar *signal_name,
                GVariant *parameters,
                gpointer user_data)
{
    (void)sig;
    (void)sender_name;
    (void)object_path;
    (void)interface;
    (void)signal_name;
    (void)user_data;

    GVariantIter *interfaces;
    const char *object;
    const gchar *interface_name;
    GVariant *properties;
    //int rc;

    g_variant_get(parameters, "(&oa{sa{sv}})", &object, &interfaces);

    while(g_variant_iter_next(interfaces, "{&s@a{sv}}", &interface_name, &properties)) {
        if(g_strstr_len(g_ascii_strdown(interface_name, -1), -1, "device")) {
            report_device_to_MQTT(properties);
        }
        g_variant_unref(properties);
    }
/*
    rc = bluez_adapter_call_method("RemoveDevice", g_variant_new("(o)", object));
    if(rc)
        g_print("Not able to remove %s\n", object);
*/
    return;
}

#define BT_ADDRESS_STRING_SIZE 18
static void bluez_device_disappeared(GDBusConnection *sig,
                const gchar *sender_name,
                const gchar *object_path,
                const gchar *interface,
                const gchar *signal_name,
                GVariant *parameters,
                gpointer user_data)
{
    (void)sig;
    (void)sender_name;
    (void)object_path;
    (void)interface;
    (void)signal_name;
    (void)user_data;

    GVariantIter *interfaces;
    const char *object;
    const gchar *interface_name;
    char address[BT_ADDRESS_STRING_SIZE] = {'\0'};

    g_variant_get(parameters, "(&oas)", &object, &interfaces);
    while(g_variant_iter_next(interfaces, "s", &interface_name)) {
        if(g_strstr_len(g_ascii_strdown(interface_name, -1), -1, "device")) {
            int i;
            char *tmp = g_strstr_len(object, -1, "dev_") + 4;

            for(i = 0; *tmp != '\0'; i++, tmp++) {
                if(*tmp == '_') {
                    address[i] = ':';
                    continue;
                }
                address[i] = *tmp;
            }
            g_print("Device %s removed\n", address);
            // why quit? g_main_loop_quit((GMainLoop *)user_data);
        }
    }
    return;
}

/*

                          ADAPTER CHANGED

      Notify scanner to run

*/

static bool pending = FALSE;


static void bluez_signal_adapter_changed(GDBusConnection *conn,
                    const gchar *sender,
                    const gchar *path,
                    const gchar *interface,
                    const gchar *signal,
                    GVariant *params,
                    void *userdata)
{
    (void)conn;
    (void)sender;
    (void)path;
    (void)interface;
    (void)userdata;

    GVariantIter *properties = NULL;
    GVariantIter *unknown = NULL;
    const char *iface;
    const char *key;
    GVariant *value = NULL;
    const gchar *signature = g_variant_get_type_string(params);

    //g_print("Adapter changed %s\n", interface);

    if(strcmp(signature, "(sa{sv}as)") != 0) {
        g_print("Invalid signature for %s: %s != %s", signal, signature, "(sa{sv}as)");
        goto done;
    }

    g_variant_get(params, "(&sa{sv}as)", &iface, &properties, &unknown);
    while(g_variant_iter_next(properties, "{&sv}", &key, &value)) {
        if(!g_strcmp0(key, "Powered")) {
            if(!g_variant_is_of_type(value, G_VARIANT_TYPE_BOOLEAN)) {
                g_print("Invalid argument type for %s: %s != %s", key,
                        g_variant_get_type_string(value), "b");
                goto done;
            }
            g_print("Adapter is Powered \"%s\"\n", g_variant_get_boolean(value) ? "on" : "off");
        }
        else if(!g_strcmp0(key, "Discovering")) {
            if(!g_variant_is_of_type(value, G_VARIANT_TYPE_BOOLEAN)) {
                g_print("Invalid argument type for %s: %s != %s", key,
                        g_variant_get_type_string(value), "b");
                goto done;
            }
            g_print("Adapter scan \"%s\"\n", g_variant_get_boolean(value) ? "on" : "off");
        }
        else {

            pending = TRUE;
            g_print("Adapter changes %s\n", key);

            if (!g_strcmp0(key, "RSSI")) {
               //int16_t rssi = g_variant_get_int16(value);
               //g_print("RSSI %d", rssi);
            }
            else {
              //g_print("%s a %s", key, g_variant_get_type_string(value));
            }
        }
    }
done:
    if(properties != NULL)
        g_variant_iter_free(properties);
    if(value != NULL)
        g_variant_unref(value);
}

static int bluez_adapter_set_property(const char *prop, GVariant *value)
{
    GVariant *result;
    GError *error = NULL;

    result = g_dbus_connection_call_sync(con,
                         "org.bluez",
                         "/org/bluez/hci0",
                         "org.freedesktop.DBus.Properties",
                         "Set",
                         g_variant_new("(ssv)", "org.bluez.Adapter1", prop, value),
                         NULL,
                         G_DBUS_CALL_FLAGS_NONE,
                         -1,
                         NULL,
                         &error);
    if(error != NULL)
        return 1;

    g_variant_unref(result);
    return 0;
}

static int bluez_set_discovery_filter()
{
    int rc;
    GVariantBuilder *b = g_variant_builder_new(G_VARIANT_TYPE_VARDICT);
    g_variant_builder_add(b, "{sv}", "Transport", g_variant_new_string("le"));  // or "auto"
    //g_variant_builder_add(b, "{sv}", "RSSI", g_variant_new_int16(-150));
    g_variant_builder_add(b, "{sv}", "DuplicateData", g_variant_new_boolean(FALSE));
    g_variant_builder_add(b, "{sv}", "Discoverable", g_variant_new_boolean(TRUE));
    g_variant_builder_add(b, "{sv}", "Pairable", g_variant_new_boolean(TRUE));
    g_variant_builder_add(b, "{sv}", "DiscoveryTimeout", g_variant_new_uint32(0));

    //GVariantBuilder *u = g_variant_builder_new(G_VARIANT_TYPE_STRING_ARRAY);
    //g_variant_builder_add(u, "s", argv[3]);
    //g_variant_builder_add(b, "{sv}", "UUIDs", g_variant_builder_end(u));

    GVariant *device_dict = g_variant_builder_end(b);
    //g_variant_builder_unref(u);
    g_variant_builder_unref(b);

    rc = bluez_adapter_call_method("SetDiscoveryFilter", g_variant_new_tuple(&device_dict, 1), NULL);
    if(rc) {
        g_print("Not able to set discovery filter\n");
        return 1;
    }

    rc = bluez_adapter_call_method("GetDiscoveryFilters",
            NULL,
            bluez_get_discovery_filter_cb);
    if(rc) {
        g_print("Not able to get discovery filter\n");
        return 1;
    }
    return 0;
}


static void bluez_list_devices(GDBusConnection *con,
				GAsyncResult *res,
				gpointer data)
{
	(void)data;
	GVariant *result = NULL;
	GVariantIter i;
	const gchar *object_path;
	GVariant *ifaces_and_properties;

	result = g_dbus_connection_call_finish(con, res, NULL);
	if(result == NULL)
		g_print("Unable to get result for GetManagedObjects\n");

	/* Parse the result */
	if(result) {
		result = g_variant_get_child_value(result, 0);
		g_variant_iter_init(&i, result);

		while(g_variant_iter_next(&i, "{&o@a{sa{sv}}}", &object_path, &ifaces_and_properties)) {
			const gchar *interface_name;
			GVariant *properties;
			GVariantIter ii;
			g_variant_iter_init(&ii, ifaces_and_properties);
			while(g_variant_iter_next(&ii, "{&s@a{sv}}", &interface_name, &properties)) {
				if(g_strstr_len(g_ascii_strdown(interface_name, -1), -1, "device")) {
					report_device_to_MQTT(properties);
				}
				g_variant_unref(properties);
			}
			g_variant_unref(ifaces_and_properties);
		}
		g_variant_unref(result);
	}
	//g_main_loop_quit((GMainLoop *)data);
}




int get_managed_objects(void * parameters)
{
    GMainLoop * loop = (GMainLoop*) parameters;

    if (!pending) return TRUE;

    // Not pending until something changes
    pending = FALSE;

    g_dbus_connection_call(con,
				"org.bluez",
				"/",
				"org.freedesktop.DBus.ObjectManager",
				"GetManagedObjects",
				NULL,
				G_VARIANT_TYPE("(a{oa{sa{sv}}})"),
				G_DBUS_CALL_FLAGS_NONE,
				-1,
				NULL,
				(GAsyncReadyCallback)bluez_list_devices,
				loop);
    return TRUE;
}



int main(int argc, char **argv)
{
    GMainLoop *loop;
    int rc;
    guint prop_changed;
    guint iface_added;
    guint iface_removed;
    //guint getmanagedobjects;

    con = g_bus_get_sync(G_BUS_TYPE_SYSTEM, NULL, NULL);
    if(con == NULL) {
        g_print("Not able to get connection to system bus\n");
        return 1;
    }

    loop = g_main_loop_new(NULL, FALSE);

    prop_changed = g_dbus_connection_signal_subscribe(con,
                        "org.bluez",
                        "org.freedesktop.DBus.Properties",
                        "PropertiesChanged",
                        NULL,
                        NULL,
                        G_DBUS_SIGNAL_FLAGS_NONE,
                        bluez_signal_adapter_changed,
                        NULL,
                        NULL);

    iface_added = g_dbus_connection_signal_subscribe(con,
                            "org.bluez",
                            "org.freedesktop.DBus.ObjectManager",
                            "InterfacesAdded",
                            NULL,
                            NULL,
                            G_DBUS_SIGNAL_FLAGS_NONE,
                            bluez_device_appeared,
                            loop,
                            NULL);

    iface_removed = g_dbus_connection_signal_subscribe(con,
                            "org.bluez",
                            "org.freedesktop.DBus.ObjectManager",
                            "InterfacesRemoved",
                            NULL,
                            NULL,
                            G_DBUS_SIGNAL_FLAGS_NONE,
                            bluez_device_disappeared,
                            loop,
                            NULL);

    rc = bluez_adapter_set_property("Powered", g_variant_new("b", TRUE));
    if(rc) {
        g_print("Not able to enable the adapter\n");
        goto fail;
    }


    rc = bluez_set_discovery_filter(argv);
    if(rc) {
        g_print("Not able to set discovery filter\n");
        goto fail;
    }

    rc = bluez_adapter_call_method("StartDiscovery", NULL, NULL);
    if(rc) {
        g_print("Not able to scan for new devices\n");
        goto fail;
    }
    g_print("Started discovery\n");

    // Ever 2s but only if something has changed (pending)
    g_timeout_add_seconds (2, get_managed_objects, loop);

    prepare_mqtt();


    g_main_loop_run(loop);


    g_print("END OF MAIN LOOP RUN\n");

    if(argc > 3) {
        rc = bluez_adapter_call_method("SetDiscoveryFilter", NULL, NULL);
        if(rc)
            g_print("Not able to remove discovery filter\n");
    }

    rc = bluez_adapter_call_method("StopDiscovery", NULL, NULL);
    if(rc)
        g_print("Not able to stop scanning\n");
    g_usleep(100);

    rc = bluez_adapter_set_property("Powered", g_variant_new("b", FALSE));
    if(rc)
        g_print("Not able to disable the adapter\n");
fail:
    g_dbus_connection_signal_unsubscribe(con, prop_changed);
    g_dbus_connection_signal_unsubscribe(con, iface_added);
    g_dbus_connection_signal_unsubscribe(con, iface_removed);
    g_object_unref(con);
    return 0;
}
