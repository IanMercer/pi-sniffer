#ifndef DEVICE_H
#define DEVICE_H

#include "kalman.h"
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

#define CATEGORY_UNKNOWN "unknown"
#define CATEGORY_PHONE "phone"
#define CATEGORY_WATCH "watch"
#define CATEGORY_TABLET "tablet"
#define CATEGORY_HEADPHONES "hp"
#define CATEGORY_COMPUTER "computer"
#define CATEGORY_TV "TV"    // AppleTV
#define CATEGORY_FIXED "fixed"
#define CATEGORY_BEACON "beacon"

// Max allowed length of names and aliases (plus 1 for null)
#define NAME_LENGTH         21

/*
   Structure for tracking BLE devices in range
*/
struct Device
{
    uint32_t id;
    char mac[18];                 // mac address string
    char name[NAME_LENGTH];
    char alias[NAME_LENGTH];
    int8_t addressType;           // 0, 1, 2
    char* category;               // Reasoned guess at what kind of device it is
    int32_t manufacturer;
    bool paired;
    bool connected;
    bool trusted;
    uint32_t deviceclass;          // https://www.bluetooth.com/specifications/assigned-numbers/Baseband/
    uint16_t appearance;
    int32_t manufacturer_data_hash;
    int32_t service_data_hash;
    int32_t uuids_length;
    int32_t uuid_hash;             // Hash value of all UUIDs - may ditinguish devices
    int32_t txpower;               // TX Power
    time_t last_rssi;              // last time an RSSI was received. If gap > 0.5 hour, ignore initial point (dead letter post)
    struct Kalman kalman;
    time_t last_sent;
    float distance;
    struct Kalman kalman_interval; // Tracks time between RSSI events in order to detect large gaps
    time_t earliest;               // Earliest time seen, used to calculate overlap
    time_t latest;                 // Latest time seen, used to calculate overlap
    int32_t count;                 // Count how many times seen (ignore 1 offs)
    int32_t column;                // Allocated column in a non-overlapping range structure
    int8_t try_connect_state;     // Zero = never tried, 1 = Try in progress, 2 = Done
};

#endif
