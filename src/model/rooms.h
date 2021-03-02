#ifndef H_ROOMS
#define H_ROOMS

#include <stdlib.h>
#include <stdbool.h>
#include "device.h"
#include "utility.h"
#include "accesspoints.h"
#include <string.h>
#include <math.h>

/*
    rooms.h defines patches, zones, areas, ...
*/

// An area: a group of rooms, maybe a shop, an administrative unit like a lab, or an entire building or 'Outside' 
struct group
{
    const char* name;           // group for reporting
    const char* tags;           // CSV tags with no spaces
    struct group* next;         // next ptr
};

// A patch: a roughly circular area about 3-5m in radius with similar distances from the sensors, the unit of measurement
// Patches are persistent, we never throw them away
// Recordings however that point to patches are ephemeral
struct patch
{
    const char* name;
    const char* room;           // Room that owns this patch
    struct group* group;        // Group/area that owns this patch (parent of room)
    struct patch* next;         // next ptr
    bool confirmed;             // confirmed came from recordings subdirectory not beacons subdirectory

    // MUTABLE DATA BELOW HERE
    double knn_score;           // calculated during scan, one ap
    double phone_total;         // how many phones
    double tablet_total;        // how many tablet
    double computer_total;      // how many computers
    double watch_total;         // how many watches
    double wearable_total;      // how many other wearables
    double covid_total;         // how many covid trackers
    double beacon_total;        // how many beacons
    double other_total;         // how many other
};

/*
  Get top k patches sorted by total, return count maybe < k
*/
int top_k_by_patch_score(struct patch* result[], int k, struct patch* patch_list);

// Initialize the configuration on startup
void read_configuration_file(const char* path, struct AccessPoint** accesspoint_list, struct Beacon** beacon_list);

/*
   get or create a patch and update any existing group also
*/
struct patch* get_or_create_patch(const char* patch_name, const char* room_name, const char* group_name, const char* tags, struct patch** patch_list, struct group** groups_list, bool confirmed);

// ------------------------------------------------------------------

/*
    summarize_by_room
*/
void summarize_by_room(struct patch* patches, struct summary** summary);

/*
    summarize_by_area
*/
void summarize_by_group(struct patch* patches, struct summary** summary);

#endif