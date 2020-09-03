/*
 *  CROWD ALERT
 *
 *  Sends bluetooth information to MQTT with a separate topic per device and parameter
 *  BLE/<device mac>/parameter
 *
 *  Sends calculated person count over UDP to port 7778 as a broadcast
 *
 *  Applies a simple kalman filter to RSSI to smooth it out somewhat
 *  Sends RSSI only when it changes enough but also regularly a keep-alive
 *
 *  See Makefile and Github
 *
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
#include <stdint.h>
#include <sys/types.h>
#include <string.h>
#include <udp.h>
#include <argp.h>

#include "utility.h"
#include "mqtt_send.h"
#include "udp.h"
#include "kalman.h"
#include "device.h"
#include "bluetooth.h"

static struct OverallState state;
// contains ... static struct Device devices[N];

static bool starting = TRUE;

// delta distance x delta time threshold for sending an update
// e.g. 5m after 2s or 1m after 10s
// prevents swamping MQTT with very small changes
// but also guarantees an occasional update to indicate still alive

#define THRESHOLD 10.0

// Handle Ctrl-c
void     int_handler(int);

static int id_gen = 0;
bool logTable = FALSE;  // set to true each time something changes

/*
      Connection to DBUS
*/
GDBusConnection *conn;

/*
  Do these two devices overlap in time? If so they cannot be the same device
*/
bool overlaps (struct Device* a, struct Device* b) {
  if (a->earliest > b->latest) return FALSE; // a is entirely after b
  if (b->earliest > a->latest) return FALSE; // b is entirely after a
  return TRUE; // must overlap if not entirely after or before
}

/*
    Compute the minimum number of devices present by assigning each in a non-overlapping manner to columns
*/
void pack_columns()
{
  // Push every device back to column zero as category may have changed
  for (int i = 0; i < state.n; i++) {
    struct Device* a = &state.devices[i];
    a->column = 0;
  }

  for (int k = 0; k < state.n; k++) {
    bool changed = false;

    for (int i = 0; i < state.n; i++) {
      for (int j = i+1; j < state.n; j++) {
        struct Device* a = &state.devices[i];
        struct Device* b = &state.devices[j];

        if (a->column != b-> column) continue;

        bool over = overlaps(a, b);

        // cannot be the same device if either has a public address (or we don't have an address type yet)
        bool haveDifferentAddressTypes = (a->addressType>0 && b->addressType>0 && a->addressType != b->addressType);

        // cannot be the same if they both have names and the names are different
        bool haveDifferentNames = (strlen(a->name) > 0) && (strlen(b->name) > 0) && (g_strcmp0(a->name, b->name) != 0);

        // cannot be the same if they both have known categories and they are different
        // Used to try to blend unknowns in with knowns but now we get category right 99.9% of the time, no longer necessary
        bool haveDifferentCategories = (a->category != b->category);// && (a->category != CATEGORY_UNKNOWN) && (b->category != CATEGORY_UNKNOWN);

        if (over || haveDifferentAddressTypes || haveDifferentNames || haveDifferentCategories) {
          b->column++;
          changed = true;
          // g_print("Compare %i to %i and bump %4i %s to %i, %i %i %i\n", i, j, b->id, b->mac, b->column, over, haveDifferentAddressTypes, haveDifferentNames);
        }
      }
    }
    if (!changed) break;
  }
}

/*
   Remove a device from array and move all later devices up one spot
*/
void remove_device(int index) {
  for (int i = index; i < state.n-1; i++) {
    state.devices[i] = state.devices[i+1];
    struct Device* dev = &state.devices[i];
    // decrease column count, may create clashes, will fix these up next
    dev->column = dev->column > 0 ? dev->column - 1 : 0;
  }
  state.n--;
  pack_columns();
}


#define MAX_TIME_AGO_COUNTING_MINUTES 5
#define MAX_TIME_AGO_LOGGING_MINUTES 10
#define MAX_TIME_AGO_CACHE 60

// Updated before any function that needs to calculate relative time
time_t now;


// Largest number of devices that can be tracked at once
#define N_COLUMNS 500

struct ColumnInfo {
    time_t latest;        // latest observation in this column
    float distance;       // distance of latest observation in this column
    int8_t category;      // category of device in this column (phone, computer, ...)
    bool isClosest;         // is this device closest to us not some other sensor
};

struct ColumnInfo columns[N_COLUMNS];

/*
    Find latest observation in each column and the distance for that
*/
void find_latest_observations () {
   for (uint i=0; i < N_COLUMNS; i++){
      columns[i].distance = -1.0;
      columns[i].category = CATEGORY_UNKNOWN;
   }
   for (int i = 0; i < state.n; i++) {
      struct Device* a = &state.devices[i];
      int col = a->column;
      if (columns[col].distance < 0.0 || columns[col].latest < a->latest) {
        columns[col].distance = a->distance;
        if (a->category != CATEGORY_UNKNOWN) {
          // a later unknown does not override an actual phone category nor extend it
          // This if is probably not necessary now as overlap tests for this
          columns[col].category = a->category;
          columns[col].latest = a->latest;
          // Do we 'own' this device or does someone else
          columns[col].isClosest = true;
          struct ClosestTo* closest = get_closest(a->id);
          columns[col].isClosest = closest != NULL && closest->access_id == state.local->id;
        }
      }
   }
}


#define N_RANGES 10
static int32_t ranges[N_RANGES] = {1, 2, 5, 10, 15, 20, 25, 30, 35, 100};
static int8_t reported_ranges[N_RANGES] = {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1};
float people_in_view_count = 0.0;
float people_closest_count = 0.0;

/*
    Find best packing of device time ranges into columns
    //Set every device to column zero
    While there is any overlap, find the overlapping pair, move the second one to the next column
*/


void report_devices_count() {
    g_debug("report_devices_count\n");

    if (starting) return;   // not during first 30s startup time

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
    for (int i = 0; i < N_RANGES; i++) {
        int range = ranges[i];

        int min = 0;

        for (int col=0; col < N_COLUMNS; col++)
        {
           if (columns[col].category != CATEGORY_PHONE) continue;   // only counting phones now not beacons, laptops, ...
           if (columns[col].distance < 0.01) continue;   // not allocated

           double delta_time = difftime(now, columns[col].latest);
           if (delta_time > MAX_TIME_AGO_COUNTING_MINUTES * 60) continue;

           if (columns[col].distance < range) min++;
        }

        int just_this_range = min - previous;
        if (reported_ranges[i] != just_this_range) {
          //g_print("Devices present at range %im %i    \n", range, just_this_range);
          reported_ranges[i] = just_this_range;
          made_changes = TRUE;
        }
        previous = min;
    }

    if (made_changes) {
      g_print("Devices by range: ");
      for (int i = 0; i < N_RANGES; i++) {
         g_print(" %i", reported_ranges[i]);
      }
      g_print("  ");
      send_to_mqtt_distances((unsigned char*)reported_ranges, N_RANGES * sizeof(int8_t));
    }

    int range_limit = 5;
    int range = ranges[range_limit];

    // Expected value of number of people here (decays if not detected recently)
    float people_in_view = 0.0;
    float people_closest = 0.0;

    for (int col=0; col < N_COLUMNS; col++)
    {
       if (columns[col].distance >= range) continue;
       if (columns[col].category != CATEGORY_PHONE) continue;   // only counting phones now not beacons, laptops, ...
       if (columns[col].distance < 0.01) continue;   // not allocated

       double delta_time = difftime(now, columns[col].latest);
       if (delta_time > MAX_TIME_AGO_COUNTING_MINUTES * 60) continue;

       double score = 0.55 - atan(delta_time/40.0  - 4.0) / 3.0;
       // A curve that stays a 1.0 for a while and then drops rapidly around 3 minutes out
       if (score > 0.9) score = 1.0;
       if (score < 0.0) score = 0.0;

       // Expected value E[x] = i x p(i) so sum p(i) for each column which is one person
       people_in_view += score;
       if (columns[col].isClosest) people_closest += score;
    }

    if (fabs(people_in_view - people_in_view_count) > 0.01 || fabs(people_closest - people_closest_count) > 0.01) {
      people_closest_count = people_closest;
      people_in_view_count = people_in_view;
      g_print("People count = %.2f (%.2f in range)\n", people_closest, people_in_view);
    }

    double scale_factor = 0.5;  // This adjusts how people map to lights which are on a 0.0-3.0 range
    // 0.0 = green
    // 1.5 = blue
    // 3.0 = red (but starts going red anywhere above 2.0)

    // Always send it in case display gets unplugged and then plugged back in
    if (state.udp_sign_port > 0)
    {
        char msg[4];
        msg[0] = 0;
        // send as ints for ease of consumption on ESP8266
        msg[1] = (int)(people_closest * scale_factor * 10.0);
        msg[2] = (int)(people_in_view * scale_factor * 10.0);
        msg[3] = 0;

        // TODO: Move to JSON for more flexibility sending names too

        udp_send(state.udp_sign_port, msg, sizeof(msg));
        g_print("UDP Sent %i\n", msg[1]);
    }
}


