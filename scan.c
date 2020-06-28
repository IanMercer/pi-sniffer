/*
 * bluez sniffer
 *  Sends bluetooth information to MQTT with a separate topic per device and parameter
 *  BLE/<device mac>/parameter
 *
 *  Applies a simple kalman filter to RSSI to smooth it out somewhat
 *  Sends RSSI only when it changes enough but also regularly a keep-alive
 *
 *  gcc `pkg-config --cflags glib-2.0 gio-2.0` -Wall -Wextra -o ./bin/bluez_adapter_filter ./bluez_adapter_filter.c `pkg-config --libs glib-2.0 gio-2.0`

 * ISSUE: Still getting after-the-fact bogus values from BlueZ

 */
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

char* client_id = NULL;

#include "utility.c"
#include "mqtt_send.c"
#include "kalman.c"

// delta distance x delta time threshold for sending an update
// e.g. 5m after 2s or 1m after 10s
// prevents swamping MQTT with very small changes
// but also guarantees an occasional update to indicate still alive

#define THRESHOLD 10.0

// Handle Ctrl-c
void     int_handler(int);

static int id_gen = 0;

/*
   Structure for reporting to MQTT

   TODO: Add time_t to this struct, keep them around not clearing hash an report every five minutes
   on all still connected devices incase MQTT isn't saving values.
*/
struct Device
{
    int id;
    char *name;
    char *alias;
    char *addressType;
    int32_t manufacturer;
    bool paired;
    bool connected;
    bool trusted;
    uint32_t deviceclass; // https://www.bluetooth.com/specifications/assigned-numbers/Baseband/
    uint16_t appearance;
    int manufacturer_data_hash;
    int uuids_length;              // should use a Hash instead
    time_t last_rssi;              // last time an RSSI was received. If gap > 0.5 hour, ignore initial point (dead letter post)
    struct Kalman kalman;
    time_t last_sent;
    float last_value;
    struct Kalman kalman_interval; // Tracks time between RSSI events in order to detect large gaps
    time_t earliest;               // Earliest time seen, used to calculate overlap
    time_t latest;                 // Latest time seen, used to calculate overlap
    int count;                     // Count how many times seen (ignore 1 offs)
    int column;                    // Allocated column in a non-overlapping range structure
};


/*
  Do these two devices overlap in time? If so they cannot be the same device
*/
bool Overlaps (struct Device* a, struct Device* b) {
  if (a->earliest > b->latest) return FALSE; // a is entirely after b
  if (b->earliest > a->latest) return FALSE; // b is entirely after a
  return TRUE; // must overlap if not entirely after or before
}

bool made_changes = FALSE;

#define MAX_TIME_AGO_MINUTES 5

// Updated before any function that needs to calculate relative time
time_t now;
// Updated for each range being scanned
int range_to_scan = 0;

/*
    Ignore devices that are too far away, don't have enough points, haven't been seen in a while ...
*/
bool ignore (struct Device* device) {

  if (device->last_value > range_to_scan) return TRUE;
  if (device->count < 2) return TRUE;

  // ignore devices that we haven't seen from in a while (5 min)
  double delta_time = difftime(now, device->latest);
  if (delta_time > MAX_TIME_AGO_MINUTES * 60) return TRUE;

  return FALSE;
}

void examine_overlap_inner (gpointer key, gpointer value, gpointer user_data)
{
  (void)key;
  struct Device* a = (struct Device*) value;
  struct Device* b = (struct Device*) user_data;
  if (a->id >= b->id) return;
  if (a->column != b->column) return; // Already on separate columns

  if (ignore(a)) return;
  if (ignore(b)) return;

  bool overlaps = Overlaps(a, b);
  int min = a->earliest;
  if (b->earliest < min) min = b->earliest;

  //g_print("(%i,%i = %i  (%li-%li), (%li-%li) ) ", a->id, b->id, overlaps, a->earliest - min, a->latest - min, b->earliest - min, b->latest - min );

  if (overlaps) {
    b->column++;
    made_changes = TRUE;
  }
}

