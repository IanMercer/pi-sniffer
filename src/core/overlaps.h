#ifndef OVERLAPS_H
#define OVERLAPS_H

#include "state.h"

/*
    Do these two devices overlap in time? If so they cannot be the same device
    (allowed to touch given granularity of time)
*/
//bool overlapsClosest(struct Closest *a, struct Closest *b);

/*
    Compute the minimum number of devices present by assigning each in a non-overlapping manner to columns
*/
void pack_closest_columns(struct OverallState* state);

#endif