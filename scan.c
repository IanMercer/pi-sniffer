/*
 * bluez sniffer
 *  Sends bluetooth information to MQTT with a separate topic per device and parameter
 *  BLE/<device mac>/parameter
 *
 *  Applies a simple kalman filter to RSSI to smooth it out somewhat
 *
 *  gcc `pkg-config --cflags glib-2.0 gio-2.0` -Wall -Wextra -o ./bin/bluez_adapter_filter ./bluez_adapter_filter.c `pkg-config --libs glib-2.0 gio-2.0`
 */
#include <glib.h>
#include <gio/gio.h>
#include "mqtt.h"
#include "mqtt_pal.h"
#include "posix_sockets.h"
#include <stdbool.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <net/if.h>
#include <time.h>
#include <math.h>

/*
      Kalman filter
*/

struct Kalman
{
    float err_measure;
    float err_estimate;
    float q;
    float current_estimate;
    float last_estimate;
    float kalman_gain;
};

void kalman_initialize(struct Kalman *k)
{
    k->last_estimate = -999; // marker value
    k->err_measure = 20.0;
    k->err_estimate = 20.0;
    k->q = 0.10;
}

float kalman_update(struct Kalman *k, float mea)
{
    // First time through, use the measured value as the actual value
    if (k->last_estimate == -999)
    {
        k->last_estimate = mea;
        return mea;
    }
    //g_print("%f %f %f %f\n", k->err_measure, k->err_estimate, k->q, mea);
    k->kalman_gain = k->err_estimate / (k->err_estimate + k->err_measure);
    k->current_estimate = k->last_estimate + k->kalman_gain * (mea - k->last_estimate);
    k->err_estimate = (1.0 - k->kalman_gain) * k->err_estimate + fabs(k->last_estimate - k->current_estimate) * k->q;
    k->last_estimate = k->current_estimate;

    return k->current_estimate;
}

/*
   Structure for reporting to MQTT

   TODO: Add time_t to this struct, keep them around not clearing hash an report every five minutes
   on all still connected devices incase MQTT isn't saving values.
*/
struct DeviceReport
{
    char *name;
    char *alias;
    char *addressType;
    int32_t manufacturer;
    bool paired;
    bool connected;
    bool trusted;
    uint32_t deviceclass; // https://www.bluetooth.com/specifications/assigned-numbers/Baseband/
    uint16_t appearance;
    int manufacturer_data_length; // should use a Hash instead
    int uuids_length;             // should use a Hash instead
    struct Kalman kalman;
    time_t last_sent;
    float last_value;
};

bool get_address_from_path(char *address, int length, const char *path)
{

    if (path == NULL)
    {
        address[0] = '\0';
        return FALSE;
    }

    char *found = g_strstr_len(path, -1, "dev_");
    if (found == NULL)
    {
        address[0] = '\0';
        return FALSE;
    }

    int i;
    char *tmp = found + 4;

    address[length - 1] = '\0'; // safety

    for (i = 0; *tmp != '\0'; i++, tmp++)
    {
        if (i >= length - 1)
        {
            break;
        }
        if (*tmp == '_')
        {
            address[i] = ':';
        }
        else
        {
            address[i] = *tmp;
        }
    }
    return TRUE;
}

/*
  pretty_print a GVariant with a label
*/
static void pretty_print2(const char *field_name, GVariant *value, gboolean types)
{
    gchar *pretty = g_variant_print(value, types);
    g_print("%s %s\n", field_name, pretty);
    g_free(pretty);
}

static void pretty_print(const char *field_name, GVariant *value)
{
    gchar *pretty = g_variant_print(value, FALSE);
    g_print("%s %s\n", field_name, pretty);
    g_free(pretty);
}

