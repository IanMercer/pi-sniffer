#ifndef UDP_H
#define UDP_H

// Client side implementation of UDP client-server model 
#include <glib.h>
#include <gio/gio.h>
#include "device.h"

// internal
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>

void udp_send(int port, const char* message, int message_length);

GCancellable* create_socket_service (char* access_name);

void close_socket_service();

void send_device_udp(struct Device* device); 

#endif
