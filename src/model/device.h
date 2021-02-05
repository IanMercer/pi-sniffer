#ifndef DEVICE_H
#define DEVICE_H

#define G_LOG_USE_STRUCTURED 1

#include "kalman.h"
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/types.h>

typedef uint8_t u_int8_t;
typedef uint16_t u_int16_t;
typedef uint32_t u_int32_t;
typedef uint64_t u_int64_t;

// Max allowed devices on this sensor
#define N 2048

// How many closest observations to track
#define CLOSEST_N 8192

// Maximum number of access points (sensors) allowed
#define N_ACCESS_POINTS 256

#define PUBLIC_ADDRESS_TYPE 1
#define RANDOM_ADDRESS_TYPE 2

#define CATEGORY_UNKNOWN 0
#define CATEGORY_PHONE 1
// Wearable other than watch
#define CATEGORY_WEARABLE 2
#define CATEGORY_TABLET 3
#define CATEGORY_HEADPHONES 4
#define CATEGORY_COMPUTER 5
#define CATEGORY_TV 6
#define CATEGORY_FIXED 7
#define CATEGORY_BEACON 8
#define CATEGORY_CAR 9
#define CATEGORY_AUDIO_CARD 10
#define CATEGORY_LIGHTING 11
#define CATEGORY_SPRINKLERS 12
#define CATEGORY_POS 13
// fridge, bed, air filter, ...
#define CATEGORY_APPLIANCE 14
// NestCam etc.
#define CATEGORY_SECURITY 15
// Bikes, bike trainers, ergos, ...
#define CATEGORY_FITNESS 16    
#define CATEGORY_PRINTER 17
#define CATEGORY_SPEAKERS 18
#define CATEGORY_CAMERA 19
#define CATEGORY_WATCH 20     
#define CATEGORY_COVID 21

// Max allowed length of names and aliases (plus 1 for null)
#define NAME_LENGTH 21
// Max allowed length of metadata (client_id, description, platform, ..)
#define META_LENGTH 256

#define TRY_CONNECT_INTERVAL_S 2
#define TRY_CONNECT_ZERO 0
// Aim for 30s to allow connection 15 * 2
#define TRY_CONNECT_COMPLETE 15

// Names are either heuristics, actual or aliases
// Greater number wins over smaller number
// Used for tracking beacons and learning mode

enum name_type { 
    nt_initial = 0,          // ""
    nt_generic = 100,        // e.g. "Beacon"
    nt_manufacturer = 200,   // e.g. "Milwaukee"
    nt_device = 300,         // e.g. "iPhone"
    nt_known = 400,          // received from Bluetooth
    nt_alias = 500           // defined for system, e.g. tag names
};

/*
   Structure for tracking BLE devices in range
*/
struct Device
{
   int id;
   bool hidden;            // not seen by this access point (yet)
   uint8_t ttl;            // time to live count down during removal process
   char mac[18];           // mac address string
   int64_t mac64;          // mac address (moving off string)
   char name[NAME_LENGTH];
   enum name_type name_type;      // not set, heuristic, known, or alias
   char alias[NAME_LENGTH];
   int8_t address_type; // 0, 1, 2
   int8_t category;    // Reasoned guess at what kind of device it is
   bool paired;
   bool connected;
   bool trusted;
   uint32_t deviceclass; // https://www.bluetooth.com/specifications/assigned-numbers/Baseband/
   uint16_t appearance;
   int manufacturer_data_hash;
   int service_data_hash;
   int uuids_length;
   int uuid_hash;                 // Hash value of all UUIDs - may ditinguish devices
   int txpower;                   // TX Power
   time_t last_rssi;              // last time an RSSI was received. If gap > 0.5 hour, ignore initial point (dead letter post)
   struct Kalman filtered_distance;
   struct Kalman filtered_rssi;   // RSSI Kalman filter
   int raw_rssi;                  // RSSI last measurement
   time_t last_sent;
   float distance;
   struct Kalman kalman_interval; // Tracks time between RSSI events in order to detect large gaps
   time_t earliest;               // Earliest time seen, used to calculate overlap
   time_t latest;                 // Latest time seen by ANY sensor, used to calculate overlap
   int count;                     // Count how many times seen (ignore 1 offs)
   int8_t try_connect_state;      // Zero = never tried, 1..N-1 = Try in progress, N = Done
   int8_t try_connect_attempts;   // How many attempts have been made to connect
   // TODO: Collect data, SVM or trilateration TBD
   char *closest; // Closest access point name
   bool is_training_beacon;      // Is this a beacon with "Indoor Positioning" turned on?
};

/*
   Set name and name type if an improvement
*/
void set_name(struct Device* d, const char*value, enum name_type name_type);


/*
   AccessPoint is another instance of the app sending information to us, aka Sensor
*/
struct AccessPoint
{
   int id;                        // sequential ID
   char client_id[META_LENGTH];   // linux allows more, truncated
   char description[META_LENGTH]; // optional description for dashboard
   char platform[META_LENGTH];    // optional platform (e.g. Pi3, Pi4, ...) for dashboard

   // Put a device 1m away and measure the average RSSI
   int rssi_one_meter; // RSSI at one meter for test device (-64 dbm typical)
   // 2.0 to 4.0, lower for indoor or cluttered environments, default is 3.5
   float rssi_factor;            // Factor for RSSI to distance calculation 2.0-4.0 see README.md
   float people_distance;        // Counts as a person if under this range (meters)

   time_t last_seen;             // Time access point was last seen

   struct AccessPoint* next;     // Linked list
   int64_t sequence;             // Message sequence number so we can spot missing messages
};

/*
   A named beacon to track and report on
*/
struct Beacon
{
   char* name;
   int64_t mac64;
   char* alias;
   struct Beacon* next;
   // Which patch it is currently in (or NULL)
   struct patch* patch;
   time_t last_seen;
};

/*
   A third 'join' table joins devices and access points together
   We keep only recent observations
     - can throw out any observation which is further away in both time and distance
     - can throw out any observation which is more than 2 min older than most recent
     - can throw out any observation which is more than an hour old
     - MUST throw out any observation for a deleted device or access point
   Calculation of location:
     - ?
*/


int category_to_int(char *category);

char *category_from_int(uint8_t i);

char *device_to_json(struct AccessPoint *a, struct Device *device);

char *access_point_to_json(struct AccessPoint *a);

bool device_from_json(const char *json, struct AccessPoint *access_point, struct Device *device);

void merge(struct Device *local, struct Device *remote, char *access_name, bool safe);

/*
   How much data is sent over MQTT
*/
enum Verbosity
{
   Counts = 1,
   Distances = 2,
   Details = 3
};

struct ClosestTo
{
   // Which device
   int64_t device_64;
   // Which access point
   struct AccessPoint* access_point;
   // How far from the access point was it
   float distance;
   // category of the device
   int8_t category;
   // Earliest for this mac on this access point
   time_t earliest;
   // Latest for this mac on this access point
   time_t latest;

   // column for packing non-overlapping
   int column;

   // Remove this, cannot go retroactive on sending data
   // Superseded by another: i.e. access_id has seen this mac address
   // in a column more recently than this one that it superseeds
   int64_t supersededby;
   // mark and sweep flag
   bool mark;
   // count from access point
   int count;
   // for debugging
   char name [NAME_LENGTH];
   // name type
   enum name_type name_type;      // 'not set', 'heuristic', 'known', or 'alias'
   // for training location to patch mappings
   bool is_training_beacon;
   // address type is public or random
   int8_t addressType; // 0, 1, 2
};

#endif
