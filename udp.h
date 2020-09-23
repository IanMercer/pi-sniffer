#ifndef UDP_H
#define UDP_H

// Client side implementation of UDP client-server model 
#include "device.h"

#include <glib.h>
#include <gio/gio.h>

void udp_send(int port, const char* message, int message_length);

GCancellable* create_socket_service (struct OverallState* state);

void close_socket_service();

void send_device_udp(struct OverallState* state, struct Device* device); 

void update_closest(struct OverallState* state, struct Device* device); 
void update_superceded(struct OverallState* state, struct Device* device); 

void send_access_point_udp(struct OverallState* state);

struct AccessPoint* add_access_point(char* client_id, const char* description, const char* platform, 
    float x, float y, float z, int rssi_one_meter, float rssi_factor, float people_distance);

struct AccessPoint* get_access_point(int id);

/*
    Iterate over access points
*/
void access_points_foreach(void (*f)(struct AccessPoint* ap, void *), void *f_data);

void print_access_points();

struct ClosestTo* get_closest(struct Device* device);

void print_counts_by_closest();

#endif
