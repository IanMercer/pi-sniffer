#ifndef H_ROOMS
#define H_ROOMS

#include <stdlib.h>
#include <stdbool.h>
#include "device.h"
#include "accesspoints.h"
#include <string.h>
#include <math.h>

/*
    rooms.h defines patches, zones, areas, ...
*/

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
    double wearable_total;      // how many wearable
    double beacon_total;        // how many beacons
};


// A patch with roughly equivalent distances from the sensors
struct patch
{
    const char* name;
    struct area* area;          // Area that owns this patch
    struct patch* next;         // next ptr
    double knn_score;          // calculated during scan, one ap
    double phone_total;         // how many phones
    double tablet_total;        // how many tablet
    double computer_total;      // how many computers
    double watch_total;         // how many watches
    double wearable_total;      // how many other wearables
    double beacon_total;        // how many beacons
};

/*
  Get top k patches sorted by total, return count maybe < k
*/
int top_k_by_patch_score(struct patch* result[], int k, struct patch* patch_list);

// Initialize the configuration on startup
void read_configuration_file(const char* path, struct AccessPoint** accesspoint_list, struct patch** room_list, struct area** group_list, struct Beacon** beacon_list);

/*
   get or create a patch and update any existing group also
*/
struct patch* get_or_create_patch(char* patch_name, char* group_name, char* tags, struct patch** patch_list, struct area** groups_list);

// ------------------------------------------------------------------

struct beacon
{
    const char* name;
    const char* mac_address;
    int64_t mac64;
    const char* alias;
};


#endif