#ifndef MQTTSEND_FILE
#define MQTTSEND_FILE
/*
    MQTT code to send BLE status
    And utility functions
*/

#include <device.h>

void exit_mqtt();

void prepare_mqtt(char *mqtt_uri, char *mqtt_topicRoot, char* client_id, char* mac_address, char* username, char* password);

void send_to_mqtt_null(char *mac_address, char *key);

void send_to_mqtt_single(char *mac_address, char *key, char *value);

void send_to_mqtt_distances(unsigned char *value, int length);

void send_to_mqtt_array(char *mac_address, char *key, unsigned char *value, int length);

void send_to_mqtt_uuids(char *mac_address, char *key, char **uuids, int length);

void send_to_mqtt_single_value(char *mac_address, char *key, int32_t value);

void send_to_mqtt_single_value_keep(char *mac_address, char *key, int32_t value);

void send_to_mqtt_single_float(char *mac_address, char *key, float value);

void mqtt_sync();

void send_device_mqtt(struct Device* device);

#endif
