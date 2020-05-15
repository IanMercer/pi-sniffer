/*
 * bluez sniffer
 *  Sends bluetooth information to MQTT with a separate topic per device and parameter
 *  BLE/<device mac>/parameter
 *
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
    char* address;
    char* name;
    char* alias;
    char* addressType;
    int16_t rssi;
    int16_t txpower;
    int32_t manufacturer;
    bool paired;
    bool connected;
    bool trusted;
    int manufacturer_data_length;
    unsigned char* manufacturer_data;
    uint32_t deviceclass;  // https://www.bluetooth.com/specifications/assigned-numbers/Baseband/
    uint16_t appearance;
    char** uuids;
    int uuids_length;
};

void get_address_from_path(char* address, int length, const char* path){
    int i;
    char *tmp = g_strstr_len(path, -1, "dev_") + 4;

    address[length-1] = '\0';  // safety

    for(i = 0; *tmp != '\0'; i++, tmp++) {
        if (i >= length-1) {
          break;
        }
        if(*tmp == '_') {
            address[i] = ':';
        } else {
            address[i] = *tmp;
        }
    }
}

/*
  pretty_print a GVariant with a label
*/
static void pretty_print2(const char* field_name, GVariant* value, gboolean types)
{
	gchar* pretty = g_variant_print(value, types);
	g_print("%s %s\n", field_name, pretty);
	g_free(pretty);
}

static void pretty_print(const char* field_name, GVariant* value)
{
	gchar* pretty = g_variant_print(value, FALSE);
	g_print("%s %s\n", field_name, pretty);
	g_free(pretty);
}


//static void send_to_mqtt(struct DeviceReport deviceReport);

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
		usleep(10000U);
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
uint8_t sendbuf[8192]; /* sendbuf should be large enough to hold multiple whole mqtt messages */
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

void send_to_mqtt_null(char* mac_address, char* key)
{
        /* Create topic including mac address */
        char topic[256];
        snprintf(topic, sizeof(topic), "%s/%s/%s", "BLE", mac_address, key);

	printf("MQTT %s %s\n", topic, "NULL");

	mqtt_publish(&mqtt, topic, "", 0, MQTT_PUBLISH_QOS_0 | MQTT_PUBLISH_RETAIN);

	/* check for errors */
	if (mqtt.error != MQTT_OK)
	{
	    fprintf(stderr, "error: %s\n", mqtt_error_str(mqtt.error));
	}
}

void send_to_mqtt_single(char* mac_address, char* key, char* value)
{
        /* Create topic including mac address */
        char topic[256];
        snprintf(topic, sizeof(topic), "%s/%s/%s", "BLE", mac_address, key);

	printf("MQTT %s %s\n", topic, value);

	mqtt_publish(&mqtt, topic, value, strlen(value) + 1, MQTT_PUBLISH_QOS_0 | MQTT_PUBLISH_RETAIN);

	/* check for errors */
	if (mqtt.error != MQTT_OK)
	{
	    fprintf(stderr, "error: %s\n", mqtt_error_str(mqtt.error));
	}
}

void send_to_mqtt_array(char* mac_address, char* key, unsigned char* value, int length)
{
        /* Create topic including mac address */
        char topic[256];
        snprintf(topic, sizeof(topic), "%s/%s/%s", "BLE", mac_address, key);

	printf("MQTT %s bytes[%d]\n", topic, length);

	mqtt_publish(&mqtt, topic, value, length, MQTT_PUBLISH_QOS_0 | MQTT_PUBLISH_RETAIN);

	/* check for errors */
	if (mqtt.error != MQTT_OK)
	{
	    fprintf(stderr, "error: %s\n", mqtt_error_str(mqtt.error));
	}
}