/* SEND TO MQTT WITH ACCESS POINT MAC ADDRESS AND TIME STAMP */



/*
    REPORT DEVICE TO MQTT

    address if known, if not have to find it in the dictionary of passed properties
    appeared = we know this is fresh data so send RSSI and TxPower with timestamp
*/

static void report_device_disconnected_to_MQTT(char* address)
{
    (void)address;
   // Not used
}


/*
   Read byte array from GVariant
*/
unsigned char* read_byte_array(GVariant* s_value, int* actualLength, uint8_t* hash)
{
    unsigned char byteArray[2048];
    int len = 0;

    GVariantIter* iter_array;
    guchar str;

    //g_debug("START read_byte_array\n");

    g_variant_get(s_value, "ay", &iter_array);

    while (g_variant_iter_loop(iter_array, "y", &str))
    {
        byteArray[len++] = str;
    }

    g_variant_iter_free(iter_array);

    unsigned char* allocdata = g_malloc(len);
    memcpy(allocdata, byteArray, len);

    *hash = 0;
    for (int i = 0; i < len; i++) {
      *hash += allocdata[i];
    }

    *actualLength = len;

    //g_debug("END read_byte_array\n");
    return allocdata;
}

void optional(char* name, char* value) {
  if (strlen(name)) return;
  g_strlcpy(name, value, NAME_LENGTH);
}

void soft_set_category(int8_t* category, int8_t category_new)
{
    if (*category == CATEGORY_UNKNOWN) *category = category_new;
}

/*
     handle the manufacturer data
*/
void handle_manufacturer(struct Device * existing, uint16_t manufacturer, unsigned char* allocdata)
{
    //g_debug("START handle_manufacturer\n");
    if (manufacturer == 0x004c) {   // Apple
        uint8_t apple_device_type = allocdata[00];
        if (apple_device_type == 0x02) {
          optional(existing->alias, "Beacon");
          g_print("  Beacon\n") ;
          existing->category = CATEGORY_BEACON;

          // Aprilbeacon temperature sensor
          if (strncmp(existing->name, "abtemp", 6) == 0) {
             uint8_t temperature = allocdata[21];
             send_to_mqtt_single_value(existing->mac, "temperature", temperature);
          }
        }
        else if (apple_device_type == 0x03) g_print("  Airprint \n");
        else if (apple_device_type == 0x05) g_print("  Airdrop \n");
        else if (apple_device_type == 0x07) {
          optional(existing->alias, "Airpods");
          g_print("  Airpods \n");
          existing->category = CATEGORY_HEADPHONES;
        }
        else if (apple_device_type == 0x08) { optional(existing->alias, "Siri"); g_print("  Siri \n"); }
        else if (apple_device_type == 0x09) { optional(existing->alias, "Airplay"); g_print("  Airplay \n"); }
        else if (apple_device_type == 0x0a) { optional(existing->alias, "Apple 0a"); g_print("  Apple 0a \n"); }
        else if (apple_device_type == 0x0b) {
          optional(existing->alias, "iWatch?");
          g_print("  Watch_c \n");
          existing->category = CATEGORY_WATCH;
        }
        else if (apple_device_type == 0x0c) g_print("  Handoff \n");
        else if (apple_device_type == 0x0d) g_print("  WifiSet \n");
        else if (apple_device_type == 0x0e) g_print("  Hotspot \n");
        else if (apple_device_type == 0x0f) g_print("  WifiJoin \n");
        else if (apple_device_type == 0x10) {
          g_print("  Nearby ");
          optional(existing->alias, "iPhone?");
          // Not right, MacBook Pro seems to send this too

          uint8_t device_status = allocdata[02];
          if (device_status & 0x80) g_print("0x80 "); else g_print(" ");
          if (device_status & 0x40) g_print(" ON +"); else g_print("OFF +");

          uint8_t lower_bits = device_status & 0x3f;

          if (lower_bits == 0x07) { g_print(" Lock screen (0x07) "); soft_set_category(&existing->category, CATEGORY_PHONE); }
          else if (lower_bits == 0x17) { g_print(" Lock screen   (0x17) "); soft_set_category(&existing->category, CATEGORY_PHONE); }
          else if (lower_bits == 0x1b) { g_print(" Home screen   (0x1b) "); soft_set_category(&existing->category, CATEGORY_PHONE); }
          else if (lower_bits == 0x1c) { g_print(" Home screen   (0x1c) "); soft_set_category(&existing->category, CATEGORY_PHONE); }
          else if (lower_bits == 0x10) { g_print(" Home screen   (0x10) "); soft_set_category(&existing->category, CATEGORY_PHONE); }
          else if (lower_bits == 0x0e) { g_print(" Outgoing call (0x0e) "); soft_set_category(&existing->category, CATEGORY_PHONE); }
          else if (lower_bits == 0x1e) { g_print(" Incoming call (0x1e) "); soft_set_category(&existing->category, CATEGORY_PHONE); }
          else g_print(" Unknown (0x%.2x) ", lower_bits);

          if (allocdata[03] &0x10) g_print("1"); else g_print("0");
          if (allocdata[03] &0x08) g_print("1"); else g_print("0");
          if (allocdata[03] &0x04) g_print("1"); else g_print("0");
          if (allocdata[03] &0x02) g_print("1"); else g_print("0");
          if (allocdata[03] &0x01) g_print("1"); else g_print("0");

          // These do not seem to be quite right
          if (allocdata[03] == 0x18) g_print(" Apple? (0x18)"); else
          if (allocdata[03] == 0x1c) g_print(" Apple? (0x1c)"); else
          if (allocdata[03] == 0x1e) g_print(" iPhone?  (0x1e)"); else
          if (allocdata[03] == 0x1a) g_print(" iWatch?  (0x1a)"); else
          if (allocdata[03] == 0x00) g_print(" TBD "); else
            g_print (" Device type (%.2x)", allocdata[03]);

          g_print("\n");
        } else {
          g_print("Did not recognize apple device type %.2x", apple_device_type);
        }
    } else if (manufacturer == 0x0087) {
        optional(existing->alias, "Garmin");
        existing->category = CATEGORY_WATCH; // could be fitness tracker
    } else if (manufacturer == 0xb4c1) {
        optional(existing->alias, "Dycoo");   // not on official Bluetooth website
    } else if (manufacturer == 0x0310) {
        optional(existing->alias, "SGL Italia S.r.l.");
        existing->category = CATEGORY_HEADPHONES;
    } else if (manufacturer == 0x00d2) {
        optional(existing->alias, "AbTemp");
        g_print("Ignoring manufdata\n");
    } else {
        // https://www.bluetooth.com/specifications/assigned-numbers/16-bit-uuids-for-members/
        g_print("  Did not recognize manufacturer 0x%.4x\n", manufacturer);
        optional(existing->alias, "Not an Apple");
    }
}


