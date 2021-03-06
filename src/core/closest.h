#ifndef _closest_h
#define _closest_h

#include <string.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/types.h>

#include "device.h"
#include "state.h"

// Add a closest observation
void add_closest(struct OverallState* state, int64_t device_64, struct AccessPoint* access_point, time_t earliest,
    time_t time, float distance, 
    int8_t category, int known_interval, int count, char* name,
    enum name_type name_type, int8_t addressType,
    bool is_training_beacon);

struct ClosestTo *get_closest_64(struct OverallState* state, int64_t mac64);

// Compute counts by patch, room and group, returns true if they changed
bool print_counts_by_closest(struct OverallState* state);

#endif