void send_to_mqtt_uuids(char* mac_address, char* key, char** uuids, int length)
{
        if (uuids == NULL) return;

        /* Create topic including mac address */
        char topic[256];

        for (int i = 0; i < length; i++) {

            snprintf(topic, sizeof(topic), "%s/%s/%s/%d", "BLE", mac_address, key, i);
            char* uuid = uuids[i];

	    printf("MQTT %s uuid[%d]\n", topic, strlen(uuid));

	    mqtt_publish(&mqtt, topic, uuid, strlen(uuid) + 1, MQTT_PUBLISH_QOS_0 | MQTT_PUBLISH_RETAIN);

	    /* check for errors */
	    if (mqtt.error != MQTT_OK)
	    {
	        fprintf(stderr, "error: %s\n", mqtt_error_str(mqtt.error));
	    }
	}
}


void send_to_mqtt_single_value(char* mac_address, char* key, int32_t value)
{
	char rssi[12];
	snprintf(rssi, sizeof(rssi), "%i", value);
	send_to_mqtt_single(mac_address, key, rssi);
}

GHashTable* hash = NULL;

// Free an allocated device

void device_report_free(void* object) {
  struct DeviceReport* val = (struct DeviceReport*) object;
  /* Free any members you need to */
  g_free(val->address);
  g_free(val->name);
  g_free(val->alias);
  g_free(val->manufacturer_data);
  g_free(val);
}


void send_to_mqtt(struct DeviceReport report)
{
	if (hash == NULL) {
		g_print("Starting a new hash table\n");
		hash = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, device_report_free);
	}
	else {
		// g_print("Send to MQTT %s\n", report.address);
	}

	struct DeviceReport* existing;

	if (!g_hash_table_contains(hash, report.address))
	{
		existing = g_malloc0(sizeof(struct DeviceReport));
		g_hash_table_insert(hash,strdup(report.address), existing);

	        // dummy struct filled with unmatched values
	        existing->address = NULL;
		existing->name = NULL;
		existing->alias = NULL;
                existing->addressType = NULL;
		existing->rssi = -1;
		existing->txpower = -1;
		existing->connected = FALSE;
		existing->trusted = FALSE;
		existing->paired = FALSE;
		existing->deviceclass = 0;
	        existing->manufacturer = 0;
                existing->manufacturer_data_length = 0;
		existing->manufacturer_data = NULL;
                existing->appearance = 0;
                existing->uuids = NULL;
                existing->uuids_length = 0;

		g_print("Device %s %24s %8s RSSI %d\n", report.address, report.name, report.addressType, report.rssi);
	} else {
		// get value from hashtable
		existing = (struct DeviceReport*) g_hash_table_lookup(hash, report.address);
		//g_print("Existing value %s\n", existing->address);
	}

	// Compare values and send
	if (g_strcmp0(existing->addressType, report.addressType) != 0) {
	  g_print("Type has changed '%s' -> '%s'  ", existing->addressType, report.addressType);
	  send_to_mqtt_single(report.address, "type", report.addressType);
	  //if (existing->addressType != NULL)
	  //    g_free(existing->addressType);
	  existing->addressType = strdup(report.addressType);
	}

	if (g_strcmp0(existing->name, report.name) != 0) {
	  g_print("Name has changed '%s' -> '%s'  ", existing->name, report.name);
	  send_to_mqtt_single(report.address, "name", report.name);
	  g_free(existing->name);
	  existing->name = strdup(report.name);
	}

	if (g_strcmp0(existing->alias, report.alias) != 0) {
	  g_print("Alias has changed '%s' -> '%s'  ", existing->alias, report.alias);
	  send_to_mqtt_single(report.address, "alias", report.alias);
	  g_free(existing->alias);
	  existing->alias = strdup(report.alias);
	}

	if (existing->rssi != report.rssi) {
	  g_print("RSSI has changed          ");
	  send_to_mqtt_single_value(report.address, "rssi", report.rssi);
	  existing->rssi = report.rssi;
	}

	if (existing->txpower != report.txpower) {
	  g_print("TXPower has changed       ");
	  send_to_mqtt_single_value(report.address, "txpower", report.txpower);
	  existing->txpower = report.txpower;
	}

	if (existing->deviceclass != report.deviceclass) {
	  g_print("Class has changed         ");
	  send_to_mqtt_single_value(report.address, "class", report.deviceclass);
	  existing->deviceclass = report.deviceclass;
	}

	if (existing->manufacturer != report.manufacturer) {
	  g_print("Manufacturer has changed  ");
	  send_to_mqtt_single_value(report.address, "manufacturer", report.manufacturer);
	  existing->manufacturer = report.manufacturer;
	}

	if (existing->appearance != report.appearance) {
	  g_print("Appearance has changed    ");
	  send_to_mqtt_single_value(report.address, "appearance", report.appearance);
	  existing->appearance = report.appearance;
	}

	if (existing->paired != report.paired) {
	  g_print("Paired has changed        ");
	  send_to_mqtt_single_value(report.address, "paired", report.paired ? 1 : 0);
	  existing->paired = report.paired;
	}

	if (existing->connected != report.connected) {
	  g_print("Connected has changed     ");
	  send_to_mqtt_single_value(report.address, "connected", report.connected ? 1 : 0);
	  existing->connected = report.connected;
	}

	if (existing->trusted != report.trusted) {
	  g_print("Trusted has changed       ");
	  send_to_mqtt_single_value(report.address, "trusted", report.trusted ? 1 : 0);
	  existing->trusted = report.trusted;
	}

	if (existing->manufacturer_data_length != report.manufacturer_data_length) {
	  g_print("ManufData has changed       ");
	  send_to_mqtt_array(report.address, "manufacturerdata", report.manufacturer_data, report.manufacturer_data_length);
	  existing->manufacturer_data_length = report.manufacturer_data_length;
	}

	if (existing->uuids_length != report.uuids_length) {
	  g_print("UUIDs has changed       ");
	  send_to_mqtt_uuids(report.address, "uuids", report.uuids, report.uuids_length);
	  existing->uuids_length = report.uuids_length;
        }

        // replace value in hash table with proper dispose on old object
        //g_hash_table_replace(hash,strdup(report.address), device_report_clone(report));
        //g_print("Already in hash table");
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
                 "org.bluez",        /* TODO Find the adapter path runtime */
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
        GVariant *child = g_variant_get_child_value(result, 0);
        bluez_property_value("GetDiscoveryFilter", child);
        g_variant_unref(child);
    }
    g_variant_unref(result);
}

