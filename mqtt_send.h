#ifndef MQTTSEND_FILE
#define MQTTSEND_FILE
/*
    MQTT code to send BLE status
    And utility functions
*/

#include "device.h"

#include <glib.h>
#include <gio/gio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <stdint.h>
#include <sys/types.h>
#include <string.h>

void exit_mqtt();

void prepare_mqtt(char *mqtt_uri, char *mqtt_topicRoot, char* access_name, char* service_name, char* mac_address, char* username, char* password);

void send_to_mqtt_null(char *mac_address, char *key);

void send_to_mqtt_single(char *mac_address, char *key, char *value);

void send_to_mqtt_distances(unsigned char *value, int length);

void send_to_mqtt_array(char *mac_address, char *key, char* valuekey, unsigned char *value, int length);

void send_to_mqtt_uuids(char *mac_address, char *key, char **uuids, int length);

void send_to_mqtt_single_value(char *mac_address, char *key, int32_t value);

void send_to_mqtt_single_value_keep(char *mac_address, char *key, int32_t value);

void send_to_mqtt_single_float(char *mac_address, char *key, float value);

void mqtt_sync();

void send_device_mqtt(struct Device* device);

#endif
