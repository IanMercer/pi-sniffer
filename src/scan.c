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
#include "core/utility.h"
#ifdef MQTT
#include "mqtt_send.h"
#endif
#include "udp.h"
#include "kalman.h"
#include "device.h"
#include "bluetooth.h"
#include "heuristics.h"
#include "influx.h"
#include "rooms.h"
#include "accesspoints.h"
#include "closest.h"
#include "webhook.h"
#include "state.h"
#include "sniffer-generated.h"
#include "sniffer-dbus.h"

#define G_LOG_USE_STRUCTURED 1
#include <glib.h>
#include <gio/gio.h>
#include <stdbool.h>
#include <stdio.h>
#include <time.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <net/if.h>
#include <math.h>
#include <signal.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/types.h>
#include <string.h>
#include <udp.h>
#include <argp.h>
#include <unistd.h>    // gethostname

// For LED flash
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <cJSON.h>

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
  Use a known beacon name for a device if present
*/
void apply_known_beacons(struct OverallState* state, struct Device* device)
{
    if (device->name_type < nt_alias)
    {
        for (struct Beacon* b = state->beacons; b != NULL; b = b->next)
        {
            // Apply only ones without a hash
            if ((g_ascii_strcasecmp(b->name, device->name) == 0) || 
                ((b->mac64 != 0) && (b->mac64 == device->mac64)))
            {
                set_name(device, b->alias, nt_alias, "beacon");
                break;
            }
        }
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
    }
    state.n--;
}

#define MAX_TIME_AGO_COUNTING_MINUTES 5
#define MAX_TIME_AGO_LOGGING_MINUTES 10
#define MAX_TIME_AGO_CACHE 60

// Updated before any function that needs to calculate relative time
time_t now;

