#ifndef UDP_H
#define UDP_H

// Client side implementation of UDP client-server model 
#include <glib.h>
#include <gio/gio.h>
#include "device.h"

void udp_send(int port, const char* message, int message_length);

GCancellable* create_socket_service (struct OverallState* state);

void close_socket_service();

void send_device_udp(struct Device* device); 

struct AccessPoint* get_access_point(int id);

struct ClosestTo* get_closest(int device_id);

#endif