/*
    Report a new or changed device to MQTT endpoint
    NOTE: Free's address when done
*/
static void report_device_internal(GVariant *properties, char *known_address, bool isUpdate)
{
    logTable = TRUE;
    //pretty_print("report_device", properties);
    char address[18];

    if (known_address) {
       g_strlcpy(address, known_address, 18);
    } else {
      // Get address from properies dictionary if not already present
      GVariant *address_from_dict = g_variant_lookup_value(properties, "Address", G_VARIANT_TYPE_STRING);
      if (address_from_dict)
      {
          const char* addr = g_variant_get_string(address_from_dict, NULL);
          g_strlcpy(address, addr, 18);
          g_variant_unref(address_from_dict);
      }
      else {
        g_print("ERROR address is null");
        return;
      }
    }

    struct Device *existing = NULL;

    // Get existing device report
    for (int i = 0; i < state.n; i++) {
      if (memcmp(state.devices[i].mac, address, 18) == 0) {
         existing = &state.devices[i];
      }
    }

    if (existing == NULL)
    {
        if (!isUpdate)
        {
           // DEBUG g_print("Skip %s, bluez get_devices call and not seen yet\n", address);
           return;
        }

        if (state.n == N) {
          g_print("Error, array of devices is full\n");
          return;
        }

        // Grab the next empty item in the array
        existing = &state.devices[state.n++];
        existing->id = id_gen++;                 // unique ID for each
        existing->hidden = false;                // we own this one
        g_strlcpy(existing->mac, address, 18);   // address

        // dummy struct filled with unmatched values
        existing->name[0] = '\0';
        existing->alias[0] = '\0';
        existing->addressType = 0;
        existing->category = CATEGORY_UNKNOWN;
        existing->connected = FALSE;
        existing->trusted = FALSE;
        existing->paired = FALSE;
        existing->deviceclass = 0;
        existing->manufacturer = 0;
        existing->manufacturer_data_hash = 0;
        existing->service_data_hash = 0;
        existing->appearance = 0;
        existing->uuids_length = 0;
        existing->uuid_hash = 0;
        existing->txpower = 12;
        time(&existing->earliest);
        existing->column = 0;
        existing->count = 0;
        existing->try_connect_state = 0;

        // RSSI values are stored with kalman filtering
        kalman_initialize(&existing->kalman);
        existing->distance = 0;
        time(&existing->last_sent);
        time(&existing->last_rssi);

        existing->last_sent = existing->last_sent - 1000;  //1s back so first RSSI goes through
        existing->last_rssi = existing->last_rssi - 1000;

        kalman_initialize(&existing->kalman_interval);

        g_print("Added device %i. %s\n", existing->id, address);
    }
    else
    {
        if (!isUpdate) {
           // from get_all_devices which includes stale data
           g_debug("Repeat device %i. %s '%s' (%s)\n", existing->id, address, existing->name, existing->alias);
        } else {
           g_print("Existing device %i. %s '%s' (%s)\n", existing->id, address, existing->name, existing->alias);
        }
    }

    // Mark the most recent time for this device (but not if it's a get all devices call)
    if (isUpdate)
    {
        time(&existing->latest);
        existing->count++;
    }

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

            if (strncmp(name, existing->name, NAME_LENGTH) != 0)
            {
                g_print("  %s Name has changed '%s' -> '%s'  ", address, existing->name, name);
                send_to_mqtt_single(address, "name", name);
                g_strlcpy(existing->name, name, NAME_LENGTH);
            }
            else {
                // g_print("  Name unchanged '%s'=='%s'\n", name, existing->name);
            }
            if (strcmp(name, "iPhone") == 0) existing->category = CATEGORY_PHONE;
            else if (strcmp(name, "iPad") == 0) existing->category = CATEGORY_TABLET;
            else if (strcmp(name, "MacBook pro") == 0) existing->category = CATEGORY_COMPUTER;
            else if (strcmp(name, "BOOTCAMP") == 0) existing->category = CATEGORY_COMPUTER;
            else if (strcmp(name, "BOOTCAMP2") == 0) existing->category = CATEGORY_COMPUTER;
            // Watches
            else if (strcmp(name, "iWatch") == 0) existing->category = CATEGORY_WATCH;
            else if (strcmp(name, "Apple Watch") == 0) existing->category = CATEGORY_WATCH;
            else if (strcmp(name, "AppleTV") == 0) existing->category = CATEGORY_TV;
            else if (strcmp(name, "Apple TV") == 0) existing->category = CATEGORY_TV;
            else if (strncmp(name, "fenix", 5) == 0) existing->category = CATEGORY_WATCH;
            // Beacons
            else if (strncmp(name, "AprilBeacon", 11) == 0) existing->category = CATEGORY_BEACON;
            else if (strncmp(name, "abtemp", 6) == 0) existing->category = CATEGORY_BEACON;
            else if (strncmp(name, "abeacon", 7) == 0) existing->category = CATEGORY_BEACON;
            else if (strncmp(name, "estimote", 8) == 0) existing->category = CATEGORY_BEACON;
            else if (strncmp(name, "LYWSD03MMC", 10) == 0) existing->category = CATEGORY_BEACON;
            // Headphones
            else if (strncmp(name, "Sesh Evo-LE", 11) == 0) existing->category = CATEGORY_HEADPHONES;  // Skullcandy
            // Cars
            else if (strncmp(name, "Audi", 4) == 0) existing->category = CATEGORY_CAR;
            else if (strncmp(name, "BMW", 3) == 0) existing->category = CATEGORY_CAR;
            else if (strncmp(name, "Subaru", 6) == 0) existing->category = CATEGORY_CAR;
            else if (strncmp(name, "Land Rover", 10) == 0) existing->category = CATEGORY_CAR;
            // TODO: Android device names
        }
        else if (strcmp(property_name, "Alias") == 0)
        {
            char *alias = g_variant_dup_string(prop_val, NULL);
            trim(alias);

            if (strncmp(alias, existing->alias, NAME_LENGTH) != 0)  // has_prefix because we may have truncated it
            {
                g_print("  %s Alias has changed '%s' -> '%s'  \n", address, existing->alias, alias);
                // NOT CURRENTLY USED: send_to_mqtt_single(address, "alias", alias);
                g_strlcpy(existing->alias, alias, NAME_LENGTH);
            }
            else {
                // g_print("  Alias unchanged '%s'=='%s'\n", alias, existing->alias);
            }
        }
        else if (strcmp(property_name, "AddressType") == 0)
        {
            char *addressType = g_variant_dup_string(prop_val, NULL);
            int newAddressType = (g_strcmp0("public", addressType) == 0) ? PUBLIC_ADDRESS_TYPE : RANDOM_ADDRESS_TYPE;

            // Compare values and send
            if (existing->addressType != newAddressType) {
                existing->addressType = newAddressType;
                g_print("  %s Address type has changed -> '%s'  ", address, addressType);
                send_to_mqtt_single(address, "type", addressType);
            }
            else {
                // DEBUG g_print("  Address type unchanged\n");
            }
            g_free(addressType);
        }
        else if (strcmp(property_name, "RSSI") == 0 && (isUpdate == FALSE)) {
            // Ignore this, it isn't helpful
            // int16_t rssi = g_variant_get_int16(prop_val);
            // g_print("  %s RSSI repeat %i\n", address, rssi);
        }
        else if (strcmp(property_name, "RSSI") == 0)
        {
            if (!isUpdate)
            {
               g_print("$$$$$$$$$$$$ RSSI is unreliable for get all devices\n");
               continue;
            }

            int16_t rssi = g_variant_get_int16(prop_val);
            //send_to_mqtt_single_value(address, "rssi", rssi);

            g_print("  %s RSSI %i\n", address, rssi);

            time_t now;
            time(&now);

            // track gap between RSSI received events
            //double delta_time_received = difftime(now, existing->last_rssi);
            time(&existing->last_rssi);


            // Smoothed delta time, interval between RSSI events
            //float current_time_estimate = (&existing->kalman_interval)->current_estimate;

            //bool tooLate = (current_time_estimate != -999) && (delta_time_received > 2.0 * current_time_estimate);

            //float average_delta_time = kalman_update(&existing->kalman_interval, (float)delta_time_sent);

            double exponent = ((state.local->rssi_one_meter - (double)rssi) / (10.0 * state.local->rssi_factor));

            double distance = pow(10.0, exponent);

            // TODO: Different devices have different signal strengths
            // iPad seems to be particulary strong. Need to calibrate this and have
            // a per-device. PowerLevel is supposed to do this but it's not reliably sent.
            if (strcmp(existing->name, "iPad") == 0) {
                distance = distance * 2.1;
            }

            float averaged = kalman_update(&existing->kalman, distance);

            // 10s with distance change of 1m triggers send
            // 1s with distance change of 10m triggers send

            double delta_time_sent = difftime(now, existing->last_sent);
            double delta_v = fabs(existing->distance - averaged);
            double score =  delta_v * delta_time_sent;

            if (score > 10.0 || delta_time_sent > 30) {
	      //g_print("  %s Will send rssi=%i dist=%.1fm, delta v=%.1fm t=%.0fs score=%.0f\n", address, rssi, averaged, delta_v, delta_time_sent, score);
              existing->distance = averaged;
              send_distance = TRUE;
            }
            else {
	      g_print("  %s Skip sending rssi=%i dist=%.1fm, delta v=%.1fm t=%.0fs score=%.0f\n", address, rssi, averaged, delta_v, delta_time_sent, score);
            }
        }
        else if (strcmp(property_name, "TxPower") == 0)
        {
            int16_t p = g_variant_get_int16(prop_val);
            if (p != existing->txpower)
            {
                g_print("  %s TXPOWER has changed %i\n", address, p);
                // NOT CURRENTLY USED ... send_to_mqtt_single_value(address, "txpower", p);
                existing->txpower = p;
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

            GVariantIter* iter_array;
            char *str;

            int uuid_hash = 0;

            g_variant_get(prop_val, "as", &iter_array);

            while (g_variant_iter_loop(iter_array, "s", &str))
            {
                if (strlen(str) < 36) continue;  // invalid GUID

                uuidArray[actualLength++] = strdup(str);

                for (uint32_t i = 0; i < strlen(str); i++) {
                   uuid_hash += (i+1) * str[i];  // sensitive to position in UUID but not to order of UUIDs
                }
            }
            g_variant_iter_free(iter_array);

            if (actualLength > 0) {
                existing->uuid_hash = uuid_hash & 0xffffffff;
            }

            if (existing->uuids_length != actualLength)
            {
                if (actualLength > 0)
                {
                    // Print off the UUIDs here
                    for (int i = 0; i < actualLength; i++) {
                        char* strCopy = strdup(uuidArray[i]);

	                // All BLE UUIDs are of the form: so we only need four hex nibbles: 0000XXXX-0000-1000-8000-00805f9b34fb
	                strCopy[8] = '\0';
	                int64_t ble_uuid = (int)strtol(strCopy, NULL, 16);

                        // EST Unknown(b9401000), Unknown(b9403000), Unknown(b9404000), Unknown(b9406000),

	                // https://www.bluetooth.com/specifications/gatt/characteristics/
	                if (ble_uuid == 0x2a29) g_print("Manufacturer, ");
	                else if (ble_uuid == 0x1800) g_print("Generic access, ");
	                else if (ble_uuid == 0x1801) g_print("Generic attribute, ");
	                else if (ble_uuid == 0x1802) g_print("Immediate Alert, ");
	                else if (ble_uuid == 0x1803) g_print("Link loss, ");
	                else if (ble_uuid == 0x1804) g_print("Tx Power level, ");
	                else if (ble_uuid == 0x1805) g_print("Current time, ");
	                else if (ble_uuid == 0x180f) g_print("Battery, ");
	                else if (ble_uuid == 0x111e) g_print("HandsFree, ");
	                else if (ble_uuid == 0x180a) g_print("Device information, ");
	                else if (ble_uuid == 0x180d) g_print("Heart rate service, ");
	                else if (ble_uuid == 0x2A37) g_print("Heart rate measurement ");
	                else if (ble_uuid == 0x89d3502b) g_print("Apple MS ");
	                else if (ble_uuid == 0x7905f431) g_print("Apple NCS ");
	                else if (ble_uuid == 0xd0611e78) g_print("Apple CS ");
	                else if (ble_uuid == 0x9fa480e0) g_print("Apple XX ");
	                else if (ble_uuid == 0xd0611e78) g_print("Continuity ");
	                else if (ble_uuid == 0xffa0) g_print("Accelerometer ");
	                else if (ble_uuid == 0xffe0) g_print("Temperature ");
                        else g_print("Unknown(%s), ", strCopy);
                        g_free(strCopy);
                    }
                    g_print("\n");
                    char **allocdata = g_malloc(actualLength * sizeof(char *));  // array of pointers to strings
                    memcpy(allocdata, uuidArray, actualLength * sizeof(char *));
                    g_print("  %s UUIDs has changed      ", address);
                    send_to_mqtt_uuids(address, "uuids", allocdata, actualLength);
                    existing->uuids_length = actualLength;
                    g_free(allocdata); // no need to free the actual strings, that happens below
                }
                else
                {
                    // Ignore this, this is after we've collected them and iPhone disconnects and then reconnects
                    //g_print("  %s UUIDs have gone        ", address);
                    //send_to_mqtt_uuids(address, "uuids", NULL, 0);
                    // But don't actually set uuids_length to null as it may come back
                }
            }
            // Free up the individual UUID strings after sending them
            for (int i = 0; i < actualLength; i++)
            {
                g_free(uuidArray[i]);
            }
        }
        else if (strcmp(property_name, "Modalias") == 0)
        {
            // not used
        }
        else if (strcmp(property_name, "Class") == 0)
        {
            // Very few devices send this information (not very useful)
            uint32_t deviceclass = g_variant_get_uint32(prop_val);
            if (existing->deviceclass != deviceclass)
            {
                g_print("  %s Class has changed         ", address);
                send_to_mqtt_single_value(address, "class", deviceclass);
                existing->deviceclass = deviceclass;
            }
        }
        else if (strcmp(property_name, "Icon") == 0)
        {
            char *icon = g_variant_dup_string(prop_val, NULL);
            if (&existing->category == CATEGORY_UNKNOWN) {
              // Should track icon and test against that instead
              g_print("  %s Icon: '%s'\n", address, icon);
            }
            if (strcmp(icon, "computer") == 0) soft_set_category(&existing->category, CATEGORY_COMPUTER);
            else if (strcmp(icon, "phone") == 0) soft_set_category(&existing->category, CATEGORY_PHONE);
            else if (strcmp(icon, "multimedia-player") == 0) soft_set_category(&existing->category, CATEGORY_TV);
            else if (strcmp(icon, "audio-card") == 0) soft_set_category(&existing->category, CATEGORY_CAR);
            g_free(icon);
        }
        else if (strcmp(property_name, "Appearance") == 0)
        { // type 'q' which is uint16
            uint16_t appearance = g_variant_get_uint16(prop_val);
            if (existing->appearance != appearance)
            {
                g_print("  %s Appearance has changed ", address);
                send_to_mqtt_single_value(address, "appearance", appearance);
                existing->appearance = appearance;
            }
        }
        else if (strcmp(property_name, "ServiceData") == 0)
        {
            if (isUpdate == FALSE) {
               continue;    // ignore this, it's stale
            }
            // A a{sv} value
            // {'000080e7-0000-1000-8000-00805f9b34fb':
            //    <[byte 0xb0, 0x23, 0x25, 0xcb, ...]>}
            //    <[byte 0xb0, 0x23, 0x25, 0xcb, 0x66, 0x54, 0xae, 0xab, 0x0a, 0x2b, 0x00, 0x04, 0x33, 0x09, 0xee, 0x60, 0x24, 0x2e, 0x00, 0xf7, 0x07, 0x00, 0x00]>}

            //pretty_print2("  ServiceData ", prop_val, TRUE); // a{sv}

            GVariant *s_value;
            GVariantIter i;
            char* service_guid;

            g_variant_iter_init(&i, prop_val);
            while (g_variant_iter_next(&i, "{sv}", &service_guid, &s_value))
            { // Just one

                uint8_t hash;
                int actualLength;
                unsigned char* allocdata = read_byte_array(s_value, &actualLength, &hash);

                if (existing->service_data_hash != hash)
                {
                    g_print("  ServiceData has changed ");
                    pretty_print2("  ServiceData", prop_val, TRUE);  // a{qv}
                    send_to_mqtt_array(address, "servicedata", allocdata, actualLength);
                    existing->service_data_hash = hash;

                    // temp={p[16] - 10} brightness={p[17]} motioncount={p[19] + p[20] * 256} moving={p[22]}");
                    if (strcmp(service_guid, "000080e7-0000-1000-8000-00805f9b34fb") == 0) {  // Sensoro
                      existing->category = CATEGORY_BEACON;

                      int battery = allocdata[14] + 256 * allocdata[15]; // ???
                      int p14 = allocdata[14];
                      int p15 = allocdata[15];
                      int temp = allocdata[16] - 10;
                      int brightness = allocdata[18];
                      int motionCount = allocdata[19] + allocdata[20] * 256;
                      int moving = allocdata[21];
                      g_print("Sensoro battery=%i, p14=%i, p15=%i, temp=%i, brightness=%i, motionCount=%i, moving=%i\n", battery, p14, p15, temp, brightness, motionCount, moving);
                      g_print("  ");
                      send_to_mqtt_single_value(address, "temperature", temp);
                      g_print("  ");
                      send_to_mqtt_single_value(address, "brightness", brightness);
                      g_print("  ");
                      send_to_mqtt_single_value(address, "motionCount", motionCount);
                      g_print("  ");
                      send_to_mqtt_single_value(address, "moving", moving);
                    }
                }

                //handle_service_data(existing, manufacturer, allocdata);

                g_variant_unref(s_value);
                g_free(allocdata);
            }

        }
        else if (strcmp(property_name, "Adapter") == 0)
        {
        }
        else if (strcmp(property_name, "ServicesResolved") == 0)
        {
        }
        else if (strcmp(property_name, "ManufacturerData") == 0)
        {
            if (isUpdate == FALSE) {
               continue;    // ignore this, it's stale
            }
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

                uint8_t hash;
                int actualLength;
                unsigned char* allocdata = read_byte_array(s_value, &actualLength, &hash);

                if (existing->manufacturer_data_hash != hash)
                {
                    pretty_print2("  ManufacturerData", prop_val, TRUE);  // a{qv}
                    g_print("  ManufData has changed ");
                    send_to_mqtt_array(address, "manufacturerdata", allocdata, actualLength);
                    existing->manufacturer_data_hash = hash;

                    if (existing->distance > 0) {
                      // And repeat the RSSI value every time someone locks or unlocks their phone
                      // Even if the change notification did not include an updated RSSI
                      //g_print("  %s Will resend distance\n", address);
                      send_distance = TRUE;
                    }
                }

                handle_manufacturer(existing, manufacturer, allocdata);

                g_variant_unref(s_value);
                g_free(allocdata);
            }
        }
        else if (strcmp(property_name, "Player") == 0)
        {
            // Property: Player o
            pretty_print2(property_name, prop_val, TRUE);
        }
        else if (strcmp(property_name, "Repeat") == 0)
        {
            // Property: Repeat s
            pretty_print2(property_name, prop_val, TRUE);
        }
        else if (strcmp(property_name, "Shuffle") == 0)
        {
            // Property: Shuffle s
            pretty_print2(property_name, prop_val, TRUE);
        }
        else if (strcmp(property_name, "Track") == 0)
        {
            // Property: Track a {sv}
            pretty_print2(property_name, prop_val, TRUE);
        }
        else if (strcmp(property_name, "Position") == 0)
        {
            // Property: Position {u}
            pretty_print2(property_name, prop_val, TRUE);
        }
        else if (strcmp(property_name, "State") == 0)
        {
            // Property: Position {u}
            pretty_print2(property_name, prop_val, TRUE);
        }
        else
        {
            const char *type = g_variant_get_type_string(prop_val);
            g_print("ERROR Unknown property: '%s' %s\n", property_name, type);
        }

        //g_print("un_ref prop_val\n");
        g_variant_unref(prop_val);
    }

    if (starting && send_distance) {
      g_print("Skip sending, starting\n");
    }
    else
    {
        if (send_distance) {
          g_print("  **** Send distance %6.3f                        ", existing->distance);
          send_to_mqtt_single_float(address, "distance", existing->distance);
          time(&existing->last_sent);

        }
        // TODO: Make this 'if send_distance or 30s has elapsed'
        // Broadcast what we know about the device to all other listeners
        //send_device_mqtt(existing);
        send_device_udp(&state, existing);
    }

    report_devices_count();
}

