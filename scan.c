//#define INDICATOR PCA8833 in Makefile

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
#include <stdint.h>
#include <sys/types.h>
#include <string.h>

#if defined(INDICATOR)
#include <pca9685.h>
#include <wiringPi.h>
#include <wiringPiI2C.h>
#endif

#include "utility.h"
#include "mqtt_send.h"
#include "kalman.h"

// Max allowed devices
#define N 2048

#define PUBLIC_ADDRESS_TYPE 1
#define RANDOM_ADDRESS_TYPE 2

#define CATEGORY_UNKNOWN "unknown"
#define CATEGORY_PHONE "phone"
#define CATEGORY_WATCH "watch"
#define CATEGORY_TABLET "tablet"
#define CATEGORY_HEADPHONES "hp"
#define CATEGORY_COMPUTER "computer"
#define CATEGORY_TV "TV"    // AppleTV
#define CATEGORY_FIXED "fixed"
#define CATEGORY_BEACON "beacon"

// Max allowed length of names and aliases (plus 1 for null)
#define NAME_LENGTH         21

typedef uint8_t  u_int8_t;
typedef uint16_t u_int16_t;
typedef uint32_t u_int32_t;
typedef uint64_t u_int64_t;


static bool starting = TRUE;

// Enable or disable extra debugging

void debug( const char *format , ... ){
   (void)format; // when not debugging
/*
   va_list arglist;
   va_start( arglist, format );
   vprintf( format, arglist );
   va_end( arglist );
*/
}

// delta distance x delta time threshold for sending an update
// e.g. 5m after 2s or 1m after 10s
// prevents swamping MQTT with very small changes
// but also guarantees an occasional update to indicate still alive

#define THRESHOLD 10.0

// Handle Ctrl-c
void     int_handler(int);

static int id_gen = 0;
bool logTable = FALSE;  // set to true each time something changes

/* ENVIRONMENT VARIABLES */

int rssi_one_meter = -64;     // Put a device 1m away and measure the average RSSI
float rssi_factor = 3.5;      // 2.0 to 4.0, lower for indoor or cluttered environments

#if defined(INDICATOR)
bool pca = false;  // is there a PCA8833 attached?
#endif

//	Random value will be from 0 to #
int rnd (int min, int max)
{
    return (rand() % (max - min + 1)) + min;
}

int pins[6] = { 7, 4, 1, 6, 3, 0 };  // 2 and 5 are GND

void set_level(int i, int v) {
    i = i % 3;
    int pin1 = 300 + pins[i];
    int pin2 = 300 + pins[5-i];
    pwmWrite (pin1, v);
    pwmWrite (pin2, v);
}


float light_target;   // 0 to 3.0
float light_state;    // 0 to 3.0


void head_to_target() {
  light_state += (light_target - light_state) * 0.20;

  float fraction = light_state - (int)light_state;

  if (light_state < 0.5) {
       set_level(0, 4096);
       set_level(1, fraction * 2096);
       set_level(2, 0);
  } else if (light_state < 1.0) {
       set_level(0, (1.0 - fraction) * 2096);
       set_level(1, 4096);
       set_level(2, 0);
  } else if (light_state < 1.5) {
       set_level(0, 0);
       set_level(1, 4096);
       set_level(2, fraction * 2096);
  } else if (light_state < 2.0) {
       set_level(0, 0);
       set_level(1, (1.0 - fraction) * 2096);
       set_level(2, 4096);
  } else {
       set_level(0, 0);
       set_level(1, 0);
       set_level(2, 4096);
       // Add flashing
  }
}



void demo(){
  unsigned int iseed = (unsigned int)time(NULL);			//Seed srand() using time() otherwise it will start from a default value of 1
  srand (iseed);
  g_print("DEMO\n");
  if (pca)
  for (int i=0; i <2000; i++)
  {
     int a = 4096 * sin(i * 6.28 / 100);
     int b = 4096 * sin((i + 333) * 6.28 / 100);
     int c = 4096 * sin((i + 666) * 6.28 / 100);

     if (a < 0) a = 0;
     if (b < 0) b = 0;
     if (c < 0) c = 0;

     set_level(0, a);
     set_level(1, b);
     set_level(2, c);
   }
   g_print("Demo done\n");
}