void report_devices_count()
{
    if (starting)
        return; // not during first 30s startup time

    // Initialize time
    time(&now);
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
            if (addr) {
                g_strlcpy(address, addr, 18);
            } else {
                g_warning("ERROR address from variant is null");
                return;
            }
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
            // These devices need to be removed from BLUEZ otherwise BLUEZ's cache gets huge!
            // g_warning("report_devices_internal, not an update, not found");
            //pretty_print2("Properties", properties, true);
            //g_debug("Remove %s, bluez get_devices called back, and not seen yet", address);
            bluez_remove_device(conn, address);
            return;
        }

        if (state.n == N)
        {
            g_warning("Error, array of devices is full");
            return;
        }

        // Grab the next empty item in the array
        existing = &state.devices[state.n++];
        existing->id = id_gen++;                // unique ID for each
        existing->ttl = 10;                     // arbitrary, set during countdown to ejection
        existing->hidden = false;               // we own this one
        g_strlcpy(existing->mac, address, 18);  // address
        existing->mac64 = mac_string_to_int_64(address);    // mac64

        // dummy struct filled with unmatched values
        existing->name[0] = '\0';
        existing->name_type = nt_initial;
        existing->alias[0] = '\0';
        existing->address_type = 0;
        existing->category = CATEGORY_UNKNOWN;
        existing->connected = FALSE;
        existing->trusted = FALSE;
        existing->paired = FALSE;
        existing->deviceclass = 0;
        existing->manufacturer_data_hash = 0;
        existing->service_data_hash = 0;
        existing->appearance = 0;
        existing->uuids_length = 0;
        existing->uuid_hash = 0;
        existing->txpower = 12;
        time(&existing->earliest);
        existing->count = 0;
        existing->try_connect_state = TRY_CONNECT_ZERO;
        existing->try_connect_attempts = 0;
        existing->is_training_beacon = false;
        existing->known_interval = 0;

        time(&existing->last_sent);
        time(&existing->last_rssi);

        // RSSI values are stored with kalman filtering
        kalman_initialize(&existing->filtered_rssi);
        existing->raw_rssi = -90;
        existing->distance = 10;

        existing->last_sent = existing->last_sent - 1000; //1s back so first RSSI goes through
        existing->last_rssi = existing->last_rssi - 1000;

        kalman_initialize(&existing->kalman_interval);

        g_info("Added device %i. %s", existing->id, address);
        apply_known_beacons(&state, existing);
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
            g_trace("Existing device %i. %s '%s' (%s)", existing->id, address, existing->name, existing->alias);
        }
    }

    // Mark the most recent time for this device (but not if it's a get all devices call)

    bool update_latest = isUpdate;   // May get cancelled if we see a DISCONNECTED message

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

            // -1 on name length because of a badly behaved beacon I have!
            if (strncmp(name, existing->name, NAME_LENGTH - 1) != 0)
            {
                // g_debug("  %s changed name '%s' -> '%s'\n", address, existing->name, name);
#ifdef MQTT
                if (state.network_up) send_to_mqtt_single(address, "name", name);
#endif
                send_distance = TRUE;
                set_name(existing, name, nt_known, "bt");

                apply_known_beacons(&state, existing);        // must apply beacons first to prevent hashing names
                apply_name_heuristics (existing, name);
            }

        }
        else if (strcmp(property_name, "Alias") == 0)
        {
            char *alias = g_variant_dup_string(prop_val, NULL);
            if (alias)
            {
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
        }
        else if (strcmp(property_name, "AddressType") == 0)
        {
            char *addressType = g_variant_dup_string(prop_val, NULL);
            int newAddressType = (g_strcmp0("public", addressType) == 0) ? PUBLIC_ADDRESS_TYPE : RANDOM_ADDRESS_TYPE;

            // Compare values and send
            if (existing->address_type != newAddressType)
            {
                existing->address_type = newAddressType;
                if (newAddressType == PUBLIC_ADDRESS_TYPE)
                {
                    g_trace("  %s Address type: '%s'", address, addressType);
                    // Not interested in random as most devices are random
                    apply_known_beacons(&state, existing);
                    apply_mac_address_heuristics(existing);
                }
#ifdef MQTT
                if (state.network_up) send_to_mqtt_single(address, "type", addressType);
#endif
            }
            else
            {
                // DEBUG g_print("  Address type unchanged\n");
            }
            g_free(addressType);
        }
        else if (strcmp(property_name, "RSSI") == 0 && !isUpdate)
        {
            // Ignore this, it isn't helpful when it's not an update
            // int16_t rssi = g_variant_get_int16(prop_val);
            // g_print("  %s RSSI repeat %i\n", address, rssi);
        }
        else if (strcmp(property_name, "RSSI") == 0)
        {
            int16_t rssi = g_variant_get_int16(prop_val);
            //send_to_mqtt_single_value(address, "rssi", rssi);

            time_t now;
            time(&now);

            // track gap between RSSI received events
            double delta_time_received = difftime(now, existing->last_rssi);
            time(&existing->last_rssi);
            existing->raw_rssi = rssi;  // unfiltered

            // If the gap is large we maybe lost this device and now it's back, so we can't assume continuity
            if (delta_time_received > 1000)
            {
                kalman_initialize(&existing->filtered_rssi);
            }

            double smoothed_rssi = kalman_update(&existing->filtered_rssi, rssi);

            // TODO: Different devices have different signal strengths
            // iPad, Apple TV, Samsung TV, ... seems to be particulary strong. Need to calibrate this and have
            // a per-device. PowerLevel is supposed to do this but it's not reliably sent.
            double rangefactor = 1.0;

            double exponent = ((state.local->rssi_one_meter  - smoothed_rssi) / (10.0 * state.local->rssi_factor));

            double distance = pow(10.0, exponent) * rangefactor;

            if (distance > 99.0) distance = 99.0;  // eliminate the ridiculous

            existing->distance = distance;

            // 10s with distance change of 1m triggers send
            // 1s with distance change of 10m triggers send

            //int delta_time_sent = difftime(now, existing->last_sent);
            //double delta_v = fabs(existing->distance - averaged);
            //double score = delta_v * delta_time_sent;

            //if (score > 10.0 || delta_time_sent > 10)
            {
                //g_print("  %s Will send rssi=%i dist=%.1fm, delta v=%.1fm t=%.0fs score=%.0f\n", address, rssi, averaged, delta_v, delta_time_sent, score);
                send_distance = TRUE;
                g_trace("  %s RSSI %i filtered=%.1f d=%.1fm", address, rssi, existing->filtered_rssi.current_estimate, distance);
            }
            //else
            //{
            //    g_trace("  %s Skip sending rssi=%i dist=%.1fm, delta v=%.1fm t=%is score=%.0f", address, rssi, averaged, delta_v, delta_time_sent, score);
            //}
        }
        else if (strcmp(property_name, "TxPower") == 0)
        {
            int16_t p = g_variant_get_int16(prop_val);
            if (p != existing->txpower)
            {
                g_trace("  %s TXPOWER has changed %i\n", address, p);
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
#ifdef MQTT
                if (state.verbosity >= Details) {
                    if (state.network_up) send_to_mqtt_single_value(address, "paired", paired ? 1 : 0);
                }
#endif
                existing->paired = paired;
            }
        }
        else if (strcmp(property_name, "Connected") == 0)
        {
            bool connected_device = g_variant_get_boolean(prop_val);
            if (existing->connected != connected_device)
            {
                if (connected_device)
                    g_debug("  %s Connected      ", address);
                else
                    g_debug("  %s Disconnected   ", address);
                    // And do not count this as an updated time
                    update_latest = FALSE;

#ifdef MQTT
                if (state.verbosity >= Details) {
                  if (state.network_up) send_to_mqtt_single_value(address, "connected", connected_device ? 1 : 0);
                }
#endif
                existing->connected = connected_device;
            }
        }
        else if (strcmp(property_name, "Trusted") == 0)
        {
            bool trusted = g_variant_get_boolean(prop_val);
            if (existing->trusted != trusted)
            {
                g_debug("  %s Trusted has changed       ", address);
#ifdef MQTT
                if (state.verbosity >= Details) {
                    if (state.network_up) send_to_mqtt_single_value(address, "trusted", trusted ? 1 : 0);
                }
#endif
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
            int existing_uuid_hash = existing->uuid_hash;

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

            if ((existing->uuid_hash != existing_uuid_hash))
            {
                char gatts[1024];
                gatts[0] = '\0';

                bool was = existing->is_training_beacon;
                existing->is_training_beacon = false;  // will set true if it's still there after (handles beacon stopping on iPhone nRF app)
                handle_uuids(existing, uuidArray, actualLength, gatts, sizeof(gatts));

                if (was && !existing->is_training_beacon){
                    g_warning("  %s (%s) is no longer transmitting an Indoor Positioning UUID", existing->mac, existing->name);
                }
                else if (!was && existing->is_training_beacon)
                {
                    g_warning("  %s (%s) is now transmitting an Indoor Positioning UUID", existing->mac, existing->name);
                }

                if (actualLength > 0)
                {
                    char **allocdata = g_malloc(actualLength * sizeof(char *)); // array of pointers to strings
                    memcpy(allocdata, uuidArray, actualLength * sizeof(char *));
                    g_info ("  %s UUIDs: %s", address, gatts);
#ifdef MQTT
                    if (state.network_up && state.verbosity >= Details) {
                        send_to_mqtt_uuids(address, "uuids", allocdata, actualLength);
                    }
#endif
                    g_free(allocdata); // no need to free the actual strings, that happens below
                }
                existing->uuids_length = actualLength;
            }
            else 
            {
               // g_debug("  %s UUIDs unchanged", existing->mac);
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
                g_debug("  %s Class has changed to 0x%.4x ", address, deviceclass);
#ifdef MQTT
                if (state.network_up) send_to_mqtt_single_value(address, "class", deviceclass);
#endif
                existing->deviceclass = deviceclass;

                handle_class(existing, deviceclass);
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
            handle_icon(existing, icon);

            g_free(icon);
        }
        else if (strcmp(property_name, "Appearance") == 0)
        { // type 'q' which is uint16
            uint16_t appearance = g_variant_get_uint16(prop_val);

            if (existing->appearance != appearance)
            {
                g_debug("  %s '%s' Appearance %i->%i", address, existing->name, existing->appearance, appearance);
#ifdef MQTT
                if (state.network_up) send_to_mqtt_single_value(address, "appearance", appearance);
#endif
                existing->appearance = appearance;
            }

            handle_appearance(existing, appearance);
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

                uint16_t hash;
                int actualLength;
                unsigned char *allocdata = read_byte_array(s_value, &actualLength, &hash);

                if (existing->service_data_hash != hash)
                {
                    //g_debug("  ServiceData has changed ");
                    pretty_print2_trace("  ServiceData", prop_val, TRUE); // a{qv}
                    // Sends the service GUID as a key in the JSON object
#ifdef MQTT
                    if (state.network_up && state.verbosity >= Details) {
                      send_to_mqtt_array(address, "ServiceData", service_guid, allocdata, actualLength);
                    }
#endif
                    existing->service_data_hash = hash;

                    // temp={p[16] - 10} brightness={p[17]} motioncount={p[19] + p[20] * 256} moving={p[22]}");
                    if (strcmp(service_guid, "000080e7-0000-1000-8000-00805f9b34fb") == 0)
                    { 
                        // Sensoro (used during testing, hence special treatement, TODO: Generalize)
                        if (strlen(existing->name) == 0) { 
                            g_strlcpy(existing->name, "Sensoro", NAME_LENGTH);
                        }
                        existing->category = CATEGORY_BEACON;

                        int battery = allocdata[14] + 256 * allocdata[15]; // ???
                        int p14 = allocdata[14];
                        int p15 = allocdata[15];
                        int temp = allocdata[16] - 10;
                        int brightness = allocdata[18];
                        int motionCount = allocdata[19] + allocdata[20] * 256;
                        int moving = allocdata[21];
                        g_debug("Sensoro battery=%i, p14=%i, p15=%i, temp=%i, brightness=%i, motionCount=%i, moving=%i\n", battery, p14, p15, temp, brightness, motionCount, moving);
#ifdef MQTT
                        send_to_mqtt_single_value(address, "temperature", temp);
                        send_to_mqtt_single_value(address, "brightness", brightness);
                        send_to_mqtt_single_value(address, "motionCount", motionCount);
                        send_to_mqtt_single_value(address, "moving", moving);
#endif
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

            // First calculate the sum of all the manufacturerdata values to see if they have changed
            uint16_t hash = 0;

            g_variant_iter_init(&i, prop_val);
            while (g_variant_iter_next(&i, "{qv}", &manufacturer, &s_value))
            {
                int actualLength;
                unsigned char *allocdata = read_byte_array(s_value, &actualLength, &hash);
                g_variant_unref(s_value);
                g_free(allocdata);
            }

            // Now read it again if it changed
            if (existing->manufacturer_data_hash != hash)
            {
                existing->manufacturer_data_hash = hash;

                g_variant_iter_init(&i, prop_val);
                while (g_variant_iter_next(&i, "{qv}", &manufacturer, &s_value))
                {
                    uint16_t hash_for_one = 0;
                    int actualLength;
                    unsigned char *allocdata = read_byte_array(s_value, &actualLength, &hash_for_one);

                    if (manufacturer == 0x4c && allocdata[0] == 0x02){
                        g_debug("  %s iBeacon  ", address);
                    } else if (manufacturer == 0x4c){
                        g_trace("  %s Manufacturer Apple  ", address);
                    } else {
                        g_trace("  %s Manufacturer 0x%4x  ", address, manufacturer);
                        }

                    // TODO: If detailed logging
                    pretty_print2_trace("  ManufacturerData", s_value, TRUE); // a{qv}

    #ifdef MQTT
                    // Need to send actual manufacturer number not always 76 here TODO
                    if (state.verbosity >= Details) {
                    send_to_mqtt_array(address, "manufacturerdata", "76", allocdata, actualLength);
                        }
    #endif

                    if (existing->distance > 0)
                    {
                        // And repeat the RSSI value every time someone locks or unlocks their phone
                        // Even if the change notification did not include an updated RSSI
                        //g_print("  %s Will resend distance\n", address);
                        send_distance = TRUE;
                    }

                    handle_manufacturer(existing, manufacturer, allocdata);

                    g_variant_unref(s_value);
                    g_free(allocdata);
                }
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
            g_debug("ERROR Unknown property: '%s' %s", property_name, type);
        }

        //g_print("un_ref prop_val\n");
        g_variant_unref(prop_val);
    }

    if (update_latest)
    {
        // DO NOT DO THIS IF THE MESSAGE IS "DISCONNECTED" AS THAT IS SENT AFTER IT HAS GONE!
        time(&existing->latest_local);
        time(&existing->latest_any);
        existing->count++;
    }

    if (starting && send_distance)
    {
        g_trace("Skip sending, starting");
    }
    else
    {
        if (send_distance)
        {
            //g_debug("  **** Send distance %6.3f                        ", existing->distance);
#ifdef MQTT
            if (state.network_up && state.verbosity >= Distances){
              send_to_mqtt_single_float(address, "distance", existing->distance);
            }
#endif
            time(&existing->last_sent);
            // Broadcast what we know about the device to all other listeners
            // only send when isUpdate is set, i.e. not for get all devices requests
            if (isUpdate)
            {
                //pack_columns();
                update_closest(&state, existing);
                send_device_udp(&state, existing);
            }
        }
    }

    report_devices_count();
}

/*
  Report device with lock on data structures
  NOTE: Frees the address when done
*/
static void report_device(struct OverallState *state, GVariant *properties, char *known_address, bool isUpdate)
{
    pthread_mutex_lock(&state->lock);
    report_device_internal(properties, known_address, isUpdate);
    pthread_mutex_unlock(&state->lock);
}

/*
   BLUETOOTH DEVICE APPEARED
*/
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
            //pretty_print("  Gatt service = ", properties);
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
        else if (g_ascii_strcasecmp(interface_name, "org.bluez.Network1") == 0)
        {
            pretty_print("  Network = ", properties);
        }
        else if (g_ascii_strcasecmp(interface_name, "org.bluez.MediaControl1") == 0)
        {
            pretty_print("  Media control = ", properties);
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

/*
    BLUETOOTH DEVICE DISAPPEARED
*/
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
                // This event doesn't seem to correspond to reality
                // g_warning("%s Device removed by BLUEZ", address);
                // DEBUG g_print("Device %s removed (by bluez) ignoring this\n", address);
                // do nothing ... report_device_disconnected_to_MQTT(address);
            }
        }
        g_free(interface_name);
    }
    g_variant_iter_free(interface_iter);
}

/*
    BLUETOOTH ADAPTER CHANGED
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
#ifdef MQTT
    mqtt_sync();
#endif
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

    //g_debug("get_managed_objects callback");

    result = g_dbus_connection_call_finish(conn, res, &error);
    if ((result == NULL) || error)
    {
        g_info("Unable to get result for GetManagedObjects\n");
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

    //g_debug("get_managed_objects");

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

    int delta_time = difftime(now, existing->latest_any);

    // 3 min for a regular device
    int max_time_ago_seconds = 3 * 60;

    // 5 min if we got to know it
    if (existing->category != CATEGORY_UNKNOWN){
        max_time_ago_seconds = 5 * 60; // was 10 * 60;
    }

    // 12 min for ibeacons
    if (existing->category == CATEGORY_BEACON){
        max_time_ago_seconds = 12 * 60;
    }

    // 1 hour upper limit
    if (max_time_ago_seconds > 60 * MAX_TIME_AGO_CACHE)
    {
        max_time_ago_seconds = 60 * MAX_TIME_AGO_CACHE;
    }

    if (delta_time < max_time_ago_seconds){
        existing->ttl = 10;
    }
    else {
        // Count down to removal completely
        existing->ttl = existing->ttl - 1;

        if (existing->ttl == 9)
        {
            //g_debug("%s '%s' BLUEZ Cache remove count=%i dt=%.1fmin dist=%.1fm", existing->mac, existing->name, existing->count, delta_time/60.0, existing->distance);
            // And so when this device reconnects we get a proper reconnect message and so that BlueZ doesn't fill up a huge
            // cache of iOS devices that have passed by or changed mac address
            bluez_remove_device(conn, existing->mac);

            // It might come right back ... or it might be truly gone
            return FALSE;
        }
        else if (existing->ttl == 5)    // 4x5s later = 20s later
        {
            //g_info("%s '%s' LOCAL Cache remove count=%i dt=%.1fmin dist=%.1fm", existing->mac, existing->name, existing->count, delta_time/60.0, existing->distance);
            return TRUE;
        }

    }


    return FALSE;
}

int clear_cache(void *parameters)
{
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


void dump_device(struct OverallState* state, struct Device *d)
{
    // Ignore any that have not been seen recently
    //double delta_time = difftime(now, a->latest);
    //if (delta_time > MAX_TIME_AGO_LOGGING_MINUTES * 60) return;

    char *addressType = d->address_type == PUBLIC_ADDRESS_TYPE ? "*" : d->address_type == RANDOM_ADDRESS_TYPE ? " " : "-";
    char *connectState = d->try_connect_state == TRY_CONNECT_COMPLETE ? " " :
                         d->try_connect_state == TRY_CONNECT_ZERO ? "z" : "i"
                         ;
    char *connectCount = d->try_connect_state == TRY_CONNECT_COMPLETE ? " " :
                         d->try_connect_attempts == 0 ? "0" :
                         d->try_connect_attempts == 1 ? "1" :
                         d->try_connect_attempts == 2 ? "2" :
                         d->try_connect_attempts == 2 ? "3" :
                         d->try_connect_attempts == 2 ? "4" : "X";

    char *category = category_from_int(d->category);

    float closest_dist = NAN;
    char *closest_ap = "unknown";
    struct ClosestTo *closest = get_closest_64(state, d->mac64);
    if (closest)
    {
        closest_dist = closest->distance;
        struct AccessPoint *ap = closest->access_point;
        if (ap)
        {
            closest_ap = ap->short_client_id;
        }
    }

    g_info("%4i %s%s%s%s %4i %5.1fm  %6li-%6li %20s %8.8s %5.1fm %2i %s", d->id % 10000, d->mac, addressType,
        connectState, connectCount, 
        d->count, d->distance, (d->earliest - state->started), (d->latest_local - state->started), d->name, closest_ap, closest_dist, d->txpower, category);
}


bool webhook_is_configured()
{
    if (state.webhook_domain == NULL) return FALSE;
    if (state.webhook_path == NULL) return FALSE;
    if (strlen(state.webhook_domain)==0) return FALSE;  
    return TRUE;
}

bool influx_is_configured()
{
    if (state.influx_server == NULL) return FALSE;
    if (strlen(state.influx_server)==0) return FALSE;
    return TRUE;
}

/*
    Report summaries to HttpPost
*/
int report_to_http_post_tick()
{
    if (!webhook_is_configured()) return FALSE;
    post_to_webhook(&state);
    return TRUE;
}

/*
* Ensure that Bluetooth is powered on and in discovery mode
*/
int ensure_bluetooth_tick()
{
    int rc = bluez_adapter_set_property(conn, "Powered", g_variant_new("b", TRUE));
    if (rc)
    {
        g_warning("Not able to enable the adapter");
        return TRUE;
    }

    rc = bluez_set_discovery_filter(conn);
    if (rc)
    {
        g_warning("Not able to set discovery filter");
        return TRUE;
    }

    rc = bluez_adapter_call_method(conn, "StartDiscovery", NULL, NULL);
    if (rc)
    {
        g_warning("Not able to scan for new devices");
        return TRUE;
    }

    return TRUE;
}


/*
    Report summaries to InfluxDB
*/
int report_to_influx_tick(struct OverallState* state)
{
    if (!influx_is_configured()) return FALSE;

    char body[4096];
    body[0] = '\0';

    bool ok = TRUE;

    time_t now = time(0);

    struct summary* summary = NULL;
    summarize_by_room(state->patches, &summary);

    // Clean out a stuck signal on InfluxDB
    //ok = ok && append_influx_line(body, sizeof(body), "<Group>", "room=<room>", "beacon=0.0,computer=0.0,phone=0.0,tablet=0.0,watch=0.0,wear=0.0", now);

    for (struct summary* s = summary; s != NULL; s = s->next)
    {
        char tags[120];
        char field[120];

        snprintf(field, sizeof(field), "beacon=%.1f,computer=%.1f,phone=%.1f,tablet=%.1f,watch=%.1f,wear=%.1f,cov=%.1f,other=%.1f",
            s->beacon_total, s->computer_total, s->phone_total, s->tablet_total, s->watch_total, s->wearable_total, s->covid_total, s->other_total);

        snprintf(tags, sizeof(tags), "room=%s", s->category);

        ok = ok && append_influx_line(body, sizeof(body), s->extra, tags, field, now);

        if (strlen(body) + 5 * 100 > sizeof(body)){
            //g_debug("%s", body);
            post_to_influx(state, body, strlen(body));
            body[0] = '\0';
        }

        //g_debug("INFLUX: %s %s %s", s->extra, tags, field);
    }

    free_summary(&summary);

    if (strlen(body) > 0)
    {
        //g_debug("%s", body);
        post_to_influx(state, body, strlen(body));
    }

    if (!ok)
    {
        g_warning("Influx messages was truncated");
    }
    return TRUE;
}

/*
    COMMUNICATION WITH DISPLAYS OVER UDP
    NB this needs to a smaller message to go over UDP
*/
void send_to_udp_display(struct OverallState *state)
{
    if (state->network_up && state->udp_sign_port > 0)
    {
        cJSON *jobject = cJSON_CreateObject();

        struct summary* summary = NULL;

        summarize_by_group(state->patches, &summary);

        for (struct summary* s=summary; s!=NULL; s=s->next)
        {
            cJSON_AddRounded(jobject, s->category, s->phone_total);
        }
        free_summary(&summary);

        // Add metadata for the sign to consume (so that signage can be adjusted remotely)
        // TODO: More levels etc. settable remotely
        cJSON_AddRounded(jobject, "sf", state->udp_scale_factor);

        char* json = cJSON_PrintUnformatted(jobject);
        cJSON_Delete(jobject);

        //g_warning("%s", json);

        // +1 for the NULL terminator
        udp_send(state->udp_sign_port, json, strlen(json)+1);        
    }
}


static int report_count = 0;

/*
    Report access point counts to InfluxDB, Web, UDP
    Called every 20s but Web hook only called once a minute and Influx once every five minutes
*/
int report_counts(void *parameters)
{
    (void)parameters;

    if (!state.network_up) return TRUE;
    if (starting) return TRUE;

    report_count++;

    if (state.isMain != FALSE)   // handle 1 or any other number
    {
        time(&now);
        // Set JSON for all ways to receive it (GET, POST, INFLUX, MQTT)
        bool changed = print_counts_by_closest(&state);

        // Send dbus always, receiver handles throttling
        if (state.json == NULL)
        {
            g_debug("Skipped send, no json");
        }
        else if (!changed)
        {
            g_debug("Skipped send, json is unchanged");
        }
        else 
        {
            g_info("Send DBus notification %s", changed?"changed":"unchanged");
            pi_sniffer_emit_notification (state.proxy, state.json);
        }

        int influx_seconds = difftime(now, state.influx_last_sent);

        if (influx_seconds > state.influx_max_period_seconds || 
            (changed && (influx_seconds > state.influx_min_period_seconds)))
        {
            g_debug("Sending to influx %is since last", influx_seconds);
            state.influx_last_sent = now;
            report_to_influx_tick(&state);
        }

        int webhook_seconds = difftime(now, state.webhook_last_sent);

        if (webhook_is_configured() &&
            (webhook_seconds > state.webhook_max_period_seconds || 
            (changed && (webhook_seconds > state.webhook_min_period_seconds))))
        {
            g_debug("Sending to webhook %is since last", webhook_seconds);
            state.webhook_last_sent = now;
            report_to_http_post_tick();
        }

        // Every 20s
        send_to_udp_display(&state);
        return TRUE;
    }
    else
    {
        g_warning("Not configured as MAIN, not sending updates to DBUS");
        return FALSE;  // remove from loop
    }
}

/*
    Print access point metadata
*/
int print_access_points_tick(void *parameters)
{
    (void)parameters;
    print_access_points(state.access_points);

    // Only the main unit does this
    if (state.isMain) 
    {
        g_info(" ");
        print_min_distance_matrix(&state);
        g_info(" ");
    }

    // Update internal temperature and other useful statistics (TODO)
    state.local->internal_temperature = get_internal_temp();

    // And send access point to everyone over UDP - so that even if no activity everyone gets a list of active access points
    send_access_point_udp(&state);

    // Update flashing LED
    int countaps = 0;
    for (struct AccessPoint* ap = state.access_points; ap != NULL; ap=ap->next){ countaps ++; }
    state.led_flash_count = countaps;

    return TRUE;
}


void print_log_table()
{
    g_info("-----------------------------------------------------------------------------------------------------------");
    g_info("Id   Address             Count   Dist    First  Last                 Name         Closest Tx Category");
    g_info("-----------------------------------------------------------------------------------------------------------");

    for (int i = 0; i < state.n; i++)
    {
        dump_device(&state, &state.devices[i]);
    }
    g_info("-----------------------------------------------------------------------------------------------------------");
}

/*
    Dump all devices present and reboot as necessary
*/
int dump_all_devices_tick(void *parameters)
{
    starting = FALSE;
    state.network_up = is_any_interface_up();

    (void)parameters; // not used
    if (starting)
        return TRUE; // not during first 30s startup time
    if (!logTable)
        return TRUE; // no changes since last time
    logTable = FALSE;
    time(&now);

    // TODO: Restrict logging this
    print_log_table();

    unsigned long total_minutes = (now - state.started) / 60; // minutes
    unsigned int minutes = total_minutes % 60;
    unsigned int hours = (total_minutes / 60) % 24;
    unsigned int days = (total_minutes) / 60 / 24;

    const char* connected = is_any_interface_up() ? "" : "NETWORK DOWN ";

    if (days > 1)
        g_info("Uptime: %i days %02i:%02i %s", days, hours, minutes, connected);
    else if (days == 1)
        g_info("Uptime: 1 day %02i:%02i %s", hours, minutes, connected);
    else
        g_info("Uptime: %02i:%02i %s", hours, minutes, connected);

    // Bluez eventually seems to stop sending us data, so for now, just restart every few hours
    struct tm *local_time = localtime( &now );
    //g_debug("Current local time and date: %s", asctime(local_time));

    if ((state.reboot_hour) > 0 &&                   // reboot hour is set
        (total_minutes > 60L) &&                     // and we didn't already reboot in this hour
        (local_time->tm_hour == state.reboot_hour))  // and it's the right hour
    {
        g_warning("*** RESTARTING AFTER %i HOURS RUNNING", hours);
        system("reboot");
        //int_handler(0);
    }

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
                //g_info(">>>>> Disconnect from %i. %s\n", a->id, a->mac);
                bluez_adapter_disconnect_device(conn, a->mac);
            }
            else if (a->category == CATEGORY_UNKNOWN)
            {
                // Failed to get enough data from connection or did not connect
                //g_info(">>>>> Failed to connect to %i. %s\n", a->id, a->mac);
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
    // If we already have a category and a non-temporary name
    if (a->category != CATEGORY_UNKNOWN && a->name_type >= nt_known)
    {
        a->try_connect_state = TRY_CONNECT_COMPLETE;
        return FALSE; // already has a category and a name
    }

    // Don't attempt connection until a device is close enough, or has been seen enough
    // otherwise likely to fail for a transient device at 12.0+m
    if (a->count == 1 && a->distance > 18.0)
        return FALSE;

    // At some point we just give up
    if (a->try_connect_attempts > 4) return FALSE;

    // Every 16 counts we try again if not already connected
    if (a->try_connect_state == TRY_CONNECT_COMPLETE && 
        a->count > a->try_connect_attempts * 16) {
            a->try_connect_state = TRY_CONNECT_ZERO;
        }

    // If not idle, already in process of trying to connect
    if (a->try_connect_state != TRY_CONNECT_ZERO) return FALSE;

    // Passed all pre-conditions, go ahead and start a connection attempt
    a->try_connect_state = 1;
    a->try_connect_attempts++;
    // Try forcing a connect to get a full dump from the device
    //g_info(">>>>>> Connect to %i. %s  attempt=%i", a->id, a->mac, a->try_connect_attempts);
    bluez_adapter_connect_device(conn, a->mac);
    return TRUE;
}

static int led_state = 0;
// TODO: Suppress this on non-Pi platforms

int flash_led(void *parameters)
{
    // Only when running on Raspberry Pi
    if (!string_contains_insensitive(state.local->platform, "ARM")) return FALSE;

    (void)parameters;                // not used
    // Every other cycle turn the LED on or off
    if (led_state < state.led_flash_count * 2) {
        char d =  (led_state & 1) == 0 ? '1' : '0';
        int fd = open("/sys/class/leds/led0/brightness", O_WRONLY);
        write (fd, &d, 1);
        close(fd);
    }

    led_state = (led_state + 1);
    // 4s past end, restart
    if (led_state > state.led_flash_count * 2 + 4){
        led_state = 0;
    }

    return TRUE;
}


#define SIMULTANEOUS_CONNECTIONS 5

int try_connect_tick(void *parameters)
{
    (void)parameters;                // not used
    int simultaneus_connections = 0; // assumes none left from previous tick
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
        //g_debug(">>>>>> Started %i connection attempts", simultaneus_connections);
    }
    return TRUE;
}