static void publish_callback(void **unused, struct mqtt_response_publish *published)
{
    (void)unused;
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

static void exit_mqtt(int status, int sockfd, pthread_t *client_daemon)
{
    if (sockfd != -1)
        close(sockfd);
    if (client_daemon != NULL)
        pthread_cancel(*client_daemon);
    //return bt_shell_noninteractive_quit(EXIT_FAILURE);
    exit(status);
}

const char *topicRoot = "BLF";

static pthread_t client_daemon;
static int sockfd;
struct mqtt_client mqtt;
uint8_t sendbuf[8192]; /* sendbuf should be large enough to hold multiple whole mqtt messages */
uint8_t recvbuf[1024]; /* recvbuf should be large enough any whole mqtt message expected to be received */

/* Create an anonymous session */
const char *client_id = NULL;
/* Ensure we have a clean session */
uint8_t connect_flags = MQTT_CONNECT_CLEAN_SESSION;

static void prepare_mqtt(char *mqtt_addr, char *mqtt_port)
{
    printf("Starting MQTT %s:%s\n", mqtt_addr, mqtt_port);

    /* open the non-blocking TCP socket (connecting to the broker) */
    sockfd = open_nb_socket(mqtt_addr, mqtt_port);
    if (sockfd == -1)
    {
        perror("Failed to open socket: ");
        exit_mqtt(EXIT_FAILURE, sockfd, NULL);
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
        exit_mqtt(EXIT_FAILURE, sockfd, NULL);
    }

    printf("Starting MQTT thread\n");

    /* start a thread to refresh the client (handle egress and ingree client traffic) */
    if (pthread_create(&client_daemon, NULL, client_refresher, &mqtt))
    {
        fprintf(stderr, "Failed to start client daemon.\n");
        exit_mqtt(EXIT_FAILURE, sockfd, NULL);
    }
}

void send_to_mqtt_null(char *mac_address, char *key)
{
    /* Create topic including mac address */
    char topic[256];
    snprintf(topic, sizeof(topic), "%s/%s/%s", topicRoot, mac_address, key);

    printf("MQTT %s %s\n", topicRoot, "NULL");

    mqtt_publish(&mqtt, topic, "", 0, MQTT_PUBLISH_QOS_0); // | MQTT_PUBLISH_RETAIN);

    /* check for errors */
    if (mqtt.error != MQTT_OK)
    {
        fprintf(stderr, "error: %s\n", mqtt_error_str(mqtt.error));
    }
}

// The mac address of the wlan0 interface (every Pi has one so fairly safe to assume wlan0 exists)

static char access_point_address[6];

static void get_mac_address()
{
    int s;
    struct ifreq buffer;

    s = socket(PF_INET, SOCK_DGRAM, 0);
    memset(&buffer, 0x00, sizeof(buffer));
    strcpy(buffer.ifr_name, "wlan0");
    ioctl(s, SIOCGIFHWADDR, &buffer);
    close(s);
    memcpy(&access_point_address, &buffer.ifr_hwaddr.sa_data, 6);
    for (s = 0; s < 6; s++)
    {
        g_print("%.2X ", (unsigned char)access_point_address[s]);
    }
    g_print("\n");
}

/* SEND TO MQTT WITH ACCESS POINT MAC ADDRESS AND TIME STAMP */

void send_to_mqtt_with_time_and_mac(char *mac_address, char *key, int i, char *value, int value_length, int flags)
{
    //        g_print("send_to_mqtt_with_time_and_mac\n");

    /* Create topic including mac address */
    char topic[256];

    if (i < 0)
    {
        snprintf(topic, sizeof(topic), "%s/%s/%s", topicRoot, mac_address, key);
    }
    else
    {
        snprintf(topic, sizeof(topic), "%s/%s/%s/%d", topicRoot, mac_address, key, i);
    }

    // Add time and access point mac address to packet
    char packet[2048];

    memset(packet, 0, 14 + 20);

    time_t now = time(0);

    memcpy(&packet, &access_point_address, 6);
    memcpy(&packet[6], &now, 8);

    memcpy(&packet[6 + 8], value, value_length);
    int packet_length = value_length + 6 + 8;

    mqtt_publish(&mqtt, topic, packet, packet_length, flags);

    /* check for errors */
    if (mqtt.error != MQTT_OK)
    {
        fprintf(stderr, "error: %s\n", mqtt_error_str(mqtt.error));
    }

    /*
	int s;
        g_print(" ");

	for(s = 0; s < packet_length; s++ )
	{
	    g_print("%.2X", (unsigned char)packet[s]);
            if (s == 6 || s == 6+8) g_print(" ");
	}
        g_print("\n");
*/
}

void send_to_mqtt_single(char *mac_address, char *key, char *value)
{
    printf("MQTT %s %s/%s %s\n", mac_address, topicRoot, key, value);
    send_to_mqtt_with_time_and_mac(mac_address, key, -1, value, strlen(value) + 1, MQTT_PUBLISH_QOS_0 | MQTT_PUBLISH_RETAIN);
}

void send_to_mqtt_array(char *mac_address, char *key, unsigned char *value, int length)
{
    printf("MQTT %s %s/%s bytes[%d]\n", mac_address, topicRoot, key, length);
    send_to_mqtt_with_time_and_mac(mac_address, key, -1, (char *)value, length, MQTT_PUBLISH_QOS_0 | MQTT_PUBLISH_RETAIN);
}

void send_to_mqtt_uuids(char *mac_address, char *key, char **uuids, int length)
{
    if (uuids == NULL)
        return;

    for (int i = 0; i < length; i++)
    {
        char *uuid = uuids[i];
        printf("MQTT %s %s/%s/%d uuid[%d]\n", mac_address, topicRoot, key, i, strlen(uuid));
        send_to_mqtt_with_time_and_mac(mac_address, key, i, uuid, strlen(uuid) + 1, MQTT_PUBLISH_QOS_0 | MQTT_PUBLISH_RETAIN);
    }
}

// numeric values that change all the time not retained, all others retained by MQTT

void send_to_mqtt_single_value(char *mac_address, char *key, int32_t value)
{
    char rssi[12];
    snprintf(rssi, sizeof(rssi), "%i", value);
    printf("MQTT %s %s/%s %s\n", mac_address, topicRoot, key, rssi);
    send_to_mqtt_with_time_and_mac(mac_address, key, -1, rssi, strlen(rssi) + 1, MQTT_PUBLISH_QOS_0);
}

void send_to_mqtt_single_float(char *mac_address, char *key, float value)
{
    char rssi[12];
    snprintf(rssi, sizeof(rssi), "%.3f", value);
    printf("MQTT %s %s/%s %s\n", mac_address, topicRoot, key, rssi);
    send_to_mqtt_with_time_and_mac(mac_address, key, -1, rssi, strlen(rssi) + 1, MQTT_PUBLISH_QOS_0);
}

GHashTable *hash = NULL;

// Free an allocated device

void device_report_free(void *object)
{
    struct DeviceReport *val = (struct DeviceReport *)object;
    /* Free any members you need to */
    g_free(val->name);
    g_free(val->alias);
    g_free(val->addressType);
    g_free(val);
}

GDBusConnection *con;

typedef void (*method_cb_t)(GObject *, GAsyncResult *, gpointer);

static int bluez_adapter_call_method(const char *method, GVariant *param, method_cb_t method_cb)
{
    //    g_print("bluez_adapter_call_method(%s)\n", method);
    GError *error = NULL;

    g_dbus_connection_call(con,
                           "org.bluez", /* TODO Find the adapter path runtime */
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
    if (error != NULL)
        return 1;
    return 0;
}

static void bluez_get_discovery_filter_cb(GObject *con,
                                          GAsyncResult *res,
                                          gpointer data)
{
    g_print("bluez_get_discovery_filter_cb(...)\n");
    (void)data;

    GVariant *result = NULL;
    result = g_dbus_connection_call_finish((GDBusConnection *)con, res, NULL);
    if (result == NULL)
        g_print("Unable to get result for GetDiscoveryFilter\n");

    if (result)
    {
        GVariant *child = g_variant_get_child_value(result, 0);
        pretty_print("GetDiscoveryFilter", child);
        g_variant_unref(child);
    }
    g_variant_unref(result);
}

// trim string (https://stackoverflow.com/a/122974/224370 but simplified)
char *trim(char *str)
{
    size_t len = 0;
    char *frontp = str;
    char *endp = NULL;

    if (str == NULL)
    {
        return NULL;
    }
    if (str[0] == '\0')
    {
        return str;
    }

    len = strlen(str);
    endp = str + len;

    /* Move the front and back pointers to address the first non-whitespace
     * characters from each end.
     */
    while (*frontp == ' ')
    {
        ++frontp;
    }
    while (*(--endp) == ' ' && endp != frontp)
    {
    }

    if (endp <= frontp)
    {
        // All whitespace
        *str = '\0';
        return str;
    }

    *(endp + 1) = '\0'; // may already be zero

    /* Shift the string so that it starts at str so that if it's dynamically
     * allocated, we can still free it on the returned pointer.
     */
    if (frontp != str)
    {
        char *startp = str;
        while (*frontp)
        {
            *startp++ = *frontp++;
        }
        *startp = '\0';
    }

    return str;
}

/*

    REPORT DEVICE TO MQTT

    address if known, if not have to find it in the dictionary of passed properties
    appeared = we know this is fresh data so send RSSI and TxPower with timestamp

*/

static bool repeat = FALSE; // repeats all values every few minutes

static void report_device_to_MQTT(GVariant *properties, char *address, bool changed)
{
    //       g_print("report_device_to_MQTT(%s)\n", address);


    char *allocated_address = NULL;
    //pretty_print("report_device", properties);

    // Get address from dictionary if not already present
    GVariant *address_from_dict = g_variant_lookup_value(properties, "Address", G_VARIANT_TYPE_STRING);
    if (address_from_dict != NULL)
    {
        if (address != NULL)
        {
            g_print("ERROR found two addresses\n");
        }
        address = g_variant_dup_string(address_from_dict, NULL);
        allocated_address = address;
        g_variant_unref(address_from_dict);
    }

    if (address == NULL)
    {
        g_print("ERROR address is null");
        return;
    }

    // Get existing report

    if (hash == NULL)
    {
        g_print("Starting a new hash table\n");
        hash = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, device_report_free);
    }

    struct DeviceReport *existing;

    if (!g_hash_table_contains(hash, address))
    {
        existing = g_malloc0(sizeof(struct DeviceReport));
        g_hash_table_insert(hash, strdup(address), existing);

        // dummy struct filled with unmatched values
        existing->name = NULL;
        existing->alias = NULL;
        existing->addressType = NULL;
        existing->connected = FALSE;
        existing->trusted = FALSE;
        existing->paired = FALSE;
        existing->deviceclass = 0;
        existing->manufacturer = 0;
        existing->manufacturer_data_length = 0;
        existing->appearance = 0;
        existing->uuids_length = 0;

        // RSSI values are stored with kalman filtering
        kalman_initialize(&existing->kalman);
        time(&existing->last_sent);
        existing->last_value = 0;

        g_print("Added hash %s\n", address);
    }
    else
    {
        // get value from hashtable
        existing = (struct DeviceReport *)g_hash_table_lookup(hash, address);
        //g_print("Existing value %s\n", existing->address);
    }

    const gchar *property_name;
    GVariantIter i;
    GVariant *prop_val;
    g_variant_iter_init(&i, properties); // no need to free this
    while (g_variant_iter_next(&i, "{&sv}", &property_name, &prop_val))
    {

        if (strcmp(property_name, "Address") == 0)
        {
            // do nothing, already picked off
        }
        else if (strcmp(property_name, "Name") == 0)
        {
            char *name = g_variant_dup_string(prop_val, NULL);

            // Trim whitespace (Bad Tracker device keeps flipping name)
            trim(name);

            if (repeat || g_strcmp0(existing->name, name) != 0)
            {
                g_print("Name has changed '%s' -> '%s'  ", existing->name, name);
                send_to_mqtt_single(address, "name", name);
                if (existing->name != NULL)
                    g_free(existing->name);
                existing->name = name; // was strdup(name);
            }
        }
        else if (strcmp(property_name, "Alias") == 0)
        {
            char *alias = g_variant_dup_string(prop_val, NULL);

            trim(alias);

            if (repeat || g_strcmp0(existing->alias, alias) != 0)
            {
                g_print("Alias has changed '%s' -> '%s'  ", existing->alias, alias);
                send_to_mqtt_single(address, "alias", alias);
            }
            if (existing->alias != NULL)
                g_free(existing->alias);
            existing->alias = alias;
        }
        else if (repeat || strcmp(property_name, "AddressType") == 0)
        {
            char *addressType = g_variant_dup_string(prop_val, NULL);

            // Compare values and send
            if (repeat || g_strcmp0(existing->addressType, addressType) != 0)
            {
                g_print("Type has changed '%s' -> '%s'  ", existing->addressType, addressType);
                send_to_mqtt_single(address, "type", addressType);
            }
            if (existing->addressType != NULL)
                g_free(existing->addressType);
            existing->addressType = addressType;
        }
        else if (strcmp(property_name, "RSSI") == 0)
        {
            int16_t rssi = g_variant_get_int16(prop_val);
            //send_to_mqtt_single_value(address, "rssi", rssi);

            time_t now;
            time(&now);

            double delta_time = difftime(now, existing->last_sent);

            float averaged = kalman_update(&existing->kalman, (float)rssi);

            // 100s with RSSI change of 1 triggers send
            // 10s with RSSI change of 10 triggers send
            double score = fabs(existing->last_value - averaged) * delta_time;

            if (changed && score > 500.0)
            {
                // only send for updates, static values not interesting
                if (fabs(averaged) > 10)
                { // rssi 0 is wrong
                    send_to_mqtt_single_float(address, "rssi", averaged);
                    time(&existing->last_sent);
                }
            }
            else {
               g_print("Skip %s RSSI %d, delta time: %.0fs score %.0f\n", address, rssi, delta_time, score);
            }
        }
        else if (strcmp(property_name, "TxPower") == 0)
        {
            if (changed)
            {
                int16_t p = g_variant_get_int16(prop_val);
                send_to_mqtt_single_value(address, "txpower", p);
            }
        }

        else if (strcmp(property_name, "Paired") == 0)
        {
            bool paired = g_variant_get_boolean(prop_val);
            if (existing->paired != paired)
            {
                g_print("Paired has changed        ");
                send_to_mqtt_single_value(address, "paired", paired ? 1 : 0);
                existing->paired = paired;
            }
        }
        else if (strcmp(property_name, "Connected") == 0)
        {
            bool connected = g_variant_get_boolean(prop_val);
            if (repeat || existing->connected != connected)
            {
                g_print("Connected has changed     ");
                send_to_mqtt_single_value(address, "connected", connected ? 1 : 0);
                existing->connected = connected;
            }
        }
        else if (strcmp(property_name, "Trusted") == 0)
        {
            bool trusted = g_variant_get_boolean(prop_val);
            if (repeat || existing->trusted != trusted)
            {
                g_print("Trusted has changed       ");
                send_to_mqtt_single_value(address, "trusted", trusted ? 1 : 0);
                existing->trusted = trusted;
            }
        }
        else if (strcmp(property_name, "LegacyPairing") == 0)
        {
            // not used
        }
        else if (strcmp(property_name, "Blocked") == 0)
        {
            // not used
        }
        else if (strcmp(property_name, "UUIDs") == 0)
        {
            //pretty_print2("UUIDs", prop_val, TRUE);  // as
            //char **array = (char**) malloc((N+1)*sizeof(char*));

            char *uuidArray[2048];
            int actualLength = 0;

            GVariantIter *iter_array;
            char *str;

            g_variant_get(prop_val, "as", &iter_array);
            while (g_variant_iter_loop(iter_array, "s", &str))
            {
                uuidArray[actualLength++] = strdup(str);
            }
            g_variant_iter_free(iter_array);

            char **allocdata = g_malloc(actualLength * sizeof(char *));
            memcpy(allocdata, uuidArray, actualLength * sizeof(char *));

            if (repeat || existing->uuids_length != actualLength)
            {
                g_print("UUIDs has changed       ");
                send_to_mqtt_uuids(address, "uuids", allocdata, actualLength);
                existing->uuids_length = actualLength;
            }

            // Free up the individual UUID strings after sending them
            if (actualLength > 0)
            {
                for (int i = 0; i < actualLength; i++)
                {
                    g_free(uuidArray[i]);
                }
            }
        }
        else if (strcmp(property_name, "Modalias") == 0)
        {
            // not used
        }
        else if (strcmp(property_name, "Class") == 0)
        { // type 'u' which is uint32
            uint32_t deviceclass = g_variant_get_uint32(prop_val);
            if (repeat || existing->deviceclass != deviceclass)
            {
                g_print("Class has changed         ");
                send_to_mqtt_single_value(address, "class", deviceclass);
                existing->deviceclass = deviceclass;
            }
        }
        else if (strcmp(property_name, "Icon") == 0)
        {
            // A string value
            //const char* type = g_variant_get_type_string (prop_val);
            //g_print("Unknown property: %s %s\n", property_name, type);
        }
        else if (strcmp(property_name, "Appearance") == 0)
        { // type 'q' which is uint16
            uint16_t appearance = g_variant_get_uint16(prop_val);
            if (repeat || existing->appearance != appearance)
            {
                g_print("Appearance has changed    ");
                send_to_mqtt_single_value(address, "appearance", appearance);
                existing->appearance = appearance;
            }
        }
        else if (strcmp(property_name, "ServiceData") == 0)
        {
            // A a{sv} value

            // {'000080e7-0000-1000-8000-00805f9b34fb': <[byte 0xb0, 0x23, 0x25, 0xcb, ...]>}
            pretty_print2("ServiceData (batch)", prop_val, TRUE); // a{sv}
        }
        else if (strcmp(property_name, "Adapter") == 0)
        {
        }
        else if (strcmp(property_name, "ServicesResolved") == 0)
        {
        }
        else if (strcmp(property_name, "ManufacturerData") == 0)
        {
            // ManufacturerData {uint16 76: <[byte 0x10, 0x06, 0x10, 0x1a, 0x52, 0xe9, 0xc8, 0x08]>}
            // {a(sv)}
            //pretty_print2("ManufacturerData (batch)", prop_val, TRUE);  // a{qv}

            GVariant *s_value;
            GVariantIter i;
            uint16_t manufacturer;

            g_variant_iter_init(&i, prop_val);
            while (g_variant_iter_next(&i, "{qv}", &manufacturer, &s_value))
            { // Just one

                if (repeat || existing->manufacturer != manufacturer)
                {
                    g_print("Manufacturer has changed  ");
                    send_to_mqtt_single_value(address, "manufacturer", manufacturer);
                    existing->manufacturer = manufacturer;
                }

                unsigned char byteArray[2048];
                int actualLength = 0;

                GVariantIter *iter_array;
                guchar str;

                g_variant_get(s_value, "ay", &iter_array);
                while (g_variant_iter_loop(iter_array, "y", &str))
                {
                    byteArray[actualLength++] = str;
                }
                g_variant_iter_free(iter_array);

                // TODO : malloc etc... report.manufacturerData = byteArray
                unsigned char *allocdata = g_malloc(actualLength);
                memcpy(allocdata, byteArray, actualLength);

                if (repeat || existing->manufacturer_data_length != actualLength)
                {
                    g_print("ManufData has changed       ");
                    send_to_mqtt_array(address, "manufacturerdata", allocdata, actualLength);
                    existing->manufacturer_data_length = actualLength;
                }

                g_variant_unref(s_value);
            }
        }
        else
        {
            const char *type = g_variant_get_type_string(prop_val);
            g_print("ERROR Unknown property: %s %s\n", property_name, type);
        }

        //g_print("un_ref prop_val\n");
        g_variant_unref(prop_val);
    }

    if (allocated_address != NULL)
        g_free(allocated_address);
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

    //g_print("device appeared\n");

    g_variant_get(parameters, "(&oa{sa{sv}})", &object, &interfaces);

    while (g_variant_iter_next(interfaces, "{&s@a{sv}}", &interface_name, &properties))
    {
        if (g_strstr_len(g_ascii_strdown(interface_name, -1), -1, "device"))
        {
            // Report device immediately
            report_device_to_MQTT(properties, NULL, FALSE);
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

    while (g_variant_iter_next(interfaces, "s", &interface_name))
    {
        if (g_strstr_len(g_ascii_strdown(interface_name, -1), -1, "device"))
        {
            char address[BT_ADDRESS_STRING_SIZE];
            if (get_address_from_path(address, BT_ADDRESS_STRING_SIZE, object))
            {
                g_print("Device %s removed\n", address);
                send_to_mqtt_single_value(address, "rssi", 0);
            }
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
    (void)path;      // "/org/bluez/hci0/dev_63_FE_94_98_91_D6"  Bluetooth device path
    (void)interface; // "org.freedesktop.DBus.Properties" ... not useful
    (void)userdata;

    GVariant *p = NULL;
    GVariantIter *unknown = NULL;
    const char *iface; // org.bluez.Device1

    //pretty_print("adapter_changed params = ", params);

    const gchar *signature = g_variant_get_type_string(params);
    if (strcmp(signature, "(sa{sv}as)") != 0)
    {
        g_print("Invalid signature for %s: %s != %s", signal, signature, "(sa{sv}as)");
        return;
    }

    //g_variant_get(params, "(&sa{sv}as)", &iface, &properties, &unknown);
    // Tuple with a string (interface name), a dictionary, and then an array of strings
    g_variant_get(params, "(&s@a{sv}as)", &iface, &p, &unknown);

    //pretty_print("adapter_changed p = ", p);
    //g_print("Interface %s\n", iface);
    //g_print("Path %s\n", path);

    char address[BT_ADDRESS_STRING_SIZE];
    if (get_address_from_path(address, BT_ADDRESS_STRING_SIZE, path))
    {
        report_device_to_MQTT(p, address, TRUE);
    }

    g_variant_unref(p);

    return;
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
    if (error != NULL)
        return 1;

    g_variant_unref(result);
    return 0;
}

static int bluez_set_discovery_filter()
{
    int rc;
    GVariantBuilder *b = g_variant_builder_new(G_VARIANT_TYPE_VARDICT);
    g_variant_builder_add(b, "{sv}", "Transport", g_variant_new_string("le")); // or "auto"
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
    if (rc)
    {
        g_print("Not able to set discovery filter\n");
        return 1;
    }

    rc = bluez_adapter_call_method("GetDiscoveryFilters",
                                   NULL,
                                   bluez_get_discovery_filter_cb);
    if (rc)
    {
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

    //        g_print("List devices\n");

    result = g_dbus_connection_call_finish(con, res, NULL);
    if (result == NULL)
        g_print("Unable to get result for GetManagedObjects\n");

    /* Parse the result */
    if (result)
    {
        GVariant *child = g_variant_get_child_value(result, 0);
        g_variant_iter_init(&i, child);

        while (g_variant_iter_next(&i, "{&o@a{sa{sv}}}", &object_path, &ifaces_and_properties))
        {
            const gchar *interface_name;
            GVariant *properties;
            GVariantIter ii;
            g_variant_iter_init(&ii, ifaces_and_properties);
            while (g_variant_iter_next(&ii, "{&s@a{sv}}", &interface_name, &properties))
            {
                if (g_strstr_len(g_ascii_strdown(interface_name, -1), -1, "device"))
                {
                    report_device_to_MQTT(properties, NULL, FALSE);
                }
                g_variant_unref(properties);
            }
            g_variant_unref(ifaces_and_properties);
        }
        g_variant_unref(child);
        g_variant_unref(result);
    }
}

int get_managed_objects(void *parameters)
{
    GMainLoop *loop = (GMainLoop *)parameters;

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

int clear_cache(void *parameters)
{
    g_print("Clearing cache\n");
    //GMainLoop * loop = (GMainLoop*) parameters;
    (void)parameters; // not used
    repeat = TRUE;
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

    if (argc < 2)
    {
        g_print("scan <mqtt server> [port:1883]");
        return -1;
    }

    char *mqtt_addr = argv[1];
    char *mqtt_port = argc > 1 ? argv[2] : "1883";

    get_mac_address();

    con = g_bus_get_sync(G_BUS_TYPE_SYSTEM, NULL, NULL);
    if (con == NULL)
    {
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
    if (rc)
    {
        g_print("Not able to enable the adapter\n");
        goto fail;
    }

    rc = bluez_set_discovery_filter(argv);
    if (rc)
    {
        g_print("Not able to set discovery filter\n");
        goto fail;
    }

    rc = bluez_adapter_call_method("StartDiscovery", NULL, NULL);
    if (rc)
    {
        g_print("Not able to scan for new devices\n");
        goto fail;
    }
    g_print("Started discovery\n");

    // Every 5 min send any changes to static information
    g_timeout_add_seconds(5*60, get_managed_objects, loop);

    // Every 1 hour, repeat all the data (in case MQTT database is lost)
    g_timeout_add_seconds(60 * 60, clear_cache, loop);

    prepare_mqtt(mqtt_addr, mqtt_port);

    g_print("Start main loop\n");

    g_main_loop_run(loop);

    g_print("END OF MAIN LOOP RUN\n");

    if (argc > 3)
    {
        rc = bluez_adapter_call_method("SetDiscoveryFilter", NULL, NULL);
        if (rc)
            g_print("Not able to remove discovery filter\n");
    }

    rc = bluez_adapter_call_method("StopDiscovery", NULL, NULL);
    if (rc)
        g_print("Not able to stop scanning\n");
    g_usleep(100);

    rc = bluez_adapter_set_property("Powered", g_variant_new("b", FALSE));
    if (rc)
        g_print("Not able to disable the adapter\n");
fail:
    g_dbus_connection_signal_unsubscribe(con, prop_changed);
    g_dbus_connection_signal_unsubscribe(con, iface_added);
    g_dbus_connection_signal_unsubscribe(con, iface_removed);
    g_object_unref(con);
    return 0;
}