/*
      Connection to DBUS
*/
GDBusConnection *conn;

/*
   Structure for tracking BLE devices in range
*/
struct Device
{
    int id;
    char mac[18];                 // mac address string
    char name[NAME_LENGTH];
    char alias[NAME_LENGTH];
    int8_t addressType;           // 0, 1, 2
    char* category;               // Reasoned guess at what kind of device it is
    int32_t manufacturer;
    bool paired;
    bool connected;
    bool trusted;
    uint32_t deviceclass;          // https://www.bluetooth.com/specifications/assigned-numbers/Baseband/
    uint16_t appearance;
    int manufacturer_data_hash;
    int service_data_hash;
    int uuids_length;              // should use a Hash instead
    int uuid_hash;                 // Hash value of all UUIDs - may ditinguish devices
    int txpower;                   // TX Power
    time_t last_rssi;              // last time an RSSI was received. If gap > 0.5 hour, ignore initial point (dead letter post)
    struct Kalman kalman;
    time_t last_sent;
    float distance;
    struct Kalman kalman_interval; // Tracks time between RSSI events in order to detect large gaps
    time_t earliest;               // Earliest time seen, used to calculate overlap
    time_t latest;                 // Latest time seen, used to calculate overlap
    int count;                     // Count how many times seen (ignore 1 offs)
    int column;                    // Allocated column in a non-overlapping range structure
    int try_connect_state;         // Zero = never tried, 1 = Try in progress, 2 = Done
};

int n = 0;       // current devices