/*
  Report device with lock on data structures
  NOTE: Free's address when done
*/
static void report_device(struct OverallState* state, GVariant *properties, char *known_address, bool isUpdate)
{
    pthread_mutex_lock(&state->lock);
    report_device_internal(properties, known_address, isUpdate);
    pthread_mutex_unlock(&state->lock);
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

    GVariantIter* interfaces;
    const char *object;
    const gchar *interface_name;
    GVariant *properties;
    //int rc;

    //pretty_print("  params = ", parameters);
    /*
      e.g.  params =  ('/org/bluez/hci0/dev_63_87_D4_04_F8_3B/service0006',
                         {'org.freedesktop.DBus.Introspectable': {},
                          'org.bluez.GattService1':
                             {'UUID': <'00001801-0000-1000-8000-00805f9b34fb'>,
                              'Device': <objectpath '/org/bluez/hci0/dev_63_87_D4_04_F8_3B'>,
                              'Primary': <true>,
                              'Includes': <@ao []>},
                              'org.freedesktop.DBus.Properties': {}})

      e.g. params =  ('/org/bluez/hci0/dev_42_1E_F8_62_6D_F9',
                        {'org.freedesktop.DBus.Introspectable': {},
                         'org.bluez.Device1': {
                             'Address': <'42:1E:F8:62:6D:F9'>,
                             'AddressType': <'random'>,
                             'Alias': <'42-1E-F8-62-6D-F9'>,
                             'Paired': <false>,
                             'Trusted': <false>,
                             'Blocked': <false>,
                             'LegacyPairing': <false>,
                             'RSSI': <int16 -80>,
                             'Connected': <false>,
                             'UUIDs': <@as []>,
                             'Adapter': <objectpath '/org/bluez/hci0'>,
                             'ManufacturerData': <{uint16 76: <[byte 0x10, 0x05, 0x03, 0x1c, 0xdb, 0x1a, 0x22]>}>,
                             'TxPower': <int16 12>,
                             'ServicesResolved': <false>}, 'org.freedesktop.DBus.Properties': {}})

    */

    g_variant_get(parameters, "(&oa{sa{sv}})", &object, &interfaces);

    while (g_variant_iter_next(interfaces, "{&s@a{sv}}", &interface_name, &properties))
    {
        if (g_ascii_strcasecmp(interface_name, "org.bluez.Device1") == 0)
        {
            // Report device immediately, including RSSI
            report_device(&state, properties, NULL, TRUE);
        }
        else if (g_ascii_strcasecmp(interface_name, "org.bluez.GattService1") == 0)
        {
           pretty_print("  Gatt service = ", properties);
        }
        else if (g_ascii_strcasecmp(interface_name, "org.bluez.GattCharacteristic1") == 0)
        {
           //pretty_print("  Gatt characteristic = ", properties);
        }
        else if (g_ascii_strcasecmp(interface_name, "org.bluez.GattDescriptor1") == 0)
        {
           //pretty_print("  Gatt descriptor = ", properties);
        }
        else if (g_ascii_strcasecmp(interface_name, "org.freedesktop.DBus.Introspectable") == 0)
        {
           //pretty_print("  DBus Introspectable = ", properties);
        }
        else if (g_ascii_strcasecmp(interface_name, "org.freedesktop.DBus.Properties") == 0)
        {
           //pretty_print("  DBus properties = ", properties);
        }
        else if (g_ascii_strcasecmp(interface_name, "org.bluez.GattService1") == 0)
        {
           pretty_print("  Gatt service = ", properties);
        }
        else if (g_ascii_strcasecmp(interface_name, "org.bluez.MediaTransport1") == 0)
        {
           pretty_print("  Media transport = ", properties);
        }
        else if (g_ascii_strcasecmp(interface_name, "org.bluez.MediaPlayer1") == 0)
        {
           pretty_print("  Media player = ", properties);
        }
        else if (g_ascii_strcasecmp(interface_name, "org.bluez.Battery1") == 0)
        {
           pretty_print("  Battery = ", properties);
        }
        else {
           g_print("Device appeared, unknown interface: %s\n", interface_name);
        }

        g_variant_unref(properties);
    }
    g_variant_iter_free(interfaces);

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

    GVariantIter *interface_iter;  // heap allocated
    const char *object;
    gchar *interface_name;

    g_variant_get(parameters, "(&oas)", &object, &interface_iter);

    while (g_variant_iter_next(interface_iter, "s", &interface_name))
    {
        if (g_ascii_strcasecmp(interface_name, "org.bluez.Device1") == 0)
        {
            char address[BT_ADDRESS_STRING_SIZE];
            if (get_address_from_path(address, BT_ADDRESS_STRING_SIZE, object))
            {
                // DEBUG g_print("Device %s removed (by bluez) ignoring this\n", address);
                report_device_disconnected_to_MQTT(address);
            }
        }
        g_free(interface_name);
    }
    g_variant_iter_free(interface_iter);
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
    //pretty_print("adapter_changed params = ", params);

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
        // interface_name is something like /org/bluez/hci0/dev_40_F8_A3_77_C5_2B
        // g_print("INTERFACE NAME: %s\n", path);
        report_device(&state, p, address, TRUE);
    }

    g_variant_unref(p);
    g_variant_iter_free(unknown);

    return;
}


