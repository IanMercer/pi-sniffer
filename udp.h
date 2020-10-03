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

void update_superseded(struct OverallState* state, struct Device* device); 

void send_access_point_udp(struct OverallState* state);


struct ClosestTo* get_closest(struct Device* device);

void print_counts_by_closest(struct AccessPoint* access_points_list, struct room* room_list);

#endif