guint prop_changed;
guint settings_prop_changed;
guint iface_added;
guint iface_removed;
GMainLoop *loop;

GCancellable *socket_service;

/*
    incoming request on DBUS, probably from web CGI
*/
static gboolean on_handle_status_request (piSniffer *interface,
                       GDBusMethodInvocation  *invocation,
                       gpointer                user_data)
{
    struct OverallState* state = (struct OverallState*)user_data;
    gchar *response;
    state->web_polling = TRUE;
    response = state->json;
    if (response == NULL) response = "NOT READY";
    pi_sniffer_complete_status(interface, invocation, response);
    // g_free (response);
    return TRUE;
}

/*
    incoming request on DBUS, probably from Azure handler, update settings
*/
static gboolean on_handle_settings_request (piSniffer *interface,
                       GDBusMethodInvocation  *invocation,
                       const gchar            *jsonSettings,
                       gpointer                user_data)
{
    struct OverallState* state = (struct OverallState*)user_data;

    // Update settings from json blob passed in

    g_info("*** Received settings update %s", jsonSettings);

    cJSON *json = cJSON_Parse(jsonSettings);
    if (json == NULL)
    {
        const char *error_ptr = cJSON_GetErrorPtr();
        if (error_ptr != NULL)
        {
            g_warning("Error parsing json settings update before: %s", error_ptr);
        }
        return TRUE;  // needed to keep running, right?
    }

    // {"Thresholds":[{"name":"Inside","on":10,"off":5}],
    //  "MaxGapSeconds":1440000,
    //  "MinGapSeconds":60,
    //  "BoostForSeconds":0,
    //  "RoomGroups":[
    //    {"Name":"Near","Rooms":[{"Name":"Near","Patches":[{"Name":"Near","Observations":[{"ap":"self","d":2.0,"u":1.0}]}]}]},
    //    {"Name":"Medium","Rooms":[{"Name":"Medium","Patches":[{"Name":"Medium","Observations":[{"ap":"self","d":2.0,"u":1.0}]}]}]},
    //    {"Name":"Far","Rooms":[{"Name":"Far","Patches":[{"Name":"Far","Observations":[{"ap":"self","d":2.0,"u":1.0}]}]}]}]}

    cJSON* thresholds = cJSON_GetObjectItemCaseSensitive(json, "Thresholds");
    if (cJSON_IsObject(thresholds))
    {
        // Read each threshold, and set it on the state object
        g_info("Setting threshold object on state (TODO) %s", state->local->client_id);
    }

    cJSON* roomGroups = cJSON_GetObjectItemCaseSensitive(json, "RoomGroups");
    if (!cJSON_IsArray(roomGroups)){
        g_warning("Could not parse RoomGroups[]");
        // non-fatal
    }
    else
    {
        cJSON* room_group = NULL;
        cJSON_ArrayForEach(room_group, roomGroups)
        {
            cJSON* room_group_name = cJSON_GetObjectItemCaseSensitive(room_group, "Name");
            cJSON* rooms = cJSON_GetObjectItemCaseSensitive(room_group, "Rooms");
            if (cJSON_IsString(room_group_name) && cJSON_IsArray(rooms))
            {
                // "Rooms":[{"Name":"Near","Patches":[{"Name":"Near","Observations":[{"ap":"self","d":2.0,"u":1.0}]}]}]
                g_debug("Room group %s", room_group_name->valuestring);

                cJSON* room = NULL;
                cJSON_ArrayForEach(room, rooms)
                {
                    cJSON* room_name = cJSON_GetObjectItemCaseSensitive(room, "Name");
                    cJSON* patches = cJSON_GetObjectItemCaseSensitive(room, "Patches");
                    if (cJSON_IsString(room_name) && cJSON_IsArray(patches))
                    {
                        g_debug("  Room %s", room_name->valuestring);

                        cJSON* patch = NULL;
                        cJSON_ArrayForEach(patch, patches)
                        {
                            cJSON* patch_name = cJSON_GetObjectItemCaseSensitive(patch, "Name");
                            cJSON* observations = cJSON_GetObjectItemCaseSensitive(patch, "Observations");

                            if (cJSON_IsString(patch_name) && cJSON_IsArray(observations))
                            {
                                g_debug("    Patch %s", patch_name->valuestring);

                                cJSON* observation = NULL;
                                cJSON_ArrayForEach(observation, observations)
                                {
                                    cJSON* ap_name = cJSON_GetObjectItemCaseSensitive(observation, "ap");
                                    cJSON* d = cJSON_GetObjectItemCaseSensitive(observation, "d"); // distance
                                    cJSON* u = cJSON_GetObjectItemCaseSensitive(observation, "u"); // count in recent period

                                    if (cJSON_IsString(ap_name) && cJSON_IsNumber(d) && cJSON_IsNumber(u))
                                    {
                                        g_debug("      Observation %s %f %f", ap_name->string, d->valuedouble, u->valuedouble);
                                    }
                                }
                            }
                        }
                    }
                    // 
                }
                // // struct Beacon* beacon = malloc(sizeof(struct Beacon));
                // // beacon->name = strdup(name->valuestring);
                // // beacon->mac64 = mac_string_to_int_64(mac->valuestring);
                // // beacon->alias = strdup(alias->valuestring);
                // // beacon->last_seen = 0;
                // // beacon->patch = NULL;
                // // beacon->next = *beacon_list;
                // // *beacon_list = beacon;
                // // //g_warning("Added beacon `%s` = '%s' to list", beacon->name, beacon->alias);
            }
            else
            {
                g_warning("Missing name or rooms array on room group");
            }
        }
    }

    pi_sniffer_complete_settings(interface, invocation);  // no value to pass back
    return TRUE;
}


