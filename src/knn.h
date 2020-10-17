#ifndef knn_h
#define knn_h
/*
    K-Nearest Neighbors Classifier
*/

#include "device.h"
#include "rooms.h"
#include "accesspoints.h"

/*
   A recording of a training event from a known BLE device in a known location
*/
struct recording
{
    // A confirmed recording has been moved from the beacons directory to the recordings directory
    bool confirmed;
    char patch_name[META_LENGTH];
    float access_point_distances[N_ACCESS_POINTS];
    struct recording* next;
};


// SAVE AND LOAD recordings

bool record (const char* directory, const char* device_name, double access_distances[N_ACCESS_POINTS], struct AccessPoint* access_points, char* location);

bool read_observations (const char * dirname, struct AccessPoint* access_points, struct recording** recordings,
    struct patch** patch_list, struct area** areas_list, bool confirmed);

void free_list(struct recording** head);

// KNN CLASSIFIER

#define TOP_K_N 7

struct top_k
{
    float distance;
    char patch_name[META_LENGTH];
    bool used;
};

int k_nearest(struct recording* recordings, double* access_point_distances, struct AccessPoint* access_points, struct top_k* top_result, int top_count, bool confirmed);

#endif