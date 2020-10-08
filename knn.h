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
    char room_name[META_LENGTH];
    float access_point_distances[N_ACCESS_POINTS];
    struct recording* next;
};


// SAVE AND LOAD recordings

bool record (char* filename, double access_distances[N_ACCESS_POINTS], struct AccessPoint* access_points, char* location);

// KNN CLASSIFIER

#define TOP_K_N 7

struct top_k
{
    float distance;
    char room_name[META_LENGTH];
};

char* classify (float* access_point_distances, struct AccessPoint* access_points, struct room* rooms, char* location);

const char* k_nearest(struct recording* recordings, float* access_point_distances, int n_access_point_distances);


#endif