/*

           REPORT DEVICE TO MQTT

*/

static void report_device_to_MQTT(GVariant *properties) {
    struct DeviceReport report;
    report.address = "";
    report.name = NULL;
    report.alias = NULL;
    report.addressType = NULL;
    report.rssi = 0;
    report.txpower = 0;
    report.paired = FALSE;
    report.connected = FALSE;
    report.trusted = FALSE;
    report.manufacturer_data = NULL;
    report.manufacturer_data_length = 0;
    report.manufacturer = 0;
    report.deviceclass = 0;
    report.appearance = 0;
    report.uuids_length = 0;
    report.uuids = NULL;

    // DEBUG g_print("[ %s ]\n", object);
    const gchar *property_name;
    GVariantIter i;
    GVariant *prop_val;
    g_variant_iter_init(&i, properties);  // no need to free this
    while(g_variant_iter_next(&i, "{&sv}", &property_name, &prop_val)) {

        // DEBUG bluez_property_value(property_name, prop_val);

        if (strcmp(property_name, "Address") == 0) {
            report.address = g_variant_dup_string(prop_val, NULL);
        }
        else if (strcmp(property_name, "Name") == 0) {
            report.name = g_variant_dup_string(prop_val, NULL);
        }
        else if (strcmp(property_name, "Alias") == 0) {
            report.alias = g_variant_dup_string(prop_val, NULL);
        }
        else if (strcmp(property_name, "AddressType") == 0) {
            report.addressType = g_variant_dup_string(prop_val, NULL);
        }
        else if (strcmp(property_name, "RSSI") == 0) {
            report.rssi = g_variant_get_int16(prop_val);
        }
        else if (strcmp(property_name, "TxPower") == 0) {
            report.txpower = g_variant_get_int16(prop_val);
        }
        else if (strcmp(property_name, "Paired") == 0) {
            report.paired = g_variant_get_boolean(prop_val);
        }
        else if (strcmp(property_name, "Connected") == 0) {
            report.connected = g_variant_get_boolean(prop_val);
        }
        else if (strcmp(property_name, "Trusted") == 0) {
            report.trusted = g_variant_get_boolean(prop_val);
        }
        else if (strcmp(property_name, "LegacyPairing") == 0) {
            // not used
        }
        else if (strcmp(property_name, "Blocked") == 0) {
            // not used
        }
        else if (strcmp(property_name, "UUIDs") == 0) {
	  pretty_print2("UUIDs", prop_val, TRUE);  // as
          //char **array = (char**) malloc((N+1)*sizeof(char*));

		char* uuidArray[2048];
		int actualLength = 0;

		GVariantIter *iter_array;
		char* str;

		g_variant_get (prop_val, "as", &iter_array);
		while (g_variant_iter_loop (iter_array, "s", &str))
		{
		    uuidArray[actualLength++] = strdup(str);
		}
		g_variant_iter_free (iter_array);

                char** allocdata = g_malloc(actualLength * sizeof(char*));
                memcpy(allocdata, uuidArray, actualLength * sizeof(char*));
		report.uuids = allocdata;
                report.uuids_length = actualLength;

		// ?? g_variant_unref(s_value);


        }
        else if (strcmp(property_name, "Modalias") == 0) {
            // not used
        }
        else if (strcmp(property_name, "Class") == 0) {        // type 'u' which is uint32
            report.deviceclass = g_variant_get_uint32(prop_val);
        }
        else if (strcmp(property_name, "Icon") == 0) {
           // A string value
           //const char* type = g_variant_get_type_string (prop_val);
           //g_print("Unknown property: %s %s\n", property_name, type);
        }
        else if (strcmp(property_name, "Appearance") == 0) {    // type 'q' which is uint16
            report.appearance = g_variant_get_uint16(prop_val);
        }
        else if (strcmp(property_name, "ServiceData") == 0) {
           // A a{sv} value

	   // {'000080e7-0000-1000-8000-00805f9b34fb': <[byte 0xb0, 0x23, 0x25, 0xcb, ...]>}
	   pretty_print2("ServiceData (batch)", prop_val, TRUE);  // a{sv}

        }
        else if (strcmp(property_name, "Adapter") == 0) {
        }
        else if (strcmp(property_name, "ServicesResolved") == 0) {
        }
        else if (strcmp(property_name, "ManufacturerData") == 0) {
	    // ManufacturerData {uint16 76: <[byte 0x10, 0x06, 0x10, 0x1a, 0x52, 0xe9, 0xc8, 0x08]>}
	    // {a(sv)}
	    //pretty_print2("ManufacturerData (batch)", prop_val, TRUE);  // a{qv}

		GVariant *s_value;
		GVariantIter i;
                uint16_t s_key;

		g_variant_iter_init(&i, prop_val);
		while(g_variant_iter_next(&i, "{qv}", &s_key, &s_value)) {
                        report.manufacturer = s_key;

                        //g_print("            k=%d", s_key);
	                //pretty_print2("           qv", s_value, TRUE);

			unsigned char byteArray[2048];
			int actualLength = 0;

			GVariantIter *iter_array;
			guchar str;

			g_variant_get (s_value, "ay", &iter_array);
			while (g_variant_iter_loop (iter_array, "y", &str))
			{
			    byteArray[actualLength++] = str;
			}
			g_variant_iter_free (iter_array);

			// TODO : malloc etc... report.manufacturerData = byteArray
                        unsigned char* allocdata = g_malloc(actualLength);
                        memcpy(allocdata, byteArray, actualLength);
			report.manufacturer_data = allocdata;
                        report.manufacturer_data_length = actualLength;

			g_variant_unref(s_value);
		}

        }
        else {
           const char* type = g_variant_get_type_string (prop_val);
           g_print("******************************************************************************\n");
           g_print("Unknown property: %s %s\n", property_name, type);
        }

        g_variant_unref(prop_val);
    }

    send_to_mqtt (report);

    g_free(report.address);
    if (report.addressType != NULL)
        g_free(report.addressType);
    if (report.name != NULL)
        g_free(report.name);
    if (report.alias != NULL)
        g_free(report.alias);
    if (report.manufacturer_data != NULL)
        g_free(report.manufacturer_data);
    if (report.uuids_length > 0) {
       for (int i = 0; i < report.uuids_length; i++){
         g_free(report.uuids[i]);
       }
    }

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

            // Report device directly, no need to scan
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

    g_variant_get(parameters, "(&oas)", &object, &interfaces);

    while(g_variant_iter_next(interfaces, "s", &interface_name)) {
        if(g_strstr_len(g_ascii_strdown(interface_name, -1), -1, "device")) {
            char address[BT_ADDRESS_STRING_SIZE];
            get_address_from_path (address, BT_ADDRESS_STRING_SIZE, object);
            g_print("Device %s removed\n", address);
	    send_to_mqtt_null(address, "rssi");
	    send_to_mqtt_null(address, "connected");
        }
        // Nope ... g_variant_unref(interface_name);
    }
    // Nope ... g_variant_unref(interfaces);
}



