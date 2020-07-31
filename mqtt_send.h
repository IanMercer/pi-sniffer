#ifndef MQTTSEND_FILE
#define MQTTSEND_FILE
/*
    MQTT code to send BLE status
    And utility functions
*/

#include <glib.h>
#include <gio/gio.h>
#include "mqtt.h"
#include <stdbool.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include "posix_sockets.h"
#include <net/if.h>
#include <time.h>
#include <math.h>
#include <signal.h>
#include <stdlib.h>

int sockfd;

struct mqtt_client mqtt;

void exit_mqtt(int status, int sockfd);

void prepare_mqtt(char *mqtt_addr, char *mqtt_port, char* client_id, char* mac_address);

void send_to_mqtt_null(char *mac_address, char *key);

void send_to_mqtt_with_time_and_mac(char *mac_address, char *key, int i, char *value, int value_length, int flags);

void send_to_mqtt_single(char *mac_address, char *key, char *value);

void send_to_mqtt_array(char *mac_address, char *key, unsigned char *value, int length);

void send_to_mqtt_uuids(char *mac_address, char *key, char **uuids, int length);

void send_to_mqtt_single_value(char *mac_address, char *key, int32_t value);

void send_to_mqtt_single_value_keep(char *mac_address, char *key, int32_t value);

void send_to_mqtt_single_float(char *mac_address, char *key, float value);

#endif
