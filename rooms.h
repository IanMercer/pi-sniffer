#ifndef H_ROOMS
#define H_ROOMS

#include <stdlib.h>
#include <stdbool.h>
#include "device.h"
#include "accesspoints.h"

/*
    Rooms
*/

// A weight for a sensor in a room
struct weight
{
    const char* name;
    double weight;
    struct weight* next;  // next ptr
};

// An area 
struct area
{
    const char* category;       // category for Influx or other db
    const char* tags;           // CSV tags with no spaces
    struct area* next;          // next ptr
    double phone_total;         // how many phones
    double tablet_total;        // how many tablet
    double computer_total;      // how many computers
    double watch_total;         // how many watches
    double beacon_total;        // how many beacons
};

// An area with roughly equivalent distances from the sensors
struct room
{
    const char* name;
    struct area* area;          // Group with category and tags
    struct room* next;          // next ptr
    double room_score;          // calculated during scan, one ap
    double phone_total;         // how many phones
    double tablet_total;        // how many tablet
    double computer_total;      // how many computers
    double watch_total;         // how many watches
    double beacon_total;        // how many beacons
};

/*
  Get top k rooms sorted by total, return count maybe < k
*/
int top_k_by_room_score(struct room* result[], int k, struct room* room_list);

// Initialize the rooms structure on startup
void read_configuration_file(const char* path, struct AccessPoint** accesspoint_list, struct room** room_list, struct area** group_list, struct Beacon** beacon_list);

/*
   get or create a room and update any existing group also
*/
struct room* get_or_create_room(char* room_name, char* group_name, char* tags, struct room** rooms_list, struct area** groups_list);

// ------------------------------------------------------------------

struct beacon
{
    const char* name;
    const char* mac_address;
    int64_t mac64;
    const char* alias;
};


#endif