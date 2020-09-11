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
#include "utility.h"
#include "mqtt_send.h"
#include "udp.h"
#include "kalman.h"
#include "device.h"
#include "bluetooth.h"

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

// For LED flash
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>

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

static int id_gen = 0;
bool logTable = FALSE; // set to true each time something changes

/*
      Connection to DBUS
*/
GDBusConnection *conn;

/*
  Do these two devices overlap in time? If so they cannot be the same device
*/
bool overlaps(struct Device *a, struct Device *b)
{
    if (a->earliest > b->latest)
        return FALSE; // a is entirely after b
    if (b->earliest > a->latest)
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
                bool haveDifferentNames = (strlen(a->name) > 0) && (strlen(b->name) > 0) && (g_strcmp0(a->name, b->name) != 0);

                // cannot be the same if they both have known categories and they are different
                // Used to try to blend unknowns in with knowns but now we get category right 99.9% of the time, no longer necessary
                bool haveDifferentCategories = (a->category != b->category); // && (a->category != CATEGORY_UNKNOWN) && (b->category != CATEGORY_UNKNOWN);

                if (over || haveDifferentAddressTypes || haveDifferentNames || haveDifferentCategories)
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
}

/*
   Remove a device from array and move all later devices up one spot
*/
void remove_device(int index)
{
    for (int i = index; i < state.n - 1; i++)
    {
        state.devices[i] = state.devices[i + 1];
        struct Device *dev = &state.devices[i];
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
            columns[col].distance = a->distance;
            if (a->category != CATEGORY_UNKNOWN)
            {
                // a later unknown does not override an actual phone category nor extend it
                // This if is probably not necessary now as overlap tests for this
                columns[col].category = a->category;
                columns[col].latest = a->latest;
                // Do we 'own' this device or does someone else
                columns[col].isClosest = true;
                struct ClosestTo *closest = get_closest(a);
                columns[col].isClosest = closest != NULL && closest->access_id == state.local->id;
            }
        }
    }
}

#define N_RANGES 10
static int32_t ranges[N_RANGES] = {1, 2, 5, 10, 15, 20, 25, 30, 35, 100};
static int8_t reported_ranges[N_RANGES] = {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1};

/*
    COMMUNICATION WITH DISPLAYS OVER UDP
*/

static char udp_last_sent = -1;

void send_to_udp_display(struct OverallState *state, float people_closest, float people_in_range)
{
    // This adjusts how people map to lights which are on a 0.0-3.0 range
    double scale_factor = state->udp_scale_factor;
    // 0.0 = green
    // 1.5 = blue
    // 3.0 = red (but starts going red anywhere above 2.0)

    // NB This sends constantly even if value is unchanged because UDP is unreliable
    // and because a display may become unplugged and then reconnected
    if (state->udp_sign_port > 0)
    {
        char msg[4];
        msg[0] = 0;
        // send as ints for ease of consumption on ESP8266
        msg[1] = (int)(people_closest * scale_factor * 10.0);
        msg[2] = (int)(people_in_range * scale_factor * 10.0);
        msg[3] = 0;

        // TODO: Move to JSON for more flexibility sending names too

        udp_send(state->udp_sign_port, msg, sizeof(msg));

        // Log only when it changes
        if (udp_last_sent != msg[1])
        {
            g_info("UDP Sent %i", msg[1]);
            udp_last_sent = msg[1];
        }
    }
}

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

            double delta_time = difftime(now, columns[col].latest);
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
        send_to_mqtt_distances((unsigned char *)reported_ranges, N_RANGES * sizeof(int8_t));
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

        double delta_time = difftime(now, columns[col].latest);
        if (delta_time > MAX_TIME_AGO_COUNTING_MINUTES * 60)
            continue;

        // double score = 0.55 - atan(delta_time/40.0  - 4.0) / 3.0; -- left some spikes in the graph, dropped too quickly
        double score = 0.55 - atan(delta_time / 45.0 - 4.0) / 3.0;
        // A curve that stays a 1.0 for a while and then drops rapidly around 3 minutes out
        if (score > 0.9)
            score = 1.0;
        if (score < 0.0)
            score = 0.0;

        // Expected value E[x] = i x p(i) so sum p(i) for each column which is one person
        people_in_range += score;
        if (columns[col].isClosest)
            people_closest += score;
    }

    if (fabs(people_in_range - state.local->people_in_range_count) > 0.01 || fabs(people_closest - state.local->people_closest_count) > 0.01)
    {
        state.local->people_closest_count = people_closest;
        state.local->people_in_range_count = people_in_range;
        g_info("People count = %.2f (%.2f in range)\n", people_closest, people_in_range);

        // And send access point to everyone over UDP
        send_access_point_udp(&state);

        GVariant *parameters = g_variant_new("(ds)", people_closest, "people"); // floating ref
        GError *error = NULL;
        gboolean ret = g_dbus_connection_emit_signal(conn, NULL, "/com/signswift/sniffer", "com.signswift.sniffer", "PeopleClosest", parameters, &error);
        if (ret)
        {
            print_and_free_error(error);
        }
    }

    send_to_udp_display(&state, people_closest, people_in_range);
}

/* SEND TO MQTT WITH ACCESS POINT MAC ADDRESS AND TIME STAMP */

/*
    REPORT DEVICE TO MQTT

    address if known, if not have to find it in the dictionary of passed properties
    appeared = we know this is fresh data so send RSSI and TxPower with timestamp
*/

