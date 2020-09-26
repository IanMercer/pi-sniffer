#ifndef H_ROOMS
#define H_ROOMS

#include <stdlib.h>
#include <stdbool.h>

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

// A group
struct group
{
    const char* name;
    struct group* next;         // next ptr
    double group_total;         // across all aps and rooms
};


// A room with weights mapping to sensor locations
struct room
{
    const char* name;
    struct group* group;        // identity mapped group
    struct weight* weights;     // head of chain of weights
    struct room* next;          // next ptr
    double room_score;          // calculated during scan, one ap
    double room_total;          // across all aps
};

// Initialize the rooms structure on startup
void get_rooms(struct room** room_list, struct group** group_list);

#endif