/*
   DBUS name acquired
*/
static void on_name_acquired (GDBusConnection *connection, const gchar *name, gpointer user_data)
{
    (void)connection;
    (void)user_data;
    g_warning("DBUS name acquired '%s'", name);
}

/*
   DBUS name lost
   TODO: Should we restart if this happens?
*/
static void on_name_lost (GDBusConnection *connection, const gchar *name, gpointer user_data)
{
    (void)connection;
    (void)user_data;
    g_warning("DBUS name lost '%s'", name);
}

// Log_handler filters messages according to logging level set for application
void custom_log_handler (const gchar *log_domain, GLogLevelFlags log_level, const gchar *message, gpointer user_data)
{
    gint debug_level = GPOINTER_TO_INT (user_data);

    /* filter out messages depending on debugging level - actually a flags enum but > works */
    if (log_level > debug_level) 
    {
        return;
    }
    //g_print("%i <= %i", log_level, debug_level);
    g_log_default_handler(log_domain, log_level, message, user_data);
}

/*
    MAIN
*/
int main(int argc, char **argv)
{
    (void)argv;
    int rc;

    int debug_level = 0;
    get_int_env("DEBUG_LEVEL", &debug_level, G_LOG_LEVEL_INFO);
    //g_log_set_handler ("Sniffer", G_LOG_LEVEL_MASK, custom_log_handler, GINT_TO_POINTER (debug_level));

    g_debug("DEBUG MESSAGE");
    g_info("INFO MESSAGE");
    g_warning("WARNING MESSAGE");

    if (pthread_mutex_init(&state.lock, NULL) != 0)
    {
        g_error("mutex init failed");
        exit(123);
    }

    g_info("initialize_state()");
    initialize_state(&state);

    display_state(&state);

    signal(SIGINT, int_handler);
    signal(SIGTERM, int_handler);

    if (argc < 1)
    {
        g_info("Bluetooth scanner\n");
        g_info("   scan\n");
        g_info("   but first set all the environment variables according to README.md");
        g_info("For Azure you must use ssl:// and :8833\n");
        return -1;
    }

    // Create a UDP listener for mesh messages about devices connected to other access points in same LAN
    g_info("create_socket_service()");

    socket_service = create_socket_service(&state);

    conn = g_bus_get_sync(G_BUS_TYPE_SYSTEM, NULL, NULL);
    if (conn == NULL)
    {
        g_warning("Not able to get connection to system bus\n");
        return 1;
    }

    g_info("\n\nStarting\n\n");

    g_info("calling g_bus_own_name");

    piSniffer * sniffer = pi_sniffer_skeleton_new ();

    state.proxy = sniffer;
    
// TODO: Sample of setting properties (none of these are used now, single JSON blob from remote service)
//    pi_sniffer_set_distance_limit(sniffer, 7.5);
//    pi_sniffer_get_max_interval(sniffer);
//    pi_sniffer_get_min_interval(sniffer);

    // DBus - CGI or other app is asking for a status (polling)
    g_signal_connect(sniffer, "handle-status", G_CALLBACK(on_handle_status_request), &state);

    // DBus - Azure communicator or other app is updating settings
    g_signal_connect(sniffer, "handle-settings", G_CALLBACK(on_handle_settings_request), &state);

    // DBus advertise the interface we support
    GError* error = NULL;
    if (!g_dbus_interface_skeleton_export (G_DBUS_INTERFACE_SKELETON (sniffer),
                                         conn,
                                         "/com/signswift/sniffer",
                                         &error))
    {
        g_warning("Failed to export skeleton");
    }
    else
    {
        g_info("Exported skeleton, DBUS ready!");
    }

    guint name_connection_id = g_bus_own_name_on_connection(conn,
        "com.signswift.sniffer",
        G_BUS_NAME_OWNER_FLAGS_ALLOW_REPLACEMENT,
        on_name_acquired,
        on_name_lost,
        NULL,
        NULL);


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

    ensure_bluetooth_tick(&state);
    g_info("Started discovery");

#ifdef MQTT
    prepare_mqtt(state.mqtt_server, state.mqtt_topic, 
        state.local->client_id, 
        "",  // no suffix on client id for MQTT
        state.mqtt_username, state.mqtt_password);
#endif
    // Periodically ask Bluez for every device including ones that are long departed
    // but only do updates to devices we have seen, do no not create a device for each
    // as there are too many and most are old, random mac addresses
    g_timeout_add_seconds(60, get_managed_objects, loop);

    // MQTT send
    g_timeout_add_seconds(5, mqtt_refresh, loop);

    // Every 5s look see if any records have expired and should be removed
    g_timeout_add_seconds(5, clear_cache, loop);

    // Every 5 min dump all devices
    // Also clear starting flag
    g_timeout_add_seconds(5 * 60, dump_all_devices_tick, loop);

    // Every 30s report counts
    g_timeout_add_seconds(20, report_counts, loop);

    // Every 5 min dump access point metadata and matrix
    g_timeout_add_seconds(301, print_access_points_tick, loop);

    // Every 10min make sure Bluetooth is in scan mode
    g_timeout_add_seconds(603, ensure_bluetooth_tick, loop);

    // Every 2s see if any unnamed device is ready to be connected
    g_timeout_add_seconds(TRY_CONNECT_INTERVAL_S, try_connect_tick, loop);

    // Flash the led N times for N people present
    g_timeout_add(300, flash_led, loop);

    g_info(" ");
    g_info(" ");
    g_info(" ");

    g_info("Start main loop");

    g_main_loop_run(loop);

    g_info("END OF MAIN LOOP RUN");

    if (argc > 3)
    {
        rc = bluez_adapter_call_method(conn, "SetDiscoveryFilter", NULL, NULL);
        if (rc)
            g_warning("Not able to remove discovery filter");
    }

    rc = bluez_adapter_call_method(conn, "StopDiscovery", NULL, NULL);
    if (rc)
        g_warning("Not able to stop scanning");
    g_usleep(100);

    g_bus_unown_name(name_connection_id);

    g_dbus_connection_signal_unsubscribe(conn, prop_changed);
    g_dbus_connection_signal_unsubscribe(conn, settings_prop_changed);
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
    g_dbus_connection_signal_unsubscribe(conn, settings_prop_changed);
    g_dbus_connection_signal_unsubscribe(conn, iface_added);
    g_dbus_connection_signal_unsubscribe(conn, iface_removed);
    g_dbus_connection_close_sync(conn, NULL, NULL);
    g_object_unref(conn);

#ifdef MQTT
    exit_mqtt();
#endif
    close_socket_service(socket_service);

    pthread_mutex_destroy(&state.lock);

    g_info("Clean exit\n");

    exit(0);
}