static void report_device_disconnected_to_MQTT(char *address)
{
    (void)address;
    // Not used
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

void optional(char *name, char *value)
{
    if (strlen(name))
        return;
    g_strlcpy(name, value, NAME_LENGTH);
}

void soft_set_category(int8_t *category, int8_t category_new)
{
    if (*category == CATEGORY_UNKNOWN)
        *category = category_new;
}

/*
     handle the manufacturer data
*/
void handle_manufacturer(struct Device *existing, uint16_t manufacturer, unsigned char *allocdata)
{
    //g_debug("START handle_manufacturer\n");
    if (manufacturer == 0x004c)
    { // Apple
        uint8_t apple_device_type = allocdata[00];
        if (apple_device_type == 0x01)
        {
            g_info(" **** Apple Device type 0x01 - what is this?");
        }
        else if (apple_device_type == 0x02)
        {
            optional(existing->alias, "Beacon");
            g_debug("  Beacon\n");
            existing->category = CATEGORY_BEACON;

            // Aprilbeacon temperature sensor
            if (strncmp(existing->name, "abtemp", 6) == 0)
            {
                uint8_t temperature = allocdata[21];
                send_to_mqtt_single_value(existing->mac, "temperature", temperature);
            }
        }
        else if (apple_device_type == 0x03)
            g_print("  Airprint \n");
        else if (apple_device_type == 0x05)
            g_print("  Airdrop \n");
        else if (apple_device_type == 0x07)
        {
            optional(existing->name, "Airpods");
            g_debug("  Airpods \n");
            existing->category = CATEGORY_HEADPHONES;
        }
        else if (apple_device_type == 0x08)
        {
            optional(existing->alias, "Siri");
            g_print("  Siri \n");
        }
        else if (apple_device_type == 0x09)
        {
            optional(existing->alias, "Airplay");
            g_print("  Airplay \n");
        }
        else if (apple_device_type == 0x0a)
        {
            optional(existing->alias, "Apple 0a");
            g_print("  Apple 0a \n");
        }
        else if (apple_device_type == 0x0b)
        {
            optional(existing->name, "iWatch?");
            g_debug("  Watch_c");
            existing->category = CATEGORY_WEARABLE;
        }
        else if (apple_device_type == 0x0c)
            g_print("  Handoff \n");
        else if (apple_device_type == 0x0d)
            g_print("  WifiSet \n");
        else if (apple_device_type == 0x0e)
            g_print("  Hotspot \n");
        else if (apple_device_type == 0x0f)
            g_print("  WifiJoin \n");
        else if (apple_device_type == 0x10)
        {
            g_debug("  Nearby ");

            // e.g. phone: <[byte 0x10, 0x06, 0x51, 0x1e, 0xc1, 0x36, 0x99, 0xe1]>}

            // too soon ... name comes later ... optional(existing->name, "Apple Device");
            // Not right, MacBook Pro seems to send this too

            uint8_t device_status = allocdata[02];
            if (device_status & 0x80)
                g_print("0x80 ");
            else
                g_print(" ");
            if (device_status & 0x40)
                g_print(" ON +");
            else
                g_print("OFF +");

            uint8_t lower_bits = device_status & 0x3f;

            // These could be iPad or iWatch too, not certain it's a phone at this point
            if (lower_bits == 0x07)
            {
                g_print(" Lock screen (0x07) "); /* soft_set_category(&existing->category, CATEGORY_PHONE); */
            }
            else if (lower_bits == 0x17)
            {
                g_print(" Lock screen   (0x17) "); /*  soft_set_category(&existing->category, CATEGORY_PHONE);*/
            }
            else if (lower_bits == 0x1b)
            {
                g_print(" Home screen   (0x1b) "); /*  soft_set_category(&existing->category, CATEGORY_PHONE);*/
            }
            else if (lower_bits == 0x1c)
            {
                g_print(" Home screen   (0x1c) "); /*  soft_set_category(&existing->category, CATEGORY_PHONE);*/
            }
            else if (lower_bits == 0x10)
            {
                g_print(" Home screen   (0x10) "); /*  soft_set_category(&existing->category, CATEGORY_PHONE);*/
            }
            else if (lower_bits == 0x0e)
            {
                g_print(" Outgoing call (0x0e) "); /*  soft_set_category(&existing->category, CATEGORY_PHONE);*/
            }
            else if (lower_bits == 0x1e)
            {
                g_print(" Incoming call (0x1e) "); /*  soft_set_category(&existing->category, CATEGORY_PHONE);*/
            }
            else
                g_debug(" Unknown (0x%.2x) ", lower_bits);

            if (allocdata[03] & 0x10)
                g_print("1");
            else
                g_print("0");
            if (allocdata[03] & 0x08)
                g_print("1");
            else
                g_print("0");
            if (allocdata[03] & 0x04)
                g_print("1");
            else
                g_print("0");
            if (allocdata[03] & 0x02)
                g_print("1");
            else
                g_print("0");
            if (allocdata[03] & 0x01)
                g_print("1");
            else
                g_print("0");

            // These do not seem to be quite right
            if (allocdata[03] == 0x18)
                g_print(" Apple? (0x18)");
            else if (allocdata[03] == 0x1c)
                g_print(" Apple? (0x1c)");
            else if (allocdata[03] == 0x1e)
                g_print(" iPhone?  (0x1e)");
            else if (allocdata[03] == 0x1a)
                g_print(" iWatch?  (0x1a)");
            else if (allocdata[03] == 0x00)
                g_print(" TBD ");
            else
                g_debug(" Device type (%.2x)", allocdata[03]);

            g_debug("\n");
        }
        else
        {
            g_debug("Did not recognize apple device type %.2x", apple_device_type);
        }
    }
    else if (manufacturer == 0x0087)
    {
        optional(existing->name, "Garmin");
        existing->category = CATEGORY_WEARABLE; // could be fitness tracker
    }
    else if (manufacturer == 0x05A7)
    {
        optional(existing->name, "Sonos");
        existing->category = CATEGORY_FIXED;
    }
    else if (manufacturer == 0xb4c1)
    {
        optional(existing->name, "Dycoo"); // not on official Bluetooth website??
    }
    else if (manufacturer == 0x0101)
    {
        optional(existing->name, "Fugoo, Inc.");
        existing->category = CATEGORY_HEADPHONES;
    }
    else if (manufacturer == 0x0310)
    {
        optional(existing->name, "SGL Italia S.r.l.");
        existing->category = CATEGORY_HEADPHONES;
    }
    else if (manufacturer == 0x3042)
    { // 12354 = someone didn't register
        optional(existing->name, "Unknown Manuf");
        existing->category = CATEGORY_HEADPHONES;
    }
    else if (manufacturer == 0x0075)
    {
        optional(existing->name, "Samsung");
        g_debug("  Manufacturer is Samsung 0x0075\n");
    }
    else if (manufacturer == 0xff19)
    {
        optional(existing->name, "Samsung");
        g_debug("  Manufacturer is Samsung? 0xff19\n");
    }
    else if (manufacturer == 0x0131)
    {
        optional(existing->name, "Cypress Semiconductor");
        g_debug("  Manufacturer is Cypress Semiconductor\n");
    }
    else if (manufacturer == 0x0110)
    {
        optional(existing->name, "Nippon Seiki Co., Ltd.");
        g_debug("  Manufacturer is Nippon Seiki Co., Ltd.\n");
    }
    else if (manufacturer == 0x0399)
    {
        optional(existing->name, "Nikon");
        g_debug("  Manufacturer is Nikon Corporation\n");
        existing->category = CATEGORY_FIXED;
    }
    else if (manufacturer == 0x0003)
    {
        optional(existing->name, "IBM");
        g_debug("  IBM\n");
        existing->category = CATEGORY_FIXED;
    }
    else if (manufacturer == 0x0501)
    {
        optional(existing->name, "Polaris ND");
        g_debug("  Polaris ND\n");
        existing->category = CATEGORY_FIXED;
    }
    else if (manufacturer == 0x014f)
    {
        optional(existing->name, "B&W Group Ltd.");
        g_debug("  B&W Group Ltd.\n");
        existing->category = CATEGORY_FIXED;
    }
    else if (manufacturer == 0x00c4)
    {
        optional(existing->name, "LG Electronics");
        g_debug("  LG Electronics\n");
        existing->category = CATEGORY_TV;
    }
    else if (manufacturer == 0x03ee)
    {
        optional(existing->name, "CUBE Technolgies");
        g_debug("  CUBE Technolgies\n");
        existing->category = CATEGORY_FIXED;
    }
    else if (manufacturer == 0x00e0)
    {
        optional(existing->name, "Google");
        g_debug("  Google\n");
    }
    else if (manufacturer == 0x0085)
    {
        optional(existing->name, "BlueRadios ODM");
        g_debug("  BlueRadios, Inc. (ODM)\n");
    }
    else if (manufacturer == 0x0434)
    {
        optional(existing->name, "Hatch Baby, Inc.");
        g_debug("  Hatch Baby, Inc.\n");
        existing->category = CATEGORY_FIXED;
    }
    else if (manufacturer == 0x0157)
    {
        optional(existing->name, "Anhui Huami Information Technology");
        g_debug("  Anhui Huami Information Technology\n");
        existing->category = CATEGORY_FIXED;
    }
    else if (manufacturer == 0x001d)
    {
        optional(existing->name, "Qualcomm");
        g_debug("  Qualcomm\n");
    }
    else if (manufacturer == 0x015e)
    {
        optional(existing->name, "Unikey Technologies, Inc");
        g_debug("  Unikey Technologies, Inc\n");
        existing->category = CATEGORY_FIXED;
    }
    else if (manufacturer == 0x01a5)
    {
        optional(existing->name, "Icon Health and Fitness");
        g_debug("  Icon Health and Fitness\n");
        existing->category = CATEGORY_FIXED;
    }
    else if (manufacturer == 0x00d2)
    {
        optional(existing->name, "AbTemp");
        g_debug("  Ignoring manufdata\n");
    }
    else
    {
        // https://www.bluetooth.com/specifications/assigned-numbers/16-bit-uuids-for-members/
        char manuf[32];
        snprintf(manuf, sizeof(manuf), "Manufacturer 0x%04x", manufacturer);
        g_info("  Did not recognize %s\n", manuf);
        optional(existing->name, manuf);
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

    if (known_address)
    {
        g_strlcpy(address, known_address, 18);
    }
    else
    {
        // Get address from properies dictionary if not already present
        GVariant *address_from_dict = g_variant_lookup_value(properties, "Address", G_VARIANT_TYPE_STRING);
        if (address_from_dict)
        {
            const char *addr = g_variant_get_string(address_from_dict, NULL);
            g_strlcpy(address, addr, 18);
            g_variant_unref(address_from_dict);
        }
        else
        {
            g_warning("ERROR address is null");
            return;
        }
    }

    struct Device *existing = NULL;

    // Get existing device report
    for (int i = 0; i < state.n; i++)
    {
        if (memcmp(state.devices[i].mac, address, 18) == 0)
        {
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

        if (state.n == N)
        {
            g_warning("Error, array of devices is full\n");
            return;
        }

        // Grab the next empty item in the array
        existing = &state.devices[state.n++];
        existing->id = id_gen++;               // unique ID for each
        existing->hidden = false;              // we own this one
        g_strlcpy(existing->mac, address, 18); // address
        existing->superceeds = 0;              // will be filled in when calculating columns

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
        existing->try_connect_state = TRY_CONNECT_ZERO;

        // RSSI values are stored with kalman filtering
        kalman_initialize(&existing->kalman);
        existing->distance = 0;
        time(&existing->last_sent);
        time(&existing->last_rssi);

        existing->last_sent = existing->last_sent - 1000; //1s back so first RSSI goes through
        existing->last_rssi = existing->last_rssi - 1000;

        kalman_initialize(&existing->kalman_interval);

        g_info("Added device %i. %s\n", existing->id, address);
    }
    else
    {
        if (!isUpdate)
        {
            // from get_all_devices which includes stale data
            //g_debug("Repeat device %i. %s '%s' (%s)\n", existing->id, address, existing->name, existing->alias);
        }
        else
        {
            g_debug("Existing device %i. %s '%s' (%s)\n", existing->id, address, existing->name, existing->alias);
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

            if (strncmp(name, existing->name, NAME_LENGTH - 1) != 0)
            {
                g_info("  %s Name has changed '%s' -> '%s'\n", address, existing->name, name);
                send_to_mqtt_single(address, "name", name);
                g_strlcpy(existing->name, name, NAME_LENGTH);
            }
            else
            {
                // g_print("  Name unchanged '%s'=='%s'\n", name, existing->name);
            }

            // TODO: Allocate categories by name automatically
            // for(int i = 0; i < sizeof(phones); i++){
            //     if (strcmp(name, phones[i] == 0)) existing->category = CATEGORY_PHONE;
            // }

            if (strcmp(name, "iPhone") == 0)
                existing->category = CATEGORY_PHONE;
            else if (strcmp(name, "iPad") == 0)
                existing->category = CATEGORY_TABLET;
            else if (strcmp(name, "MacBook pro") == 0)
                existing->category = CATEGORY_COMPUTER;
            else if (strcmp(name, "BOOTCAMP") == 0)
                existing->category = CATEGORY_COMPUTER;
            else if (strcmp(name, "BOOTCAMP2") == 0)
                existing->category = CATEGORY_COMPUTER;
            // Watches
            else if (strcmp(name, "iWatch") == 0)
                existing->category = CATEGORY_WEARABLE;
            else if (strcmp(name, "Apple Watch") == 0)
                existing->category = CATEGORY_WEARABLE;
            else if (strncmp(name, "Galaxy Watch", 12) == 0)
                existing->category = CATEGORY_WEARABLE;
            else if (strncmp(name, "Gear S3", 7) == 0)
                existing->category = CATEGORY_WEARABLE;
            else if (strncmp(name, "fenix", 5) == 0)
                existing->category = CATEGORY_WEARABLE;
            else if (strncmp(name, "Ionic", 5) == 0)
                existing->category = CATEGORY_WEARABLE; // FITBIT
            else if (strncmp(name, "Versa", 5) == 0)
                existing->category = CATEGORY_WEARABLE; // FITBIT
            else if (strncmp(name, "Charge 2", 8) == 0)
                existing->category = CATEGORY_WEARABLE; // FITBIT
            else if (strncmp(name, "Charge 3", 8) == 0)
                existing->category = CATEGORY_WEARABLE; // FITBIT
            else if (strncmp(name, "Mi Smart Band", 13) == 0)
                existing->category = CATEGORY_WEARABLE; // Fitness
            else if (strncmp(name, "TICKR X", 7) == 0)
                existing->category = CATEGORY_WEARABLE; // Heartrate
            // TVs
            else if (strcmp(name, "AppleTV") == 0)
                existing->category = CATEGORY_TV;
            else if (strcmp(name, "Apple TV") == 0)
                existing->category = CATEGORY_TV;
            // Beacons
            else if (strncmp(name, "AprilBeacon", 11) == 0)
                existing->category = CATEGORY_BEACON;
            else if (strncmp(name, "abtemp", 6) == 0)
                existing->category = CATEGORY_BEACON;
            else if (strncmp(name, "abeacon", 7) == 0)
                existing->category = CATEGORY_BEACON;
            else if (strncmp(name, "estimote", 8) == 0)
                existing->category = CATEGORY_BEACON;
            else if (strncmp(name, "Tile", 4) == 0)
                existing->category = CATEGORY_BEACON;
            else if (strncmp(name, "LYWSD03MMC", 10) == 0)
                existing->category = CATEGORY_BEACON;
            // Headphones or speakers
            else if (strncmp(name, "Sesh Evo-LE", 11) == 0)
                existing->category = CATEGORY_HEADPHONES; // Skullcandy
            else if (strncmp(name, "F2", 2) == 0)
                existing->category = CATEGORY_HEADPHONES; // Soundpal F2 spakers
            else if (strncmp(name, "Jabra", 5) == 0)
                existing->category = CATEGORY_HEADPHONES;
            else if (strncmp(name, "LE-Bose", 7) == 0)
                existing->category = CATEGORY_HEADPHONES;
            else if (strncmp(name, "LE-reserved_C", 13) == 0)
                existing->category = CATEGORY_HEADPHONES;
            else if (strncmp(name, "Blaze", 5) == 0)
                existing->category = CATEGORY_HEADPHONES;
            else if (strncmp(name, "HarpBT", 6) == 0)
                existing->category = CATEGORY_HEADPHONES;
            // TVs
            // e.g. "[TV] Samsung Q70 Series (65)" icon is audio_card
            else if (strncmp(name, "[TV] Samsung", 12) == 0)
                existing->category = CATEGORY_TV;
            else if (strncmp(name, "[Signage] Samsung", 17) == 0)
                existing->category = CATEGORY_TV;
            // Printers
            else if (strncmp(name, "ENVY Photo", 10) == 0)
                existing->category = CATEGORY_FIXED; // printer
            // POS Terminals
            else if (strncmp(name, "Bluesnap", 8) == 0)
                existing->category = CATEGORY_FIXED;  // POS
            else if (strncmp(name, "IBM", 3) == 0)
                existing->category = CATEGORY_FIXED;  // POS
            else if (strncmp(name, "NWTR040", 7) == 0)
                existing->category = CATEGORY_FIXED;  // POS
            // Cars
            else if (strncmp(name, "Audi", 4) == 0)
                existing->category = CATEGORY_CAR;
            else if (strncmp(name, "VW ", 3) == 0)
                existing->category = CATEGORY_CAR;
            else if (strncmp(name, "BMW", 3) == 0)
                existing->category = CATEGORY_CAR;
            else if (strncmp(name, "GM_PEPS_", 8) == 0)
                existing->category = CATEGORY_CAR; // maybe the key fob
            else if (strncmp(name, "Subaru", 6) == 0)
                existing->category = CATEGORY_CAR;
            else if (strncmp(name, "Land Rover", 10) == 0)
                existing->category = CATEGORY_CAR;
            // TODO: Android device names
        }
        else if (strcmp(property_name, "Alias") == 0)
        {
            char *alias = g_variant_dup_string(prop_val, NULL);
            trim(alias);

            if (strncmp(alias, existing->alias, NAME_LENGTH - 1) != 0) // has_prefix because we may have truncated it
            {
                //g_debug("  %s Alias has changed '%s' -> '%s'\n", address, existing->alias, alias);
                // NOT CURRENTLY USED: send_to_mqtt_single(address, "alias", alias);
                g_strlcpy(existing->alias, alias, NAME_LENGTH);
            }
            else
            {
                // g_print("  Alias unchanged '%s'=='%s'\n", alias, existing->alias);
            }
        }
        else if (strcmp(property_name, "AddressType") == 0)
        {
            char *addressType = g_variant_dup_string(prop_val, NULL);
            int newAddressType = (g_strcmp0("public", addressType) == 0) ? PUBLIC_ADDRESS_TYPE : RANDOM_ADDRESS_TYPE;

            // Compare values and send
            if (existing->addressType != newAddressType)
            {
                existing->addressType = newAddressType;
                if (newAddressType == PUBLIC_ADDRESS_TYPE)
                {
                    g_debug("  %s Address type has changed -> '%s'\n", address, addressType);
                    // Not interested in random as most devices are random
                }
                send_to_mqtt_single(address, "type", addressType);
            }
            else
            {
                // DEBUG g_print("  Address type unchanged\n");
            }
            g_free(addressType);
        }
        else if (strcmp(property_name, "RSSI") == 0 && (isUpdate == FALSE))
        {
            // Ignore this, it isn't helpful
            // int16_t rssi = g_variant_get_int16(prop_val);
            // g_print("  %s RSSI repeat %i\n", address, rssi);
        }
        else if (strcmp(property_name, "RSSI") == 0)
        {
            if (!isUpdate)
            {
                //g_print("$$$$$$$$$$$$ RSSI is unreliable for get all devices\n");
                continue;
            }

            int16_t rssi = g_variant_get_int16(prop_val);
            //send_to_mqtt_single_value(address, "rssi", rssi);

            g_debug("  %s RSSI %i\n", address, rssi);

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
            if (strcmp(existing->name, "iPad") == 0)
            {
                distance = distance * 2.1;
            }

            float averaged = kalman_update(&existing->kalman, distance);

            // 10s with distance change of 1m triggers send
            // 1s with distance change of 10m triggers send

            double delta_time_sent = difftime(now, existing->last_sent);
            double delta_v = fabs(existing->distance - averaged);
            double score = delta_v * delta_time_sent;

            if (score > 10.0 || delta_time_sent > 30)
            {
                //g_print("  %s Will send rssi=%i dist=%.1fm, delta v=%.1fm t=%.0fs score=%.0f\n", address, rssi, averaged, delta_v, delta_time_sent, score);
                existing->distance = averaged;
                send_distance = TRUE;
            }
            else
            {
                g_debug("  %s Skip sending rssi=%i dist=%.1fm, delta v=%.1fm t=%.0fs score=%.0f\n", address, rssi, averaged, delta_v, delta_time_sent, score);
            }
        }
        else if (strcmp(property_name, "TxPower") == 0)
        {
            int16_t p = g_variant_get_int16(prop_val);
            if (p != existing->txpower)
            {
                g_debug("  %s TXPOWER has changed %i\n", address, p);
                // NOT CURRENTLY USED ... send_to_mqtt_single_value(address, "txpower", p);
                existing->txpower = p;
            }
        }

        else if (strcmp(property_name, "Paired") == 0)
        {
            bool paired = g_variant_get_boolean(prop_val);
            if (existing->paired != paired)
            {
                g_debug("  %s Paired has changed        ", address);
                if (state.verbosity >= Details) {
                    send_to_mqtt_single_value(address, "paired", paired ? 1 : 0);
                }
                existing->paired = paired;
            }
        }
        else if (strcmp(property_name, "Connected") == 0)
        {
            bool connected = g_variant_get_boolean(prop_val);
            if (existing->connected != connected)
            {
                if (connected)
                    g_debug("  %s Connected      ", address);
                else
                    g_debug("  %s Disconnected   ", address);

                if (state.verbosity >= Details) {
                  send_to_mqtt_single_value(address, "connected", connected ? 1 : 0);
                }
                existing->connected = connected;
            }
        }
        else if (strcmp(property_name, "Trusted") == 0)
        {
            bool trusted = g_variant_get_boolean(prop_val);
            if (existing->trusted != trusted)
            {
                g_debug("  %s Trusted has changed       ", address);
                if (state.verbosity >= Details) {
                    send_to_mqtt_single_value(address, "trusted", trusted ? 1 : 0);
                }
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

            int uuid_hash = 0;

            g_variant_get(prop_val, "as", &iter_array);

            while (g_variant_iter_loop(iter_array, "s", &str))
            {
                if (strlen(str) < 36)
                    continue; // invalid GUID

                uuidArray[actualLength++] = strdup(str);

                for (uint32_t i = 0; i < strlen(str); i++)
                {
                    uuid_hash += (i + 1) * str[i]; // sensitive to position in UUID but not to order of UUIDs
                }
            }
            g_variant_iter_free(iter_array);

            if (actualLength > 0)
            {
                existing->uuid_hash = uuid_hash & 0xffffffff;
            }

            if (existing->uuids_length != actualLength)
            {
                if (actualLength > 0)
                {
                    char gatts[1024];
                    gatts[0] = '\0';

                    // Print off the UUIDs here
                    for (int i = 0; i < actualLength; i++)
                    {
                        char *strCopy = strdup(uuidArray[i]);

                        // All common BLE UUIDs are of the form: 0000XXXX-0000-1000-8000-00805f9b34fb
                        // so we only need two hex bytes. But Apple and others use complete GUIDs
                        // for their own services, so let's take the first four hex bytes
                        strCopy[8] = '\0';
                        int64_t ble_uuid = strtol(strCopy, NULL, 16);

                        // https://www.bluetooth.com/specifications/gatt/characteristics/

                        if (ble_uuid == 0x00001000) append_text(gatts, sizeof(gatts), "ServiceDiscoveryServer, ");
                        else if (ble_uuid == 0x00001001L) append_text(gatts, sizeof(gatts), "BrowseGroupDescriptor, ");
                        else if (ble_uuid == 0x00001002L) append_text(gatts, sizeof(gatts), "PublicBrowseGroup, ");
                        else if (ble_uuid == 0x00001101L) append_text(gatts, sizeof(gatts), "SerialPort, ");
                        else if (ble_uuid == 0x00001102L) append_text(gatts, sizeof(gatts), "LANAccessUsingPPP, ");
                        else if (ble_uuid == 0x00001103L) append_text(gatts, sizeof(gatts), "DialupNetworking, ");
                        else if (ble_uuid == 0x00001104L) append_text(gatts, sizeof(gatts), "IrMCSync, ");
                        else if (ble_uuid == 0x00001105L) append_text(gatts, sizeof(gatts), "OBEXObjectPush, ");
                        else if (ble_uuid == 0x00001106L) append_text(gatts, sizeof(gatts), "OBEXFileTransfer, ");
                        else if (ble_uuid == 0x00001107L) append_text(gatts, sizeof(gatts), "IrMCSyncCommand, ");
                        else if (ble_uuid == 0x00001108L) append_text(gatts, sizeof(gatts), "Headset, ");
                        else if (ble_uuid == 0x00001109L) append_text(gatts, sizeof(gatts), "CordlessTelephony, ");
                        else if (ble_uuid == 0x0000110AL) append_text(gatts, sizeof(gatts), "AudioSource, ");
                        else if (ble_uuid == 0x0000110BL) append_text(gatts, sizeof(gatts), "AudioSink, ");
                        else if (ble_uuid == 0x0000110CL) append_text(gatts, sizeof(gatts), "AVRemoteControlTarget, ");
                        else if (ble_uuid == 0x0000110DL) append_text(gatts, sizeof(gatts), "AdvancedAudioDistribution, ");
                        else if (ble_uuid == 0x0000110EL) append_text(gatts, sizeof(gatts), "AVRemoteControl, ");
                        else if (ble_uuid == 0x0000110FL) append_text(gatts, sizeof(gatts), "VideoConferencing, ");
                        else if (ble_uuid == 0x00001110L) append_text(gatts, sizeof(gatts), "Intercom, ");
                        else if (ble_uuid == 0x00001111L) append_text(gatts, sizeof(gatts), "Fax, ");
                        else if (ble_uuid == 0x00001112L) append_text(gatts, sizeof(gatts), "HeadsetAudioGateway, ");
                        else if (ble_uuid == 0x00001113L) append_text(gatts, sizeof(gatts), "WAP, ");
                        else if (ble_uuid == 0x00001114L) append_text(gatts, sizeof(gatts), "WAPClient, ");
                        else if (ble_uuid == 0x00001115L) append_text(gatts, sizeof(gatts), "PANU, ");
                        else if (ble_uuid == 0x00001116L) append_text(gatts, sizeof(gatts), "NAP, ");
                        else if (ble_uuid == 0x00001117L) append_text(gatts, sizeof(gatts), "GN, ");
                        else if (ble_uuid == 0x00001118L) append_text(gatts, sizeof(gatts), "DirectPrinting, ");
                        else if (ble_uuid == 0x00001119L) append_text(gatts, sizeof(gatts), "ReferencePrinting, ");
                        else if (ble_uuid == 0x0000111AL) append_text(gatts, sizeof(gatts), "Imaging, ");
                        else if (ble_uuid == 0x0000111BL) append_text(gatts, sizeof(gatts), "ImagingResponder, ");
                        else if (ble_uuid == 0x0000111CL) append_text(gatts, sizeof(gatts), "ImagingAutomaticArchive, ");
                        else if (ble_uuid == 0x0000111DL) append_text(gatts, sizeof(gatts), "ImagingReferenceObjects, ");
                        else if (ble_uuid == 0x0000111EL) append_text(gatts, sizeof(gatts), "Handsfree, ");
                        else if (ble_uuid == 0x0000111FL) append_text(gatts, sizeof(gatts), "HandsfreeAudioGateway, ");
                        else if (ble_uuid == 0x00001120L) append_text(gatts, sizeof(gatts), "DirectPrintingReferenceObjects, ");
                        else if (ble_uuid == 0x00001121L) append_text(gatts, sizeof(gatts), "ReflectedUI, ");
                        else if (ble_uuid == 0x00001122L) append_text(gatts, sizeof(gatts), "BasicPringing, ");
                        else if (ble_uuid == 0x00001123L) append_text(gatts, sizeof(gatts), "PrintingStatus, ");
                        else if (ble_uuid == 0x00001124L) append_text(gatts, sizeof(gatts), "HumanInterfaceDevice, ");
                        else if (ble_uuid == 0x00001125L) append_text(gatts, sizeof(gatts), "HardcopyCableReplacement, ");
                        else if (ble_uuid == 0x00001126L) append_text(gatts, sizeof(gatts), "HCRPrint, ");
                        else if (ble_uuid == 0x00001127L) append_text(gatts, sizeof(gatts), "HCRScan, ");
                        else if (ble_uuid == 0x00001128L) append_text(gatts, sizeof(gatts), "CommonISDNAccess, ");
                        else if (ble_uuid == 0x00001129L) append_text(gatts, sizeof(gatts), "VideoConferencingGW, ");
                        else if (ble_uuid == 0x0000112AL) append_text(gatts, sizeof(gatts), "UDIMT, ");
                        else if (ble_uuid == 0x0000112BL) append_text(gatts, sizeof(gatts), "UDITA, ");
                        else if (ble_uuid == 0x0000112CL) append_text(gatts, sizeof(gatts), "AudioVideo, ");
                        else if (ble_uuid == 0x0000112DL) append_text(gatts, sizeof(gatts), "SIMAccess, ");
                        else if (ble_uuid == 0x00001200L) append_text(gatts, sizeof(gatts), "PnPInformation, ");
                        else if (ble_uuid == 0x00001201L) append_text(gatts, sizeof(gatts), "GenericNetworking, ");
                        else if (ble_uuid == 0x00001202L) append_text(gatts, sizeof(gatts), "GenericFileTransfer, ");
                        else if (ble_uuid == 0x00001203L) append_text(gatts, sizeof(gatts), "GenericAudio, ");
                        else if (ble_uuid == 0x00001204L) append_text(gatts, sizeof(gatts), "GenericTelephony, ");
                        else if (ble_uuid == 0x2a29L) append_text(gatts, sizeof(gatts), "Manufacturer, ");
                        else if (ble_uuid == 0x1800L) append_text(gatts, sizeof(gatts), "Generic access, ");
                        else if (ble_uuid == 0x1801L) append_text(gatts, sizeof(gatts), "Generic attribute, ");
                        else if (ble_uuid == 0x1802L) append_text(gatts, sizeof(gatts), "Immediate Alert, ");
                        else if (ble_uuid == 0x1803L) append_text(gatts, sizeof(gatts), "Link loss, ");
                        else if (ble_uuid == 0x1804L) append_text(gatts, sizeof(gatts), "Tx Power level, ");
                        else if (ble_uuid == 0x1805L) append_text(gatts, sizeof(gatts), "Current time, ");
                        else if (ble_uuid == 0x180fL) append_text(gatts, sizeof(gatts), "Battery, ");
                        else if (ble_uuid == 0x111eL) append_text(gatts, sizeof(gatts), "HandsFree, ");
                        else if (ble_uuid == 0x180aL) append_text(gatts, sizeof(gatts), "Device information, ");
                        else if (ble_uuid == 0x180dL) append_text(gatts, sizeof(gatts), "Heart rate service, ");
                        else if (ble_uuid == 0x2A37L) append_text(gatts, sizeof(gatts), "Heart rate measurement ");
                        else if (ble_uuid == 0xFEAAL) append_text(gatts, sizeof(gatts), "Eddystone ");
                        else if (ble_uuid == 0xb9401000) append_text(gatts, sizeof(gatts), "Estimote 1, ");
                        else if (ble_uuid == 0xb9402000) append_text(gatts, sizeof(gatts), "Estimote 2,");
                        else if (ble_uuid == 0xb9403000) append_text(gatts, sizeof(gatts), "Estimote 3, ");
                        else if (ble_uuid == 0xb9404000) append_text(gatts, sizeof(gatts), "Estimote 4, ");
                        else if (ble_uuid == 0xb9405000) append_text(gatts, sizeof(gatts), "Estimote 5, ");
                        else if (ble_uuid == 0xb9406000) append_text(gatts, sizeof(gatts), "Estimote 6, ");
                        else if (ble_uuid == 0x89d3502bL) append_text(gatts, sizeof(gatts), "Apple MS, ");
                        else if (ble_uuid == 0x7905f431L) append_text(gatts, sizeof(gatts), "Apple NCS, ");
                        else if (ble_uuid == 0xd0611e78L) append_text(gatts, sizeof(gatts), "Apple CS, ");
                        else if (ble_uuid == 0x9fa480e0L) append_text(gatts, sizeof(gatts), "Apple XX, ");
                        else if (ble_uuid == 0xd0611e78L) append_text(gatts, sizeof(gatts), "Continuity, ");
                        else if (ble_uuid == 0xffa0L) append_text(gatts, sizeof(gatts), "Accelerometer, ");
                        else if (ble_uuid == 0xffe0L) append_text(gatts, sizeof(gatts), "Temperature, ");
                        else
                            append_text(gatts, sizeof(gatts), "Unknown(%s), ", strCopy);

                        g_free(strCopy);
                    }
                    char **allocdata = g_malloc(actualLength * sizeof(char *)); // array of pointers to strings
                    memcpy(allocdata, uuidArray, actualLength * sizeof(char *));
                    g_info ("  %s UUIDs: %s", address, gatts);
                    if (state.verbosity >= Details) {
                      send_to_mqtt_uuids(address, "uuids", allocdata, actualLength);
                    }
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
                g_debug("  %s Class has changed to %i        ", address, deviceclass);
                send_to_mqtt_single_value(address, "class", deviceclass);
                existing->deviceclass = deviceclass;
            }
        }
        else if (strcmp(property_name, "Icon") == 0)
        {
            char *icon = g_variant_dup_string(prop_val, NULL);
            if (&existing->category == CATEGORY_UNKNOWN)
            {
                // Should track icon and test against that instead
                g_debug("  %s Icon: '%s'\n", address, icon);
            }
            if (strcmp(icon, "computer") == 0)
                soft_set_category(&existing->category, CATEGORY_COMPUTER);
            else if (strcmp(icon, "phone") == 0)
                soft_set_category(&existing->category, CATEGORY_PHONE);
            else if (strcmp(icon, "multimedia-player") == 0)
                soft_set_category(&existing->category, CATEGORY_TV);
            else if (strcmp(icon, "audio-card") == 0)
                soft_set_category(&existing->category, CATEGORY_AUDIO_CARD);
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
            if (isUpdate == FALSE)
            {
                continue; // ignore this, it's stale
            }
            // A a{sv} value
            // {'000080e7-0000-1000-8000-00805f9b34fb':
            //    <[byte 0xb0, 0x23, 0x25, 0xcb, ...]>}
            //    <[byte 0xb0, 0x23, 0x25, 0xcb, 0x66, 0x54, 0xae, 0xab, 0x0a, 0x2b, 0x00, 0x04, 0x33, 0x09, 0xee, 0x60, 0x24, 0x2e, 0x00, 0xf7, 0x07, 0x00, 0x00]>}

            //pretty_print2("  ServiceData ", prop_val, TRUE); // a{sv}

            GVariant *s_value;
            GVariantIter i;
            char *service_guid;

            g_variant_iter_init(&i, prop_val);
            while (g_variant_iter_next(&i, "{sv}", &service_guid, &s_value))
            { // Just one

                uint8_t hash;
                int actualLength;
                unsigned char *allocdata = read_byte_array(s_value, &actualLength, &hash);

                if (existing->service_data_hash != hash)
                {
                    //g_debug("  ServiceData has changed ");
                    pretty_print2("  ServiceData", prop_val, TRUE); // a{qv}
                    // Sends the service GUID as a key in the JSON object
                    if (state.verbosity >= Details) {
                      send_to_mqtt_array(address, "ServiceData", service_guid, allocdata, actualLength);
                    }
                    existing->service_data_hash = hash;

                    // temp={p[16] - 10} brightness={p[17]} motioncount={p[19] + p[20] * 256} moving={p[22]}");
                    if (strcmp(service_guid, "000080e7-0000-1000-8000-00805f9b34fb") == 0)
                    { // Sensoro
                        existing->category = CATEGORY_BEACON;

                        int battery = allocdata[14] + 256 * allocdata[15]; // ???
                        int p14 = allocdata[14];
                        int p15 = allocdata[15];
                        int temp = allocdata[16] - 10;
                        int brightness = allocdata[18];
                        int motionCount = allocdata[19] + allocdata[20] * 256;
                        int moving = allocdata[21];
                        g_debug("Sensoro battery=%i, p14=%i, p15=%i, temp=%i, brightness=%i, motionCount=%i, moving=%i\n", battery, p14, p15, temp, brightness, motionCount, moving);
                        send_to_mqtt_single_value(address, "temperature", temp);
                        send_to_mqtt_single_value(address, "brightness", brightness);
                        send_to_mqtt_single_value(address, "motionCount", motionCount);
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
            if (isUpdate == FALSE)
            {
                continue; // ignore this, it's stale
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
                    if (manufacturer == 0x4c){
                        g_info("  %s Manufacturer Apple  ", address);
                    } else {
                        g_info("  %s Manufacturer 0x%4x  ", address, manufacturer);
                    }
                    send_to_mqtt_single_value(address, "manufacturer", manufacturer);
                    existing->manufacturer = manufacturer;
                }

                uint8_t hash;
                int actualLength;
                unsigned char *allocdata = read_byte_array(s_value, &actualLength, &hash);

                if (existing->manufacturer_data_hash != hash)
                {
                    pretty_print2("  ManufacturerData", prop_val, TRUE); // a{qv}
                    //g_debug("  ManufData has changed ");
                    // Need to send actual manufacturer number not always 76 here TODO
                    if (state.verbosity >= Details) {
                      send_to_mqtt_array(address, "manufacturerdata", "76", allocdata, actualLength);
                    }
                    existing->manufacturer_data_hash = hash;

                    if (existing->distance > 0)
                    {
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
            g_debug("ERROR Unknown property: '%s' %s\n", property_name, type);
        }

        //g_print("un_ref prop_val\n");
        g_variant_unref(prop_val);
    }

    if (starting && send_distance)
    {
        g_debug("Skip sending, starting\n");
    }
    else
    {
        if (send_distance)
        {
            g_debug("  **** Send distance %6.3f                        ", existing->distance);
            if (state.verbosity >= Distances){
              send_to_mqtt_single_float(address, "distance", existing->distance);
            }
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
static void report_device(struct OverallState *state, GVariant *properties, char *known_address, bool isUpdate)
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

    GVariantIter *interfaces;
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
            // Gatt service = : {'UUID': <'00001805-0000-1000-8000-00805f9b34fb'>,
            //   'Device': <objectpath '/org/bluez/hci0/dev_4F_87_E1_13_66_A5'>,
            //   'Primary': <true>,
            //   'Includes': <@ao []>}
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
        else
        {
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

    GVariantIter *interface_iter; // heap allocated
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

    //g_debug("List devices call back\n");

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

    //g_debug("Get managed objects\n");

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
gboolean should_remove(struct Device *existing)
{
    time_t now;
    time(&now);

    double delta_time = difftime(now, existing->latest);

    // 10 min for a regular device
    int max_time_ago_seconds = 10 * 60;

    // 20 min for a beacon or other public mac address
    if (existing->addressType == PUBLIC_ADDRESS_TYPE)
    {
        max_time_ago_seconds = 20 * 60;
    }

    // 1 hour upper limit
    if (max_time_ago_seconds > 60 * MAX_TIME_AGO_CACHE)
    {
        max_time_ago_seconds = 60 * MAX_TIME_AGO_CACHE;
    }

    gboolean remove = delta_time > max_time_ago_seconds;

    if (remove)
    {

        g_debug("  Cache remove %s '%s' count=%i dt=%.1fs dist=%.1fm\n", existing->mac, existing->name, existing->count, delta_time, existing->distance);

        // And so when this device reconnects we get a proper reconnect message and so that BlueZ doesn't fill up a huge
        // cache of iOS devices that have passed by or changed mac address

        GVariant *vars[1];
        vars[0] = g_variant_new_string(existing->mac);
        GVariant *param = g_variant_new_tuple(vars, 1); // floating

        int rc = bluez_adapter_call_method(conn, "RemoveDevice", param, NULL);
        if (rc)
            g_warning("Not able to remove %s\n", existing->mac);
        //else
        //  g_debug("    ** Removed %s from BlueZ cache too\n", existing->mac);
    }

    return remove; // 60 min of no activity = remove from cache
}

int clear_cache(void *parameters)
{
    starting = FALSE;

    //    g_print("Clearing cache\n");
    (void)parameters; // not used

    // Remove any item in cache that hasn't been seen for a long time
    for (int i = 0; i < state.n; i++)
    {
        while (i < state.n && should_remove(&state.devices[i]))
        {
            remove_device(i); // changes n, but brings a new device to position i
        }
    }

    // And report the updated count of devices present
    report_devices_count();

    return TRUE;
}

// Time when service started running (used to print a delta time)
static time_t started;

void dump_device(struct Device *d)
{
    // Ignore any that have not been seen recently
    //double delta_time = difftime(now, a->latest);
    //if (delta_time > MAX_TIME_AGO_LOGGING_MINUTES * 60) return;

    char *addressType = d->addressType == PUBLIC_ADDRESS_TYPE ? "pub" : d->addressType == RANDOM_ADDRESS_TYPE ? "ran" : "---";
    char *category = category_from_int(d->category);

    float closest_dist = NAN;
    char *closest_ap = "unknown";
    struct ClosestTo *closest = get_closest(d);
    if (closest)
    {
        closest_dist = closest->distance;
        struct AccessPoint *ap = get_access_point(closest->access_id);
        if (ap)
        {
            closest_ap = ap->client_id;
        }
    }

    g_info("%4i %s %4i %3s %5.1fm %4i  %6li-%6li %20s %13s %5.1fm %s\n", d->id % 10000, d->mac, d->count, addressType, d->distance, d->column, (d->earliest - started), (d->latest - started), d->name, closest_ap, closest_dist, category);
}

/*
    Dump all devices present
*/

int dump_all_devices_tick(void *parameters)
{
    (void)parameters; // not used
    if (starting)
        return TRUE; // not during first 30s startup time
    if (!logTable)
        return TRUE; // no changes since last time
    logTable = FALSE;
    g_info("---------------------------------------------------------------------------------------------------------------\n");
    g_info("Id   Address          Count Typ   Dist  Col   First   Last                 Name              Closest Category  \n");
    g_info("---------------------------------------------------------------------------------------------------------------\n");
    time(&now);
    for (int i = 0; i < state.n; i++)
    {
        dump_device(&state.devices[i]);
    }
    g_info("---------------------------------------------------------------------------------------------------------------\n");

    unsigned long total_minutes = (now - started) / 60; // minutes
    unsigned int minutes = total_minutes % 60;
    unsigned int hours = (total_minutes / 60) % 24;
    unsigned int days = (total_minutes) / 60 / 24;

    float people_closest = state.local->people_closest_count;
    float people_in_range = state.local->people_in_range_count;

    if (days > 1)
        g_info("Uptime: %i days %02i:%02i  People %.2f (%.2f in range)\n", days, hours, minutes, people_closest, people_in_range);
    else if (days == 1)
        g_info("Uptime: 1 day %02i:%02i  People %.2f (%.2f in range)\n", hours, minutes, people_closest, people_in_range);
    else
        g_info("Uptime: %02i:%02i  People %.2f (%.2f in range)\n", hours, minutes, people_closest, people_in_range);

    print_access_points();

    // Bluez eventually seems to stop sending us data, so for now, just restart every few hours
    if (hours > 2)
        int_handler(0);

    return TRUE;
}

/*
    Try connecting to a device that isn't currently connected
    Rate limited to one connection attempt every 15s, followed by a disconnect
    Always process disconnects first, and only if none active, start next connection
*/

gboolean try_disconnect(struct Device *a)
{
    if (a->try_connect_state > TRY_CONNECT_ZERO && a->try_connect_state < TRY_CONNECT_COMPLETE)
    {

        a->try_connect_state = a->try_connect_state + 1;

        if (a->try_connect_state == TRY_CONNECT_COMPLETE)
        {
            if (a->connected)
            {
                g_info(">>>>> Disconnect from %i. %s\n", a->id, a->mac);
                bluez_adapter_disconnect_device(conn, a->mac);
            }
            else if (a->category == CATEGORY_UNKNOWN)
            {
                // Failed to get enough data from connection or did not connect
                g_info(">>>>> Failed to connect to %i. %s\n", a->id, a->mac);
            }
            return TRUE;
        }
    }
    // didn't change state, try next one
    return FALSE;
}

/* 
  Attempts to connect to a device IF if we don't already know what category it is
  returns true if a connection was initiated so that outer loop can limit how many
  simultaneous connections are attempted.
*/
gboolean try_connect(struct Device *a)
{
    if (a->category != CATEGORY_UNKNOWN)
        return FALSE; // already has a category
    //if (strlen(a->name) > 0) return FALSE;    // already named

    // Count will always be > 0 but optionally can change '0' to '1' to only attempt
    // connecting when a device has been seen more than once. Useful in situations where
    // there may be too many transient devices to attempt connection to all of them
    if (a->count > 0 && a->try_connect_state == 0)
    {
        a->try_connect_state = 1;
        // Try forcing a connect to get a full dump from the device
        g_info(">>>>>> Connect to %i. %s\n", a->id, a->mac);
        bluez_adapter_connect_device(conn, a->mac);
        return TRUE;
    }
    // didn't change state, try next one
    return FALSE;
}

static int led_state = 0;

// TODO: Suppress this on non-Pi platforms

static void flash_led()
{
    led_state = (led_state + 1) & 0x01;
    char d = led_state == 0 ? '0' : '1';
    int fd = open("/sys/class/leds/led0/brightness", O_WRONLY);
    write (fd, &d, 1);
    close(fd);
}

#define SIMULTANEOUS_CONNECTIONS 5

int try_connect_tick(void *parameters)
{
    flash_led();

    int simultaneus_connections = 0; // assumes none left from previous tick
    (void)parameters;                // not used
    if (starting)
        return TRUE; // not during first 30s startup time
    for (int i = 0; i < state.n; i++)
    {
        // Disconnect all devices that are in a try connect state
        try_disconnect(&state.devices[i]);
    }
    for (int i = 0; i < state.n; i++)
    {
        // Connect up to SIMULTANEOUS_CONNECTIONS devices at once
        if (try_connect(&state.devices[i]))
            simultaneus_connections++;
        if (simultaneus_connections > SIMULTANEOUS_CONNECTIONS)
            break;
    }
    if (simultaneus_connections > 0)
    {
        g_debug(">>>>>> Started %i connection attempts", simultaneus_connections);
    }
    return TRUE;
}

static char client_id[META_LENGTH];

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

guint prop_changed;
guint iface_added;
guint iface_removed;
GMainLoop *loop;
static char mac_address[6];       // bytes
static char mac_address_text[13]; // string

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

    g_debug("Get mac address\n");
    get_mac_address(mac_address);
    mac_address_to_string(mac_address_text, sizeof(mac_address_text), mac_address);
    g_info("Local MAC address is: %s\n", mac_address_text);

    // Create a UDP listener for mesh messages about devices connected to other access points in same LAN
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
        g_warning("Not able to enable the adapter\n");
        goto fail;
    }

    rc = bluez_set_discovery_filter(conn);
    if (rc)
    {
        g_warning("Not able to set discovery filter\n");
        goto fail;
    }

    rc = bluez_adapter_call_method(conn, "StartDiscovery", NULL, NULL);
    if (rc)
    {
        g_warning("Not able to scan for new devices\n");
        goto fail;
    }
    g_info("Started discovery\n");

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
    g_timeout_add_seconds(10, try_connect_tick, loop);

    g_info(" ");
    g_info(" ");
    g_info(" ");

    display_state();

    g_info("Start main loop\n");

    g_main_loop_run(loop);

    g_info("END OF MAIN LOOP RUN\n");

    if (argc > 3)
    {
        rc = bluez_adapter_call_method(conn, "SetDiscoveryFilter", NULL, NULL);
        if (rc)
            g_warning("Not able to remove discovery filter\n");
    }

    rc = bluez_adapter_call_method(conn, "StopDiscovery", NULL, NULL);
    if (rc)
        g_warning("Not able to stop scanning\n");
    g_usleep(100);

    rc = bluez_adapter_set_property(conn, "Powered", g_variant_new("b", FALSE));
    if (rc)
        g_warning("Not able to disable the adapter\n");
fail:
    g_dbus_connection_signal_unsubscribe(conn, prop_changed);
    g_dbus_connection_signal_unsubscribe(conn, iface_added);
    g_dbus_connection_signal_unsubscribe(conn, iface_removed);
    g_dbus_connection_close_sync(conn, NULL, NULL);
    g_object_unref(conn);
    return 0;
}

void int_handler(int dummy)
{
    (void)dummy;

    g_main_loop_quit(loop);
    g_main_loop_unref(loop);

    g_dbus_connection_signal_unsubscribe(conn, prop_changed);
    g_dbus_connection_signal_unsubscribe(conn, iface_added);
    g_dbus_connection_signal_unsubscribe(conn, iface_removed);
    g_dbus_connection_close_sync(conn, NULL, NULL);
    g_object_unref(conn);

    exit_mqtt();

    close_socket_service(socket_service);

    pthread_mutex_destroy(&state.lock);

    g_info("Clean exit\n");

    exit(0);
}