// Every 10s we need to let MQTT send and receive messages
int mqtt_refresh(void *parameters)
{
    (void)parameters;
    //GMainLoop *loop = (GMainLoop *)parameters;
    // Send any MQTT messages
    mqtt_sync();

    return TRUE;
}


/*
    BLUEZ_SERVICE_NAME =           'org.bluez'
    DBUS_OM_IFACE =                'org.freedesktop.DBus.ObjectManager'
    LE_ADVERTISING_MANAGER_IFACE = 'org.bluez.LEAdvertisingManager1'
    GATT_MANAGER_IFACE =           'org.bluez.GattManager1'
    GATT_CHRC_IFACE =              'org.bluez.GattCharacteristic1'
    UART_SERVICE_UUID =            '6e400001-b5a3-f393-e0a9-e50e24dcca9e'
    UART_RX_CHARACTERISTIC_UUID =  '6e400002-b5a3-f393-e0a9-e50e24dcca9e'
    UART_TX_CHARACTERISTIC_UUID =  '6e400003-b5a3-f393-e0a9-e50e24dcca9e'
    LOCAL_NAME =                   'rpi-gatt-server'

*/



/*
    bluez_list_devices
*/
void bluez_list_devices(GDBusConnection *conn, GAsyncResult *res, gpointer data)
{
    (void)data;
    GVariant *result = NULL;
    GVariantIter i;
    const gchar *object_path;
    GVariant *ifaces_and_properties;
    GError *error = NULL;

    g_debug("List devices call back\n");

    result = g_dbus_connection_call_finish(conn, res, &error);
    if ((result == NULL) || error)
    {
        g_print("Unable to get result for GetManagedObjects\n");
        print_and_free_error(error);
        // probably out of memory
        exit(-1);
    }

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
                if (g_ascii_strcasecmp(interface_name, "org.bluez.Device1") == 0)
                {
                    report_device(&state, properties, NULL, FALSE);
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

    g_debug("Get managed objects\n");

    g_dbus_connection_call(conn,
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


// Remove old items from cache
gboolean should_remove(struct Device* existing)
{
  time_t now;
  time(&now);

  double delta_time = difftime(now, existing->latest);

  // 10 min for a regular device
  int max_time_ago_seconds = 10 * 60;

  // 20 min for a beacon or other public mac address
  if (existing->addressType == PUBLIC_ADDRESS_TYPE) { max_time_ago_seconds = 20 * 60; }

  // 1 hour upper limit
  if (max_time_ago_seconds > 60 * MAX_TIME_AGO_CACHE) { max_time_ago_seconds = 60 * MAX_TIME_AGO_CACHE; }

  gboolean remove = delta_time > max_time_ago_seconds;

  if (remove) {

    g_print("  Cache remove %s '%s' count=%i dt=%.1fs dist=%.1fm\n", existing->mac, existing->name, existing->count, delta_time, existing->distance);

    // And so when this device reconnects we get a proper reconnect message and so that BlueZ doesn't fill up a huge
    // cache of iOS devices that have passed by or changed mac address

    GVariant* vars[1];
    vars[0] = g_variant_new_string(existing->mac);
    GVariant* param = g_variant_new_tuple(vars, 1);  // floating

    int rc = bluez_adapter_call_method(conn, "RemoveDevice", param, NULL);
    if (rc)
      g_print("Not able to remove %s\n", existing->mac);
    //else
    //  g_debug("    ** Removed %s from BlueZ cache too\n", existing->mac);
  }

  return remove;  // 60 min of no activity = remove from cache
}


int clear_cache(void *parameters)
{
    starting = FALSE;

//    g_print("Clearing cache\n");
    (void)parameters; // not used

    // Remove any item in cache that hasn't been seen for a long time
    for (int i=0; i < state.n; i++) {
      while (i < state.n && should_remove(&state.devices[i])) {
        remove_device(i);              // changes n, but brings a new device to position i
      }
    }

    // And report the updated count of devices present
    report_devices_count();

    return TRUE;
}


// Time when service started running (used to print a delta time)
static time_t started;

void dump_device (struct Device* a)
{
  // Ignore any that have not been seen recently
  //double delta_time = difftime(now, a->latest);
  //if (delta_time > MAX_TIME_AGO_LOGGING_MINUTES * 60) return;

  char* addressType = a->addressType == PUBLIC_ADDRESS_TYPE ? "pub" : a->addressType == RANDOM_ADDRESS_TYPE ? "ran" : "---";
  char* category = category_from_int(a->category);

  float closest_dist = NAN;
  char* closest_ap = "unknown";
  struct ClosestTo* closest = get_closest(a->id);
  if (closest){
    closest_dist = closest->distance;
    struct AccessPoint* ap = get_access_point(closest->access_id);
    if (ap){
        closest_ap = ap->client_id;
    }
  }

  g_print("%3i %s %4i %3s %5.1fm %4i  %6li-%6li %20s %13s %5.1fm %s\n", a->id%1000, a->mac, a->count, addressType, a->distance, a->column, (a->earliest - started), (a->latest - started), a->name, closest_ap, closest_dist, category);
}


/*
    Dump all devices present
*/

int dump_all_devices_tick(void *parameters)
{
    (void)parameters; // not used
    if (starting) return TRUE;   // not during first 30s startup time
    if (!logTable) return TRUE; // no changes since last time
    logTable = FALSE;
    g_print("--------------------------------------------------------------------------------------------------------------\n");
    g_print("Id  Address          Count Typ   Dist  Col   First   Last                 Name              Closest Category  \n");
    g_print("--------------------------------------------------------------------------------------------------------------\n");
    time(&now);
    for (int i=0; i < state.n; i++) {
      dump_device(&state.devices[i]);
    }
    g_print("--------------------------------------------------------------------------------------------------------------\n");

    unsigned long total_minutes = (now - started) / 60;  // minutes
    unsigned int minutes = total_minutes % 60;
    unsigned int hours = (total_minutes / 60) % 24;
    unsigned int days = (total_minutes) / 60 / 24;

    if (days > 1)
      g_print("Uptime: %i days %02i:%02i  People %.2f (%.2f in range)\n", days, hours, minutes, people_closest_count, people_in_view_count);
    else if (days == 1)
      g_print("Uptime: 1 day %02i:%02i  People %.2f (%.2f in range)\n", hours, minutes, people_closest_count, people_in_view_count);
    else
      g_print("Uptime: %02i:%02i  People %.2f (%.2f in range)\n", hours, minutes, people_closest_count, people_in_view_count);

    // Bluez eventually seems to stop sending us data, so for now, just restart every few hours
    if (hours > 2) int_handler(0);

    return TRUE;
}


/*
    Try connecting to a device that isn't currently connected
    Rate limited to one connection attempt every 15s, followed by a disconnect
    Always process disconnects first, and only if none active, start next connection
*/

gboolean try_disconnect (struct Device* a)
{
  if (a->try_connect_state == 1 && a->connected) {
    a->try_connect_state = 2;
    g_print(">>>>>> Disconnect from %s\n", a->mac);
    bluez_adapter_disconnect_device(conn, a->mac);
    return TRUE;
  }
  // didn't change state, try next one
  return FALSE;
}

gboolean try_connect (struct Device* a)
{
  if (a->category != CATEGORY_UNKNOWN) return FALSE; // already has a category
  //if (strlen(a->name) > 0) return FALSE;    // already named

  if (a->count > 1 && a->try_connect_state == 0) {
    a->try_connect_state = 1;
    // Try forcing a connect to get a full dump from the device
    g_print(">>>>>> Connect to %s\n", a->mac);
    bluez_adapter_connect_device(conn, a->mac);
    return TRUE;  
  }
  // didn't change state, try next one
  return FALSE;
}

int try_connect_tick(void *parameters)
{
    (void)parameters; // not used
    if (starting) return TRUE;   // not during first 30s startup time
    for (int i=0; i < state.n; i++) {
      if (try_disconnect(&state.devices[i])) return TRUE;
    }
    for (int i=0; i < state.n; i++) {
      if (try_connect(&state.devices[i])) return TRUE;
    }
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

static char client_id[META_LENGTH];

void initialize_state()
{
    // Default values if not set in environment variables
    const char* description = "Please set a HOST_DESCRIPTION in the environment variables";
    const char* platform = "Please set a HOST_PLATFORM in the environment variables";
    float position_x = -1.0;
    float position_y = -1.0;
    float position_z = -1.0;
    int rssi_one_meter = -64;     // fairly typical RPI3 and iPhone
    float rssi_factor = 3.5;        // fairly cluttered indoor default
    float people_distance = 7.0;    // 7m default range
    state.udp_mesh_port = 7779;
    state.udp_sign_port = 0; // 7778;

    // no devices yet
    state.n = 0;

    gethostname(client_id, META_LENGTH);

    // Optional metadata about the access point for dashboard
    const char* s_client_id = getenv("HOST_NAME");
    const char* s_client_description = getenv("HOST_DESCRIPTION");
    const char* s_client_platform = getenv("HOST_PLATFORM");

    if (s_client_id != NULL) strncpy(client_id, s_client_id, META_LENGTH);
    // These two can just be pointers to the constant strings or the supplied metadata
    if (s_client_description != NULL) description = s_client_description;
    if (s_client_platform != NULL) platform = s_client_platform;

    const char* s_position_x = getenv("POSITION_X");
    const char* s_position_y = getenv("POSITION_Y");
    const char* s_position_z = getenv("POSITION_Z");

    if (s_position_x != NULL) position_x = (float)atof(s_position_x);
    if (s_position_y != NULL) position_y = (float)atof(s_position_y);
    if (s_position_z != NULL) position_z = (float)atof(s_position_z);

    const char* s_rssi_one_meter = getenv("RSSI_ONE_METER");
    const char* s_rssi_factor = getenv("RSSI_FACTOR");
    const char* s_people_distance = getenv("PEOPLE_DISTANCE");

    if (s_rssi_one_meter != NULL) rssi_one_meter = atoi(s_rssi_one_meter);
    if (s_rssi_factor != NULL) rssi_factor = atof(s_rssi_factor);
    if (s_people_distance != NULL) people_distance = atof(s_people_distance);

    state.local = add_access_point(client_id, description, platform, 
        position_x, position_y, position_z,
        rssi_one_meter, rssi_factor, people_distance);

    // UDP Settings

    const char* s_udp_mesh_port = getenv("UDP_MESH_PORT");
    const char* s_udp_sign_port = getenv("UDP_SIGN_PORT");
    const char* s_udp_scale_factor = getenv("UDP_SCALE_FACTOR");

    if (s_udp_mesh_port != NULL) state.udp_mesh_port = atoi(s_udp_mesh_port);
    if (s_udp_sign_port != NULL) state.udp_sign_port = atoi(s_udp_sign_port);
    if (s_udp_scale_factor != NULL) state.udp_scale_factor = atoi(s_udp_scale_factor);

    // MQTT Settings

    state.mqtt_topic = getenv("MQTT_TOPIC");
    if (state.mqtt_topic == NULL) state.mqtt_topic = "BLF";  // sorry, historic name
    state.mqtt_server = getenv("MQTT_SERVER");
    if (state.mqtt_server == NULL) state.mqtt_server = "192.168.0.52:1883";  // sorry, my old server
    state.mqtt_username = getenv("MQTT_USERNAME");
    state.mqtt_password = getenv("MQTT_PASSWORD");
}

void display_state()
{
    g_print("HOST_NAME = %s\n", state.local->client_id);
    g_print("HOST_DESCRIPTION = %s\n", state.local->description);
    g_print("HOST_PLATFORM = %s\n", state.local->platform);
    g_print("Position: (%.1f,%.1f,%.1f)\n", state.local->x, state.local->y, state.local->z);

    g_print("RSSI_ONE_METER Power at 1m : %i\n", state.local->rssi_one_meter);
    g_print("RSSI_FACTOR to distance : %.1f   (typically 2.0 (indoor, cluttered) to 4.0 (outdoor, no obstacles)\n", state.local->rssi_factor);
    g_print("PEOPLE_DISTANCE : %.1fm (cutoff)\n", state.local->people_distance);

    g_print("UDP_MESH_PORT=%i\n", state.udp_mesh_port);
    g_print("UDP_SIGN_PORT=%i\n", state.udp_sign_port);
    g_print("UDP_SCALE_FACTOR=%.1f\n", state.udp_scale_factor);

    g_print("MQTT_TOPIC='%s'\n", state.mqtt_topic);
    g_print("MQTT_SERVER='%s'\n", state.mqtt_server);
    g_print("MQTT_USERNAME='%s'\n", state.mqtt_username);
    g_print("MQTT_PASSWORD='%s'\n", state.mqtt_password == NULL ? "(null)" : "*****");
}


guint prop_changed;
guint iface_added;
guint iface_removed;
GMainLoop *loop;
static char mac_address[6];        // bytes
static char mac_address_text[13];  // string

GCancellable *socket_service;

int main(int argc, char **argv)
{
    (void)argv;
    int rc;

    if (pthread_mutex_init(&state.lock, NULL) != 0)
    {
        printf("\n mutex init failed\n");
        exit(123);
    }

    initialize_state();

    signal(SIGINT, int_handler);
    signal(SIGTERM, int_handler);

    if (argc < 1)
    {
        g_print("Bluetooth scanner\n");
        g_print("   scan\n");
        g_print("   but first set all the environment variables according to README.md");
        g_print("For Azure you must use ssl:// and :8833\n");
        return -1;
    }

    g_print("Get mac address\n");
    get_mac_address(mac_address);
    mac_address_to_string(mac_address_text, sizeof(mac_address_text), mac_address);
    g_print("Local MAC address is: %s\n", mac_address_text);

    // Create a UDP listener for mesh messages about devices connected to other access points in same LAN
    socket_service = create_socket_service(&state);

    g_print("\n\nStarting\n\n");

    conn = g_bus_get_sync(G_BUS_TYPE_SYSTEM, NULL, NULL);
    if (conn == NULL)
    {
        g_print("Not able to get connection to system bus\n");
        return 1;
    }

    // Grab zero time = time when started
    time(&started);

    loop = g_main_loop_new(NULL, FALSE);

    prop_changed = g_dbus_connection_signal_subscribe(conn,
                                                      "org.bluez",
                                                      "org.freedesktop.DBus.Properties",
                                                      "PropertiesChanged",
                                                      NULL,
                                                      NULL,
                                                      G_DBUS_SIGNAL_FLAGS_NONE,
                                                      bluez_signal_adapter_changed,
                                                      NULL,
                                                      NULL);

    iface_added = g_dbus_connection_signal_subscribe(conn,
                                                     "org.bluez",
                                                     "org.freedesktop.DBus.ObjectManager",
                                                     "InterfacesAdded",
                                                     NULL,
                                                     NULL,
                                                     G_DBUS_SIGNAL_FLAGS_NONE,
                                                     bluez_device_appeared,
                                                     loop,
                                                     NULL);

    iface_removed = g_dbus_connection_signal_subscribe(conn,
                                                       "org.bluez",
                                                       "org.freedesktop.DBus.ObjectManager",
                                                       "InterfacesRemoved",
                                                       NULL,
                                                       NULL,
                                                       G_DBUS_SIGNAL_FLAGS_NONE,
                                                       bluez_device_disappeared,
                                                       loop,
                                                       NULL);

    rc = bluez_adapter_set_property(conn, "Powered", g_variant_new("b", TRUE));
    if (rc)
    {
        g_print("Not able to enable the adapter\n");
        goto fail;
    }

    rc = bluez_set_discovery_filter(conn);
    if (rc)
    {
        g_print("Not able to set discovery filter\n");
        goto fail;
    }

    rc = bluez_adapter_call_method(conn, "StartDiscovery", NULL, NULL);
    if (rc)
    {
        g_print("Not able to scan for new devices\n");
        goto fail;
    }
    g_print("Started discovery\n");

    prepare_mqtt(state.mqtt_server, state.mqtt_topic, state.local->client_id, mac_address, state.mqtt_username, state.mqtt_password);

    // Periodically ask Bluez for every device including ones that are long departed
    // but only do updates to devices we have seen, do no not create a device for each
    // as there are too many and most are old, random mac addresses
    g_timeout_add_seconds(60, get_managed_objects, loop);

    // MQTT send
    g_timeout_add_seconds(5, mqtt_refresh, loop);

    // Every 30s look see if any records have expired and should be removed
    // Also clear starting flag
    g_timeout_add_seconds(30, clear_cache, loop);

    // Every 15s dump all devices
    g_timeout_add_seconds(15, dump_all_devices_tick, loop);

    // Every 13s see if any unnamed device is ready to be connected
    g_timeout_add_seconds(13, try_connect_tick, loop);

    g_print("\n\n\n\n\n\n");

    display_state();

    g_print("Start main loop\n");

    g_main_loop_run(loop);

    g_print("END OF MAIN LOOP RUN\n");

    if (argc > 3)
    {
        rc = bluez_adapter_call_method(conn, "SetDiscoveryFilter", NULL, NULL);
        if (rc)
            g_print("Not able to remove discovery filter\n");
    }

    rc = bluez_adapter_call_method(conn, "StopDiscovery", NULL, NULL);
    if (rc)
        g_print("Not able to stop scanning\n");
    g_usleep(100);

    rc = bluez_adapter_set_property(conn, "Powered", g_variant_new("b", FALSE));
    if (rc)
        g_print("Not able to disable the adapter\n");
fail:
    g_dbus_connection_signal_unsubscribe(conn, prop_changed);
    g_dbus_connection_signal_unsubscribe(conn, iface_added);
    g_dbus_connection_signal_unsubscribe(conn, iface_removed);
    g_dbus_connection_close_sync (conn, NULL, NULL);
    g_object_unref(conn);
    return 0;
}

void int_handler(int dummy) {
    (void) dummy;

    g_main_loop_quit(loop);
    g_main_loop_unref(loop);

    g_dbus_connection_signal_unsubscribe(conn, prop_changed);
    g_dbus_connection_signal_unsubscribe(conn, iface_added);
    g_dbus_connection_signal_unsubscribe(conn, iface_removed);
    g_dbus_connection_close_sync (conn, NULL, NULL);
    g_object_unref(conn);

    exit_mqtt();

    close_socket_service(socket_service);

    pthread_mutex_destroy(&state.lock);

    g_print("Clean exit\n");

    exit(0);
}
