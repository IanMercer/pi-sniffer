#ifndef AGGREGATE_H
#define AGGREGATE_H

#include <string.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/types.h>
#include "device.h"


struct ClosestTo
{
   // Which access point
   struct AccessPoint* access_point;
   // How far from the access point was it
   float distance;
   // Earliest for this mac on this access point
   time_t earliest;
   // Latest for this mac on this access point
   time_t latest;
   // count from access point
   int count;

   // second sweep pass
   bool mark_pass_2;

   // next in chain of same mac address
   struct ClosestTo* next;
};


/*
* Short linked list of recent patches for a device
*/
struct RecentRoom
{
    // patch name
    char name[NAME_LENGTH];

    // time started
    time_t started;

    struct RecentRoom* next;
};


/*
*  Head on a chain of closest items
*/
struct ClosestHead
{
    // Mac address
    int64_t mac64;

    // category of the device
    int8_t category;

    // Name
    char name[NAME_LENGTH];

    // name type
    enum name_type name_type;

    // address type is public or random
    int8_t addressType; // 0, 1, 2

    // Pointer to first item, chain along each observation
    // also maintained in most recent first order
    struct ClosestTo* closest;

    // Superseded by another: i.e. access_id has seen this mac address
    // in a column more recently than this one that it superseeds
    int64_t supersededby;

    // Prior run superseded value
    int64_t debug_supersededby_prior;

    // Max probabiity that it was superseded even if didn't mark it as such
    float superseded_probability;

    // the most likely patch for this device
    struct patch* patch;

    // for training location to patch mappings
    bool is_training_beacon;

    // linked list of recent rooms
    struct RecentRoom* recent_rooms; 

    // next closest head in chain
    struct ClosestHead* next;
};

#endif