static struct Device devices[N];

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
  for (int i = 0; i < n; i++) {
    struct Device* a = &devices[i];
    a->column = 0;
  }

  for (int k = 0; k< n; k++) {
    bool changed = false;

    for (int i = 0; i < n; i++) {
      for (int j = i+1; j < n; j++) {
        struct Device* a = &devices[i];
        struct Device* b = &devices[j];

        if (a->column != b-> column) continue;

        bool over = overlaps(a, b);

        // cannot be the same device if either has a public address (or we don't have an address type yet)
        bool haveDifferentAddressTypes = (a->addressType>0 && b->addressType>0 && a->addressType != b->addressType);

        // cannot be the same if they both have names and the names are different
        bool haveDifferentNames = (strlen(a->name) > 0) && (strlen(b->name) > 0) && (g_strcmp0(a->name, b->name) != 0);

        // cannot be the same if they both have known categories and they are different
        bool haveDifferentCategories = (g_strcmp0(a->category, b->category) != 0 &&
           g_strcmp0(a->category, CATEGORY_UNKNOWN) != 0 && g_strcmp0(b->category, CATEGORY_UNKNOWN) != 0);

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
  for (int i = index; i < n-1; i++) {
    devices[i] = devices[i+1];
    struct Device* dev = &devices[i];
    // decrease column count, may create clashes, will fix these up next
    dev->column = dev->column > 0 ? dev->column - 1 : 0;
  }
  n--;
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
    char* category;       // category of device in this column (phone, computer, ...)
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
   for (int i = 0; i < n; i++) {
      struct Device* a = &devices[i];
      int col = a->column;
      if (columns[col].distance < 0.0 || columns[col].latest < a->latest) {
        columns[col].distance = a->distance;
        columns[col].latest = a->latest;
        if (strcmp(columns[col].category, CATEGORY_UNKNOWN) == 0) {
          // a later unknown does not override an actual phone category
          columns[col].category = a->category;
        }
      }
   }
}


#define N_RANGES 10
static int32_t ranges[N_RANGES] = {1, 2, 5, 10, 15, 20, 25, 30, 35, 100};
static int8_t reported_ranges[N_RANGES] = {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1};
int people_count = 0;

/*
    Find best packing of device time ranges into columns
    //Set every device to column zero
    While there is any overlap, find the overlapping pair, move the second one to the next column
*/


void report_devices_count() {
    debug("report_devices_count\n");
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
           if (strcmp(columns[col].category, CATEGORY_PHONE) != 0) continue;   // only counting phones now not beacons, laptops, ...
           if (columns[col].distance < 0.01) continue;   // not allocated

           double delta_time = difftime(now, columns[col].latest);
           if (delta_time > MAX_TIME_AGO_COUNTING_MINUTES * 60) continue;

           if (columns[col].distance < range) min++;
        }

        int just_this_range = min - previous;
        if (reported_ranges[i] != just_this_range) {
          g_print("Devices present at range %im %i    \n", range, just_this_range);
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
    int people = 0;

    for (int i=0; i < range_limit; i++)
    {
       people += reported_ranges[i];
    }

    if (people != people_count) {
      people_count = people;

      g_print("People count = %i\n", people);

#if defined(INDICATOR)


      light_target = people / 5.0;   // 3 = red

      head_to_target();

/*
      int r=0, g=0, b=0, v=0;

      if (people < 2) g = 4096;
      else if (people < 4) b = 4096;
      else r = 4096;
      // pulse the LEDs the appropriate color
      //int r = 2048 + 2048 * sin((float)tick * 16 / 128);
      //int g = 2048 + 2048 * sin((float)tick * 4 / 128);
      //int b = 2048 + 2048 * sin((float)tick * 1 / 128);
      //int v = 4096;// 2048 + 2048 * sin((float)tick * 7 / 128);

      if (pca) {
        pwmWrite (300, b);
        pwmWrite (301, r);
        pwmWrite (302, g);
        pwmWrite (303, v);
      }
*/

#endif
   }
}



/* SEND TO MQTT WITH ACCESS POINT MAC ADDRESS AND TIME STAMP */



typedef void (*method_cb_t)(GObject *, GAsyncResult *, gpointer);


static void print_and_free_error(GError *error) 
{
  if (error)
  {
       g_print("Error: %s\n", error->message);
       g_error_free (error);
  }
}

static int bluez_adapter_call_method(const char *method, GVariant *param, method_cb_t method_cb)
{
    //    g_print("bluez_adapter_call_method(%s)\n", method);
    GError *error = NULL;

    g_dbus_connection_call(conn,
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
    {
       print_and_free_error(error);
       return 1;
    }
    return 0;
}

static int bluez_device_call_method(const char *method, char* address, GVariant *param, method_cb_t method_cb)
{
    //    g_print("bluez_device_call_method(%s)\n", method);
    GError *error = NULL;
    char path[100];

    // e.g. /org/bluez/hci0/dev_C1_B4_70_76_57_EE
    get_path_from_address(address, path, sizeof(path));
    //g_print("Path %s\n", path);

    g_dbus_connection_call(conn,
                           "org.bluez",
                           path,
                           "org.bluez.Device1",
                           method,
                           param,
                           NULL,                       // the expected type of the reply (which will be a tuple), or null
                           G_DBUS_CALL_FLAGS_NONE,
                           20000,                      // timeout in millseconds or -1
                           NULL,                       // cancellable or null
                           method_cb,                  // callback or null
                           &error);
    if (error != NULL)
    {
       print_and_free_error(error);
       return 1;
    }
    return 0;
}


/*
      Get a single property from a Bluez device
*/

/*
static int bluez_adapter_get_property(const char* path, const char *prop, method_cb_t method_cb)
{
	GError *error = NULL;

	g_dbus_connection_call(conn,
				     "org.bluez",
                                     path,
				     "org.freedesktop.DBus.Properties",
				     "Get",
                                     g_variant_new("(ss)", "org.bluez.Adapter1", prop),
				     // For "set": g_variant_new("(ssv)", "org.bluez.Device1", prop, value),
				     NULL,
				     G_DBUS_CALL_FLAGS_NONE,
				     20000,
				     NULL,
                                     method_cb,
				     &error);
	if(error != NULL)
		return 1;

	return 0;
}
*/


static void bluez_get_discovery_filter_cb(GObject *con,
                                          GAsyncResult *res,
                                          gpointer data)
{
    (void)data;

    debug("START bluez_get_discovery_filter_cb\n");

    GVariant *result = NULL;
    GError *error = NULL;

    result = g_dbus_connection_call_finish((GDBusConnection *)con, res, &error);

    if (result == NULL || error) 
    {
        g_print("Unable to get result for GetDiscoveryFilter\n");
        print_and_free_error(error);
    }

    if (result)
    {
        GVariant *child = g_variant_get_child_value(result, 0);
        pretty_print("GetDiscoveryFilter", child);
        g_variant_unref(child);
    }
    g_variant_unref(result);
    debug("DONE bluez_get_discovery_filter_cb\n");
}


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

static int bluez_adapter_connect_device(char *address)
{
	int rc = bluez_device_call_method("Connect", address, NULL, NULL);
	if(rc) {
		g_print("Not able to call Connect\n");
		return 1;
	}
	return 0;
}

static int bluez_adapter_disconnect_device(char *address)
{
	int rc = bluez_device_call_method("Disconnect", address, NULL, NULL);
	if(rc) {
		g_print("Not able to call Disconnect\n");
		return 1;
	}
	return 0;
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

    debug("START read_byte_array\n");

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

    debug("END read_byte_array\n");
    return allocdata;
}

void optional(char* name, char* value) {
  if (strlen(name)) return;
  g_strlcpy(name, value, NAME_LENGTH);
}

void soft_set(char** name, char* value) {
  if (strcmp(*name, CATEGORY_UNKNOWN) == 0) {
    *name = value;
  }
}

/*
     handle the manufacturer data
*/
void handle_manufacturer(struct Device * existing, uint16_t manufacturer, unsigned char* allocdata)
{
    debug("START handle_manufacturer\n");
    if (manufacturer == 0x004c) {   // Apple
        uint8_t apple_device_type = allocdata[00];
        if (apple_device_type == 0x02) {
          optional(existing->alias, "Beacon");
          g_print("  Beacon\n") ;
          existing->category = CATEGORY_BEACON;
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

          if (lower_bits == 0x07) { g_print(" Lock screen (0x07) "); soft_set(&existing->category, CATEGORY_PHONE); }
          else if (lower_bits == 0x17) { g_print(" Lock screen   (0x17) "); soft_set(&existing->category, CATEGORY_PHONE); }
          else if (lower_bits == 0x1b) { g_print(" Home screen   (0x1b) "); soft_set(&existing->category, CATEGORY_PHONE); }
          else if (lower_bits == 0x1c) { g_print(" Home screen   (0x1c) "); soft_set(&existing->category, CATEGORY_PHONE); }
          else if (lower_bits == 0x10) { g_print(" Home screen   (0x10) "); soft_set(&existing->category, CATEGORY_PHONE); }
          else if (lower_bits == 0x0e) { g_print(" Outgoing call (0x0e) "); soft_set(&existing->category, CATEGORY_PHONE); }
          else if (lower_bits == 0x1e) { g_print(" Incoming call (0x1e) "); soft_set(&existing->category, CATEGORY_PHONE); }
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
static void report_device_to_MQTT(GVariant *properties, char *known_address, bool isUpdate)
{
    debug("START report_device_to_MQTT\n");

    logTable = TRUE;
    //       g_print("report_device_to_MQTT(%s)\n", address);
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
    for (int i = 0; i<n; i++) {
      if (memcmp(devices[i].mac, address, 18) == 0) {
         existing = &devices[i];
      }
    }

    if (existing == NULL)
    {
        if (!isUpdate)
        {
           // DEBUG g_print("Skip %s, bluez get_devices call and not seen yet\n", address);
           return;
        }

        if (n == N) {
          g_print("Error, array of devices is full\n");
          return;
        }

        // Grab the next empty item in the array
        existing = &devices[n++];
        existing->id = id_gen++;                 // unique ID for each
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
           debug("Repeat device %i. %s '%s' (%s)\n", existing->id, address, existing->name, existing->alias);
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
            else if (strcmp(name, "iWatch") == 0) existing->category = CATEGORY_WATCH;
            else if (strcmp(name, "Apple Watch") == 0) existing->category = CATEGORY_WATCH;
            else if (strcmp(name, "AppleTV") == 0) existing->category = CATEGORY_TV;
            else if (strcmp(name, "Apple TV") == 0) existing->category = CATEGORY_TV;
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

            double exponent = ((rssi_one_meter - (double)rssi) / (10.0 * rssi_factor));

            double distance = pow(10.0, exponent);

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
            g_print("  %s Icon: '%s'\n", address, icon);
            if (strcmp(icon, "computer") == 0) soft_set(&existing->category, CATEGORY_COMPUTER);
            else if (strcmp(icon, "phone") == 0) soft_set(&existing->category, CATEGORY_PHONE);
            else if (strcmp(icon, "multimedia-player") == 0) soft_set(&existing->category, CATEGORY_TV);

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
    }

    report_devices_count();
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
            report_device_to_MQTT(properties, NULL, TRUE);
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
        report_device_to_MQTT(p, address, TRUE);
    }

    g_variant_unref(p);
    g_variant_iter_free(unknown);

    return;
}

static int bluez_adapter_set_property(const char *prop, GVariant *value)
{
    GVariant *result;
    GError *error = NULL;
    GVariant *gvv = g_variant_new("(ssv)", "org.bluez.Adapter1", prop, value);

    result = g_dbus_connection_call_sync(conn,
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
    {
       print_and_free_error(error);
       return 1;
    }

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
    GError *error = NULL;

    debug("List devices call back\n");

    result = g_dbus_connection_call_finish(con, res, &error);
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
    mqtt_sync();
    head_to_target();
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



int get_managed_objects(void *parameters)
{
    GMainLoop *loop = (GMainLoop *)parameters;

    debug("Get managed objects\n");

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

  // 1 min for single hit, 160 min for regular ping
  int max_time_ago_seconds = existing->count * 60;   // 1 after 1 min, 2 after 2 min, ...
  if (max_time_ago_seconds > 60 * MAX_TIME_AGO_CACHE) { max_time_ago_seconds = 60 * MAX_TIME_AGO_CACHE; }

  gboolean remove = delta_time > max_time_ago_seconds;

  if (remove) {

    g_print("  Cache remove %s %s %.1fs %.1fm\n", existing->mac, existing->name, delta_time, existing->distance);

    // And so when this device reconnects we get a proper reconnect message and so that BlueZ doesn't fill up a huge
    // cache of iOS devices that have passed by or changed mac address

    GVariant* vars[1];
    vars[0] = g_variant_new_string(existing->mac);
    GVariant* param = g_variant_new_tuple(vars, 1);  // floating

    int rc = bluez_adapter_call_method("RemoveDevice", param, NULL);
    if (rc)
      g_print("Not able to remove %s\n", existing->mac);
    //else
    //  debug("    ** Removed %s from BlueZ cache too\n", existing->mac);
  }

  return remove;  // 60 min of no activity = remove from cache
}


int clear_cache(void *parameters)
{
    starting = FALSE;

//    g_print("Clearing cache\n");
    (void)parameters; // not used

    // Remove any item in cache that hasn't been seen for a long time
    for (int i=0; i<n; i++) {
      while (i<n && should_remove(&devices[i])) {
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

  g_print("%3i %s %4i %3s %5.1fm %4i  %6li-%6li %20s %20s %s\n", a->id%1000, a->mac, a->count, addressType, a->distance, a->column, (a->earliest - started), (a->latest - started), a->name, a->alias, a->category);
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
    g_print("Id  Address          Count Typ   Dist  Col First   Last                   Name                Alias Category  \n");
    g_print("--------------------------------------------------------------------------------------------------------------\n");
    time(&now);
    for (int i=0; i<n; i++) {
      dump_device(&devices[i]);
    }
    g_print("--------------------------------------------------------------------------------------------------------------\n");

    unsigned long total_minutes = (now - started) / 60;  // minutes
    unsigned int minutes = total_minutes % 60;
    unsigned int hours = (total_minutes / 60) % 24;
    unsigned int days = (total_minutes) / 60 / 24;

    if (days > 1)
      g_print("Uptime: %i days %02i:%02i  People %i   target %.1f %.1f\n", days, hours, minutes, people_count, light_target, light_state);
    else if (days == 1)
      g_print("Uptime: 1 day %02i:%02i  People %i   target %.1f %.1f\n", hours, minutes, people_count,light_target, light_state);
    else
      g_print("Uptime: %02i:%02i  People %i   target %.1f %.1f\n", hours, minutes, people_count, light_target, light_state);


    // Bluez eventually seems to stop sending us data, so for now, just restart every six hours
    if (hours > 5) int_handler(0);

    return TRUE;
}


/*
    Try connecting to a device that isn't currently connected
    Rate limited to one connection attempt every 15s, followed by a disconnect
*/

gboolean try_connect (struct Device* a)
{
  if (strlen(a->name) > 0) return FALSE;    // already named

  if (a->count > 1 && a->try_connect_state == 0) {
    a->try_connect_state = 1;
    // Try forcing a connect to get a full dump from the device
    g_print(">>>>>> Connect to %s\n", a->mac);
    bluez_adapter_connect_device(a->mac);
    return TRUE;  }
  else if (a->try_connect_state == 1 && a->connected) {
    a->try_connect_state = 2;
    g_print(">>>>>> Disconnect from %s\n", a->mac);
    bluez_adapter_disconnect_device(a->mac);
    return TRUE;
  }
  // didn't change state, try next one
  return FALSE;
}

int try_connect_tick(void *parameters)
{
    (void)parameters; // not used
    if (starting) return TRUE;   // not during first 30s startup time
    for (int i=0; i<n; i++) {
      if (try_connect(&devices[i])) break;
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


guint prop_changed;
guint iface_added;
guint iface_removed;
GMainLoop *loop;
static char client_id[256];         // linux allows more, truncated
static char mac_address[6];        // bytes
static char mac_address_text[13];  // string


int main(int argc, char **argv)
{
    int rc;

    signal(SIGINT, int_handler);
    signal(SIGTERM, int_handler);

    if (argc < 2)
    {
        g_print("Bluetooth scanner\n");
        g_print("   scan <[ssl://]mqtt server:[port]> [topicRoot=BLF] [username] [password]\n");
        g_print("For Azure you must use ssl:// and :8833\n");
        return -1;
    }

#if defined(INDICATOR)
/* WIRING PI AND PWB9685 */
    wiringPiSetup();
    int fd = pca9685Setup(300, 0x40, 100);  // 100Hz less flicker

    if (fd < 0) {
      g_print("No 9685 found %i\n", fd);
      pca = false;
    }
    else {
      pca = true;
      pca9685PWMReset(fd);
    }

    demo();

#endif

    char* mqtt_uri = argv[1];
    char* mqtt_topicRoot = argc > 2 ? argv[2] : "BLF";
    char* username = argc > 3 ? argv[3] : NULL;
    char* password = argc > 4 ? argv[4] : NULL;

    gethostname(client_id, sizeof(client_id));

    g_print("Hostname is %s\n", client_id);

    g_print("Get mac address\n");

    get_mac_address(mac_address);

    mac_address_to_string(mac_address_text, sizeof(mac_address_text), mac_address);
    g_print("Local MAC address is: %s\n", mac_address_text);

    const char* s_rssi_one_meter = getenv("RSSI_ONE_METER");
    const char* s_rssi_factor = getenv("RSSI_FACTOR");

    if (s_rssi_one_meter != NULL) rssi_one_meter = atoi(s_rssi_one_meter);
    if (s_rssi_factor != NULL) rssi_factor = atof(s_rssi_factor);

    g_print("Using RSSI Power at 1m : %i\n", rssi_one_meter);
    g_print("Using RSSI to distance factor : %.1f (typically 2.0 (indoor, cluttered) to 4.0 (outdoor, no obstacles)\n", rssi_factor);

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

    prepare_mqtt(mqtt_uri, mqtt_topicRoot, client_id, mac_address, username, password);

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

    g_print("Clean exit\n");

    exit(0);
}
