#ifndef H_ROOMS
#define H_ROOMS
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

// A room with weights mapping to sensor locations
struct room
{
    const char* name;
    const char* group;
    struct weight* weights;   // head of chain
};


// Initialize the rooms structure on startup
struct room** get_rooms(int* room_count);

#endif