/*

                          ADAPTER CHANGED
*/


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
    (void)path;                         // "/org/bluez/hci0/dev_63_FE_94_98_91_D6"  Bluetooth device path
    (void)interface;                    // "org.freedesktop.DBus.Properties" ... not useful
    (void)userdata;

    GVariantIter *properties = NULL;
    GVariantIter *unknown = NULL;
    const char *iface;                  // org.bluez.Device1
    const char *key;
    GVariant *value = NULL;

    //g_print("Adapter changed %s\n", interface);

    const gchar *signature = g_variant_get_type_string(params);
    if(strcmp(signature, "(sa{sv}as)") != 0) {
        g_print("Invalid signature for %s: %s != %s", signal, signature, "(sa{sv}as)");
        goto done;
    }

    g_variant_get(params, "(&sa{sv}as)", &iface, &properties, &unknown);

    while(g_variant_iter_next(properties, "{&sv}", &key, &value)) {
        if(!g_strcmp0(key, "Powered")) {
            g_print("Adapter is Powered \"%s\"\n", g_variant_get_boolean(value) ? "on" : "off");
        }
        else if(!g_strcmp0(key, "Discovering")) {
            g_print("Adapter scan \"%s\"\n", g_variant_get_boolean(value) ? "on" : "off");
        }
        else {
            char address[BT_ADDRESS_STRING_SIZE];
            get_address_from_path (address, BT_ADDRESS_STRING_SIZE, path);

            //g_print("Adapter changes %s %s\n", address, key);

            if (!g_strcmp0(key, "RSSI")) {
		int16_t rssi = g_variant_get_int16(value);
	 	send_to_mqtt_single_value(address, "rssi", rssi);
            }
            else if (!g_strcmp0(key, "TxPower")) {
		int16_t p = g_variant_get_int16(value);
	 	send_to_mqtt_single_value(address, "txpower", p);
            }
            else if (!g_strcmp0(key, "Name")) {
                pretty_print("Name", value);
                // Name changed, will pick up at next full fetch
            }
            else if (!g_strcmp0(key, "Alias")) {
                pretty_print("Alias", value);
                // Alias changed, will pick up at next full fetch
            }
            else if (!g_strcmp0(key, "Connected")) {
                // Connected changed, will pick up at next full fetch   type=b
            }
            else if (!g_strcmp0(key, "UUIDs")) {
		pretty_print2("UUIDs (changed)", value, TRUE);
                // TODO: Decode and save to report
                // Will pick up at next full fetch   type=b
            }
            else if (!g_strcmp0(key, "ServicesResolved")) {
                // Do nothing   type=b
            }
            else if (!g_strcmp0(key, "Appearance")) {
                // Will pick up at next full fetch
            }
            else if (!g_strcmp0(key, "Icon")) {
                pretty_print("Icon", value);
                // Will pick up at next full fetch
            }
            else if (!g_strcmp0(key, "ManufacturerData")) {             //type= a{qv}
                // ManufacturerData {uint16 76: <[byte 0x10, 0x06, 0x10, 0x1a, 0x52, 0xe9, 0xc8, 0x08]>}
                pretty_print2("ManufacturerData*", value, TRUE);

                // TODO: Handle ManufacturerData changes
            }
            else if (!g_strcmp0(key, "ServiceData")) {

	        // {'000080e7-0000-1000-8000-00805f9b34fb': <[byte 0xb0, 0x23, 0x25, 0xcb, ...]>}
                pretty_print2("ServiceData*", value, TRUE);

		GVariant *s_value;
		GVariantIter i;
                gchar *s_key;

		g_variant_iter_init(&i, value);
		while(g_variant_iter_next(&i, "{sv}", &s_key, &s_value)) {
                        // uuid = s_key;

                        //g_print("            k=%d", s_key);
	                //pretty_print2("           qv", s_value, TRUE);

			unsigned char byteArray[2048];
			int actualLength = 0;

			GVariantIter *iter_array;
			guchar str;

			g_variant_get (s_value, "ay", &iter_array);
			while (g_variant_iter_loop (iter_array, "y", &str))
			{
			    byteArray[actualLength++] = str;
			}
			g_variant_iter_free (iter_array);

                        // using the guid instead of serviceData as the path for MQTT publish
                        // allows receiver to look for specific GUIDs
	                send_to_mqtt_array(address, s_key, byteArray, actualLength);

//                        unsigned char* allocdata = g_malloc(actualLength);
//                        memcpy(allocdata, byteArray, actualLength);

			g_variant_unref(s_value);
		}

            }
            else {
	 	// PING ... send_to_mqtt_single_value(address, "connected", 1);
                g_print("*** TODO: Handle '%s' a %s\n", key, g_variant_get_type_string(value));
            }
        }
        g_variant_unref(value);
    }