void examine_overlap_outer (gpointer key, gpointer value, gpointer user_data)
{
  (void)key;
  GHashTable* table = (GHashTable*)user_data;
  made_changes = TRUE;
  while(made_changes) {
    made_changes = FALSE;
    g_hash_table_foreach(table, examine_overlap_inner, value);
  }
}

void set_column_to_zero (gpointer key, gpointer value, gpointer user_data)
{
  (void)key;
  (void)user_data;
  struct Device* a = (struct Device*) value;
  a->column = 0;
}

int max_column = -1;

void find_max_column (gpointer key, gpointer value, gpointer user_data)
{
  (void)key;
  (void)user_data;
  struct Device* a = (struct Device*) value;
  if (a->column > max_column) max_column = a->column;
}

/*
  Calculates the minimum number of devices present
*/
int min_devices_present(GHashTable* table, int range) {
  range_to_scan = range;
  g_hash_table_foreach(table, examine_overlap_outer, table);
  max_column = -1;
  g_hash_table_foreach(table, find_max_column, table);
  // g_print("\nMax column = %i\n", max_column);
  return max_column+1;
}


#define N_RANGES 10
static int ranges[N_RANGES] = {1, 2, 5, 10, 15, 20, 25, 30, 35, 100};
static int reported_counts[N_RANGES] = {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1};

/*
    Find best packing of device time ranges into columns
    //Set every device to column zero
    While there is any overlap, find the overlapping pair, move the second one to the next column
*/

void report_devices_count(GHashTable* table) {
    int max = g_hash_table_size(table);

    // Initialize time and columns
    time(&now);
    g_hash_table_foreach(table, set_column_to_zero, table);

    // Calculate for each range how many devices are inside that range at the moment
    // Ignoring any that are potential MAC address randomizations of others
    for (int i = 0; i < N_RANGES; i++) {
        int range = ranges[i];

        // Do the packing of devices into columns for all devices under this range
        int min = min_devices_present(table, range);

        if (reported_counts[i] != min) {
          g_print("Devices present at range %im %i-%i    ", range, min, max);
          reported_counts[i] = min;
          char srange[3];
          snprintf(srange, sizeof(srange), "%i", range);
          send_to_mqtt_single_value("summary", srange, min);

          // legacy
          if (range == 30) {
            send_to_mqtt_single_value("summary", "min_devices", min);
          }
        }
    }
}


// The mac address of the wlan0 interface (every Pi has one so fairly safe to assume wlan0 exists)

static char access_point_address[6];
char controller_mac_address[13];
char hostbuffer[256];

/* SEND TO MQTT WITH ACCESS POINT MAC ADDRESS AND TIME STAMP */


GHashTable *hash = NULL;

// Free an allocated device

