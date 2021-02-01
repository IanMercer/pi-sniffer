#ifndef UDP_H
#define UDP_H

// Client side implementation of UDP client-server model 
#include "device.h"
#include "state.h"

#include <glib.h>
#include <gio/gio.h>

void udp_send(int port, const char* message, int message_length);

GCancellable* create_socket_service (struct OverallState* state);

void close_socket_service();

/*
    Broadcast updated device
*/
void send_device_udp(struct OverallState* state, struct Device* device); 

/*
    Update closest (direct, local update)
*/
void update_closest(struct OverallState* state, struct Device* device); 

void update_superseded(struct OverallState* state, struct Device* device); 

void send_access_point_udp(struct OverallState* state);

#endif