done:
    g_variant_iter_free(properties);
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
		GVariant *child = g_variant_get_child_value(result, 0);
		g_variant_iter_init(&i, child);

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
		g_variant_unref(child);
		g_variant_unref(result);
	}
	//g_main_loop_quit((GMainLoop *)data);
}




int get_managed_objects(void * parameters)
{
    GMainLoop * loop = (GMainLoop*) parameters;

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


/*
static void connect_reply(DBusMessage *message, void *user_data)
{
	GDBusProxy *proxy = user_data;
	DBusError error;

	dbus_error_init(&error);

	if (dbus_set_error_from_message(&error, message) == TRUE) {
		bt_shell_printf("Failed to connect: %s\n", error.name);
		dbus_error_free(&error);
		return;
	}

	g_print("Connection successful\n");

	set_default_device(proxy, NULL);
        return;
}

static void cmd_connect(int argc, char *address)
{
	GDBusProxy *proxy;

	if (check_default_ctrl() == FALSE) {
            g_print("No default controller");
            return;
        }

	proxy = find_proxy_by_address(default_ctrl->devices, address);
	if (!proxy) {
		g_print("Device %s not available\n", address);
                return;
	}

	if (g_dbus_proxy_method_call(proxy, "Connect", NULL, connect_reply,
							proxy, NULL) == FALSE) {
		g_print("Failed to connect\n");
                return;
	}

	g_print("Attempting to connect to %s\n", address);
}

*/



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

    // Ever 30s but only if something has changed (pending)
    g_timeout_add_seconds (30, get_managed_objects, loop);

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