void device_report_free(void *object)
{
    struct Device *val = (struct Device *)object;
    g_print("FREE name '%s'\n", val->name);
    /* Free any members you need to */
    g_free(val->name);
    g_print("FREE alias '%s'\n", val->alias);
    g_free(val->alias);
    g_print("FREE value\n");
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


/*
    REPORT DEVICE TO MQTT

    address if known, if not have to find it in the dictionary of passed properties
    appeared = we know this is fresh data so send RSSI and TxPower with timestamp
*/

static bool repeat = FALSE; // runs the get managed objects call just once, 15s after startup

static void report_device_disconnected_to_MQTT(char* address)
{
    if (hash == NULL) return;

    if (!g_hash_table_contains(hash, address))
        return;

    //struct Device * existing = (struct Device *)g_hash_table_lookup(hash, address);

    // Reinitialize it so next value is swallowed??
    //kalman_initialize(&existing->kalman);

    // TODO: A stable iBeacon gets disconnect events all the time - this is not useful data

    // Send a marker value to say "GONE"
    //int fake_rssi = 40 + ((float)(address[5] & 0xF) / 10.0) + ((float)(access_point_address[5] & 0x3) / 2.0);

    //send_to_mqtt_single_value(address, "rssi", -fake_rssi);

    // DON'T REMOVE VALUE FROM HASH TABLE - We get disconnected messages and then immediately reconnects
    // Remove value from hash table
    // g_hash_table_remove(hash, address);
}



static void report_device_to_MQTT(GVariant *properties, char *address, bool changed)
{
    //       g_print("report_device_to_MQTT(%s)\n", address);


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

    struct Device *existing;

    if (!g_hash_table_contains(hash, address))
    {
        existing = g_malloc0(sizeof(struct Device));
        g_hash_table_insert(hash, strdup(address), existing);

        // dummy struct filled with unmatched values
        existing->id = id_gen++;   // unique ID for each
        existing->name = NULL;
        existing->alias = NULL;
        existing->addressType = NULL;
        existing->connected = FALSE;
        existing->trusted = FALSE;
        existing->paired = FALSE;
        existing->deviceclass = 0;
        existing->manufacturer = 0;
        existing->manufacturer_data_hash = 0;
        existing->appearance = 0;
        existing->uuids_length = 0;
        time(&existing->earliest);
        existing->column = 0;
        existing->count = 0;

        // RSSI values are stored with kalman filtering
        kalman_initialize(&existing->kalman);
        existing->last_value = 0;
        time(&existing->last_sent);
        time(&existing->last_rssi);

        existing->last_sent = existing->last_sent - 10000; // bump back in time so first RSSI goes straight through
        existing->last_rssi = existing->last_rssi - 10000; // bump back in time so first RSSI goes straight through

        kalman_initialize(&existing->kalman_interval);

        g_print("Added device %i. %s\n", existing->id, address);
    }
    else
    {
        // get value from hashtable
        existing = (struct Device *)g_hash_table_lookup(hash, address);
        //g_print("Existing value %s\n", existing->address);
        g_print("Existing device %i. %s '%s'\n", existing->id, address, existing->name);
    }

    // Mark the most recent time for this device
    time(&existing->latest);
    existing->count++;

    // If after examining every key/value pair, distance has been set then we will send it
    bool send_distance = FALSE;

    const gchar *property_name;
    GVariantIter i;
    GVariant *prop_val;
    g_variant_iter_init(&i, properties); // no need to free this
    while (g_variant_iter_next(&i, "{&sv}", &property_name, &prop_val))
    {
        //bool isStringValue = g_variant_type_equal(g_variant_get_type(prop_val), G_VARIANT_TYPE_STRING);

        if (strcmp(property_name, "Address") == 0)
        {
            // do nothing, already picked off
        }
        else if (strcmp(property_name, "Name") == 0)
        {
            char *name = g_variant_dup_string(prop_val, NULL);

            // Trim whitespace (Bad Tracker device keeps flipping name)
            trim(name);

            if (g_strcmp0(existing->name, name) != 0)
            {
                g_print("  %s Name has changed '%s' -> '%s'  ", address, existing->name, name);
                send_to_mqtt_single(address, "name", name);
            }
            else {
                g_print(  "Name unchanged '%s'\n", name);
            }
            if (existing->name != NULL)
              g_free(existing->name);
            existing->name = name;
        }
        else if (strcmp(property_name, "Alias") == 0)
        {
            char *alias = g_variant_dup_string(prop_val, NULL);

            trim(alias);

            if (g_strcmp0(existing->alias, alias) != 0)
            {
                g_print("  %s Alias has changed '%s' -> '%s'  ", address, existing->alias, alias);
                send_to_mqtt_single(address, "alias", alias);
            }
            else {
                g_print(  "Alias unchanged '%s'\n", alias);
            }
            if (existing->alias != NULL)
                g_free(existing->alias);
            existing->alias = alias;
        }
        else if (strcmp(property_name, "AddressType") == 0)
        {
            char *addressType = g_variant_dup_string(prop_val, NULL);

            // Compare values and send
            if (g_strcmp0(existing->addressType, addressType) != 0)
            {
                g_print("  %s Type has changed '%s' -> '%s'  ", address, existing->addressType, addressType);
                send_to_mqtt_single(address, "type", addressType);
                if (g_strcmp0("public", addressType) == 0) {
                  existing->addressType = "public";
                } else {
                  existing->addressType = "random";
                }
            }
            else {
                g_print(  "Address type unchanged\n");
            }
            g_free(addressType);
        }
        else if (strcmp(property_name, "RSSI") == 0 && (changed == FALSE)) {
            int16_t rssi = g_variant_get_int16(prop_val);
            g_print("  %s RSSI repeat %i changed=%i\n", address, rssi, changed);
        }
        else if (strcmp(property_name, "RSSI") == 0)
        {
            int16_t rssi = g_variant_get_int16(prop_val);
            //send_to_mqtt_single_value(address, "rssi", rssi);

            g_print("  %s RSSI %i changed=%i\n", address, rssi, changed);

            time_t now;
            time(&now);

            // track gap between RSSI received events
            //double delta_time_received = difftime(now, existing->last_rssi);
            time(&existing->last_rssi);

            double delta_time_sent = difftime(now, existing->last_sent);

            // Smoothed delta time, interval between RSSI events
            //float current_time_estimate = (&existing->kalman_interval)->current_estimate;

            //bool tooLate = (current_time_estimate != -999) && (delta_time_received > 2.0 * current_time_estimate);

            //float average_delta_time = kalman_update(&existing->kalman_interval, (float)delta_time_sent);

            double N = 3.0;   // 2.0 to 4.0 depending on environment

            bool isBarn = access_point_address[5] == 0xa3;
            if (isBarn) {
              N = 2.8;        // no walls (2.5 gave 15m distances for 5m real
            }

            double OneMeterRSSI = -50.0; // Measured power

            //   1m =>  rssi == -50.0
            //  10m =>  30+50 == -rssi   -80dbM
            //  RSSI = -50   distance = 1.0m
            //  RSSI = -100  distance = 46m         which seems about right for outdoor, maybe N is less indoor?

            double exponent = ((OneMeterRSSI - (double)rssi) / (10.0 * N));

            double distance = pow(10.0, exponent);

            float averaged = kalman_update(&existing->kalman, distance);


            // 100s with RSSI change of 1 triggers send
            // 10s with RSSI change of 10 triggers send
            double delta_v = fabs(existing->last_value - averaged);
            double score =  delta_v * (delta_time_sent + 1.0);

 //           if (tooLate) {
 //               // Significant gap since last RSSI, this may be the 'dead letter' from controller
 //               g_print("Ignoring dead letter RSSI %s %.0fs > 2.0 * %.0fs\n", address, delta_time_received, average_delta_time);
 //               //send_to_mqtt_single_float(address, "rssi", -109.0);  // debug, dummy value
 //           }
 //           else
            if (changed && (score > THRESHOLD))
            {
                // ignore RSSI values that are impossibly good (<10 when normal range is -20 to -120)
	        //g_print("  %s Will send rssi=%i dist=%.1fm, delta v=%.1fm t=%.0fs score=%.0f\n", address, rssi, averaged, delta_v, delta_time_sent, score);
                existing->last_value = averaged;
                send_distance = TRUE;
            }
            else if (changed) {
               g_print("  %s Skip rssi=%i dist=%.1fm, delta v:%.1fm t:%.0fs score %.0f\n", address, rssi, averaged, delta_v, delta_time_sent, score);
            }
            else {
               g_print("  %s Ignore rssi=%i dist=%.1fm, delta v:%.1fm t:%.0fs\n", address, rssi, averaged, delta_v, delta_time_sent);
            }
        }
        else if (strcmp(property_name, "TxPower") == 0)
        {
            if (changed)
            {
                g_print("  %s TXPOWER has changed ", address);
                int16_t p = g_variant_get_int16(prop_val);
                send_to_mqtt_single_value(address, "txpower", p);
            }
        }

        else if (strcmp(property_name, "Paired") == 0)
        {
            bool paired = g_variant_get_boolean(prop_val);
            if (existing->paired != paired)
            {
                g_print("  %s Paired has changed        ", address);
                send_to_mqtt_single_value(address, "paired", paired ? 1 : 0);
                existing->paired = paired;
            }
        }
        else if (strcmp(property_name, "Connected") == 0)
        {
            bool connected = g_variant_get_boolean(prop_val);
            if (existing->connected != connected)
            {
                g_print("  %s Connected has changed     ", address);
                send_to_mqtt_single_value(address, "connected", connected ? 1 : 0);
                existing->connected = connected;
            }
        }
        else if (strcmp(property_name, "Trusted") == 0)
        {
            bool trusted = g_variant_get_boolean(prop_val);
            if (existing->trusted != trusted)
            {
                g_print("  %s Trusted has changed       ", address);
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
                g_print("  %s UUIDs has changed       ", address);
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
                g_print("  %s Appearance has changed ", address);
                send_to_mqtt_single_value(address, "appearance", appearance);
                existing->appearance = appearance;
            }
        }
        else if (strcmp(property_name, "ServiceData") == 0)
        {
            // A a{sv} value

            // {'000080e7-0000-1000-8000-00805f9b34fb': <[byte 0xb0, 0x23, 0x25, 0xcb, ...]>}
            pretty_print2("  ServiceData ", prop_val, TRUE); // a{sv}
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

            GVariant *s_value;
            GVariantIter i;
            uint16_t manufacturer;

            g_variant_iter_init(&i, prop_val);
            while (g_variant_iter_next(&i, "{qv}", &manufacturer, &s_value))
            { // Just one

                if (existing->manufacturer != manufacturer)
                {
                    g_print("  %s Manufacturer has changed ", address);
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

                uint8_t hash = 0;
                for (int i = 0; i < actualLength; i++) {
                  hash += allocdata[i];
                }

                if (existing->manufacturer_data_hash != hash)
                {
                    pretty_print2("  ManufacturerData", prop_val, TRUE);  // a{qv}
                    g_print("  %s ManufData has changed       ", address);
                    send_to_mqtt_array(address, "manufacturerdata", allocdata, actualLength);
                    existing->manufacturer_data_hash = hash;

                    if (existing->last_value > 0) {
                      // And repeat the RSSI value every time someone locks or unlocks their phone
                      // Even if the change notification did not include an updated RSSI
                      //g_print("  %s Will resend distance\n", address);
                      send_distance = TRUE;
                    }
                }

		if (manufacturer == 0x004c) {
                  uint8_t apple_device_type = allocdata[00];
                  if (apple_device_type == 0x02) {
                    g_print("  Beacon \n");
                    if (existing->name == NULL) existing->name = strdup("Beacon");
                  }
                  else if (apple_device_type == 0x03) g_print("  Airprint \n");
                  else if (apple_device_type == 0x05) g_print("  Airdrop \n");
                  else if (apple_device_type == 0x07) {
                     g_print("  Airpods \n");
                     if (existing->name == NULL) existing->name = strdup("Airpods");
                  }
                  else if (apple_device_type == 0x08) g_print("  Siri \n");
                  else if (apple_device_type == 0x09) g_print("  Airplay \n");
                  else if (apple_device_type == 0x0a) {
                     g_print("  Apple 0a \n");
                     if (existing->name == NULL) existing->name = strdup("Apple 0a");
                  }
                  else if (apple_device_type == 0x0b) {
                     g_print("  Watch_c \n");
                    if (existing->name == NULL) existing->name = strdup("Apple iWatch");
                  }
                  else if (apple_device_type == 0x0c) g_print("  Handoff \n");
                  else if (apple_device_type == 0x0d) g_print("  WifiSet \n");
                  else if (apple_device_type == 0x0e) g_print("  Hotspot \n");
                  else if (apple_device_type == 0x0f) g_print("  WifiJoin \n");
                  else if (apple_device_type == 0x10) {
                    g_print("  Nearby ");

                    if (existing->name == NULL) { existing->name = strdup("Apple iPhone"); g_print("*SET NAME*"); }

                    uint8_t device_status = allocdata[02];
                    if (device_status == 0x57) g_print(" Lock screen (57) ");
                    else if (device_status == 0x47) g_print(" Lock screen (47) ");
                    else if (device_status == 0x1b) g_print(" Home screen (1b) ");
                    else if (device_status == 0x1c) g_print(" Home screen (1c) ");
                    else if (device_status == 0x50) g_print(" Home screen (50) ");
                    else if (device_status == 0x4e) g_print(" Outgoing call (4e) ");
                    else if (device_status == 0x5e) g_print(" Incoming call (5e) ");
                    else if (device_status == 0x01) g_print(" Off (01) ");
                    else if (device_status == 0x03) g_print(" Off (03) ");
                    else if (device_status == 0x09) g_print(" Off (09) ");
                    else if (device_status == 0x0a) g_print(" Off (0a) ");
                    else if (device_status == 0x13) g_print(" Off (13) ");
                    else if (device_status == 0x18) g_print(" Off (18) ");
                    else if (device_status == 0x1a) g_print(" Off (1a) ");
                    else if (device_status < 0x20) g_print(" Off %.2x", device_status); else g_print(" On %.2x ", device_status);

                    if (allocdata[03] == 0x18) g_print(" Macbook off "); else
                    if (allocdata[03] == 0x1c) g_print(" Macbook on "); else
                    if (allocdata[03] == 0x1e) g_print(" iPhone (iOS 13 On) "); else
                    if (allocdata[03] == 0x1a) g_print(" iWatch (iOS 13 Off) "); else
                    if (allocdata[03] == 0x00) g_print(" TBD "); else
                      g_print (" device type %.2x", allocdata[03]);  // 1e = iPhone, 1a = iWatch

                    g_print("\n");
                  } else {
                    g_print("Did not recognize apple device type %.2x", apple_device_type);
                  }
                } else if (manufacturer == 0x0310) {
                    if (existing->name == NULL) existing->name = strdup("SGL Italia S.r.l.");
                } else {
                  // https://www.bluetooth.com/specifications/assigned-numbers/16-bit-uuids-for-members/
                  g_print("  Did not recognize manufacturer 0x%.4x\n", manufacturer);
                  if (existing->name == NULL) existing->name = strdup("Not an Apple!");
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

    if (send_distance) {
      g_print("  **** Send distance %6.3f                        ", existing->last_value);
      send_to_mqtt_single_float(address, "distance", existing->last_value);
      time(&existing->last_sent);
    }


    report_devices_count(hash);

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
            // Report device immediately, including RSSI
            report_device_to_MQTT(properties, NULL, TRUE);
        }
        g_variant_unref(properties);
    }

    // Nope ... g_variant_unref(object);
    // Nope ... g_variant_unref(interfaces);

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
                report_device_disconnected_to_MQTT(address);
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
    (void)sender;    // "1.4120"
    (void)path;      // "/org/bluez/hci0/dev_63_FE_94_98_91_D6"  Bluetooth device path
    (void)interface; // "org.freedesktop.DBus.Properties" ... not useful
    (void)userdata;

    GVariant *p = NULL;
    GVariantIter *unknown = NULL;
    const char *iface; // org.bluez.Device1  OR  org.bluez.Adapter1

    // ('org.bluez.Adapter1', {'Discovering': <true>}, [])
    // or a device ... handled by address
    // pretty_print("adapter_changed params = ", params);

    const gchar *signature = g_variant_get_type_string(params);
    if (strcmp(signature, "(sa{sv}as)") != 0)
    {
        g_print("Invalid signature for %s: %s != %s", signal, signature, "(sa{sv}as)");
        return;
    }

    // Tuple with a string (interface name), a dictionary, and then an array of strings
    g_variant_get(params, "(&s@a{sv}as)", &iface, &p, &unknown);

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
    GVariant *gvv = g_variant_new("(ssv)", "org.bluez.Adapter1", prop, value);

    result = g_dbus_connection_call_sync(con,
                                         "org.bluez",
                                         "/org/bluez/hci0",
                                         "org.freedesktop.DBus.Properties",
                                         "Set",
                                         gvv,
                                         NULL,
                                         G_DBUS_CALL_FLAGS_NONE,
                                         -1,
                                         NULL,
                                         &error);
    if (result != NULL)
      g_variant_unref(result);

    // not needed: g_variant_unref(gvv);

    if (error != NULL)
        return 1;

    return 0;
}

static int bluez_set_discovery_filter()
{
    int rc;
    GVariantBuilder *b = g_variant_builder_new(G_VARIANT_TYPE_VARDICT);
    g_variant_builder_add(b, "{sv}", "Transport", g_variant_new_string("le")); // or "auto"
    //g_variant_builder_add(b, "{sv}", "RSSI", g_variant_new_int16(-150));
    g_variant_builder_add(b, "{sv}", "DuplicateData", g_variant_new_boolean(TRUE));
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

    // no need to ... g_variant_unref(device_dict);

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


// Every 10s we need to let MQTT send and receive messages
int mqtt_refresh(void *parameters)
{
    (void)parameters;
    //GMainLoop *loop = (GMainLoop *)parameters;
    // Send any MQTT messages
    mqtt_sync(&mqtt);
    return TRUE;
}


int get_managed_objects(void *parameters)
{
    GMainLoop *loop = (GMainLoop *)parameters;

    if (!repeat) {
      repeat = TRUE;

      g_print("Get managed objects\n");

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

    }
    return TRUE;
}


// Remove old items from cache
gboolean remove_func (gpointer key, void *value, gpointer user_data) {
  (void)user_data;
  struct Device *existing = (struct Device *)value;

  time_t now;
  time(&now);

  double delta_time_sent = difftime(now, existing->last_sent);

  gboolean remove = delta_time_sent > 15 * 60;  // 15 min

  if (remove) {
    g_print("  Cache remove %s %.1fs %.1fm\n", (char*)key, delta_time_sent, existing->kalman.current_estimate);
  }

  return remove;  // 15 min of no activity = remove from cache
}


int clear_cache(void *parameters)
{
    if (hash == NULL) return TRUE;

//    g_print("Clearing cache\n");
    //GMainLoop * loop = (GMainLoop*) parameters;
    (void)parameters; // not used

    // Remove any item in cache that hasn't been seen for a long time
    gpointer user_data = NULL;
    g_hash_table_foreach_remove (hash, &remove_func, user_data);

    // And report the updated count of devices present
    report_devices_count(hash);

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

guint prop_changed;
guint iface_added;
guint iface_removed;

int main(int argc, char **argv)
{
    GMainLoop *loop;
    int rc;
    //guint getmanagedobjects;

    signal(SIGINT, int_handler);
    signal(SIGTERM, int_handler);

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

    rc = bluez_set_discovery_filter();
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

    prepare_mqtt(mqtt_addr, mqtt_port, client_id);

    // Once after startup send any changes to static information
    // Added back because I don't think this is the issue
    g_timeout_add_seconds(15, get_managed_objects, loop);

    // MQTT send
    g_timeout_add_seconds(5, mqtt_refresh, loop);

    // Every 30s look see if any records have expired and should be removed
    g_timeout_add_seconds(30, clear_cache, loop);


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

void int_handler(int dummy) {
    (void) dummy;

    //keepRunning = 0;
    g_dbus_connection_signal_unsubscribe(con, prop_changed);
    g_dbus_connection_signal_unsubscribe(con, iface_added);
    g_dbus_connection_signal_unsubscribe(con, iface_removed);
    g_object_unref(con);

    if (sockfd != -1)
        close(sockfd);

    if (hash != NULL)
        g_hash_table_destroy (hash);

    g_print("Clean exit\n");

    exit(0);
}
