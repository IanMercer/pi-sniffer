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
    // Recordings are ephemeral but patches are forever
    struct patch* patch;
    // Distance for each access point in same order
    float access_point_distances[N_ACCESS_POINTS];
    // next
    struct recording* next;
};


// SAVE AND LOAD recordings

bool record (const char* directory, const char* device_name, float access_distances[N_ACCESS_POINTS], struct AccessPoint* access_points);

bool read_observations (const char * dirname, struct OverallState* state, bool confirmed);

void free_list(struct recording** head);

// KNN CLASSIFIER

#define TOP_K_N 17

struct top_k
{
    // Output from the scoring
    float probability_is;

    // Output from the scoring
    float probability_isnt;

    // p(is) * 1 - p(isnt)
    float probability_combined;
    /**
     * Pointer to a patch
     */
    struct patch* patch;
    bool used;
    /*
    * Probability
    */
    float normalized_probability;
};

int k_nearest(struct recording* recordings, 
    float accessdistances[N_ACCESS_POINTS],
    float accesstimes[N_ACCESS_POINTS], 
    double average_gap,
    struct AccessPoint* access_points, struct top_k* top_result, int top_count, 
    bool confirmed, bool debug);

/*
*  Compare two closest values
*/
float compare_closest (struct ClosestHead* a, struct ClosestHead* b, struct OverallState* state);

/*
*  Compare two distances using heuristic with cut off and no match handling
*/
double score_one_pair(float a_distance, float b_distance, time_t a_time, time_t b_time);

#endif