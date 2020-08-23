#ifndef DEVICE_H
#define DEVICE_H

#include "kalman.h"
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/types.h>

typedef uint8_t  u_int8_t;
typedef uint16_t u_int16_t;
typedef uint32_t u_int32_t;
typedef uint64_t u_int64_t;

// Max allowed devices
#define N 2048

#define PUBLIC_ADDRESS_TYPE 1
#define RANDOM_ADDRESS_TYPE 2

#define CATEGORY_UNKNOWN 0
#define CATEGORY_PHONE 1
#define CATEGORY_WATCH 2
#define CATEGORY_TABLET 3
#define CATEGORY_HEADPHONES 4
#define CATEGORY_COMPUTER 5
#define CATEGORY_TV 6 
#define CATEGORY_FIXED 7
#define CATEGORY_BEACON 8
#define CATEGORY_CAR 9

// Max allowed length of names and aliases (plus 1 for null)
#define NAME_LENGTH         21

/*
   Structure for tracking BLE devices in range
*/
struct Device
{
    int id;
    char mac[18];                 // mac address string
    char name[NAME_LENGTH];
    char alias[NAME_LENGTH];
    int8_t addressType;           // 0, 1, 2
    int8_t category;               // Reasoned guess at what kind of device it is
    int manufacturer;
    bool paired;
    bool connected;
    bool trusted;
    uint32_t deviceclass;          // https://www.bluetooth.com/specifications/assigned-numbers/Baseband/
    uint16_t appearance;
    int manufacturer_data_hash;
    int service_data_hash;
    int uuids_length;
    int uuid_hash;                 // Hash value of all UUIDs - may ditinguish devices
    int txpower;                   // TX Power
    time_t last_rssi;              // last time an RSSI was received. If gap > 0.5 hour, ignore initial point (dead letter post)
    struct Kalman kalman;
    time_t last_sent;
    float distance;
    struct Kalman kalman_interval; // Tracks time between RSSI events in order to detect large gaps
    time_t earliest;               // Earliest time seen, used to calculate overlap
    time_t latest;                 // Latest time seen, used to calculate overlap
    int count;                     // Count how many times seen (ignore 1 offs)
    int column;                    // Allocated column in a non-overlapping range structure
    int8_t try_connect_state;      // Zero = never tried, 1 = Try in progress, 2 = Done
    // TODO: Collect data, SVM or trilateration TBD
    char* closest;                 // Closest access point name
};

int category_to_int(char* category);

char* category_from_int(uint i);

char* device_to_json (struct Device* device, const char* from);

bool device_from_json(const char* json, struct Device* device, char* from, int from_length);

void merge(struct Device* local, struct Device* remote);

// Shared device state object (one globally for app, thread safe access needed)
struct OverallState 
{
    int n;                       // current devices
    pthread_mutex_t lock;
    struct Device devices[N];
    char client_id[256];         // linux allows more, truncated
};

#endif
