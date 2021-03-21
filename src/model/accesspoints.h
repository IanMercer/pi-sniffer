#ifndef ACCESSPOINTS_H
#define ACCESSPOINTS_H
/*
    Access Points
*/

#include "device.h"
#include "state.h"

struct AccessPoint* get_or_create_access_point(struct OverallState* state, const char* client_id, bool* created);

struct AccessPoint* create_local_access_point(struct OverallState* state, char* client_id, const char* description, const char* platform, 
    int rssi_one_meter, float rssi_factor, float people_distance);

struct AccessPoint *get_access_point(struct AccessPoint* access_point_list, int id);

void print_access_points(struct AccessPoint* access_points_list);

void print_min_distance_matrix(struct OverallState* state);

int get_index(struct AccessPoint* head, int id);

/**
 * add or update a sensor float
 */
void add_or_update_sensor_float (struct AccessPoint* ap, const char* id, float value);

/**
 * add or update a sensor int
 */
void add_or_update_sensor_int (struct AccessPoint* ap, const char* id, int value);

void add_or_update_internal_temp (struct AccessPoint* ap, float value);
void add_or_update_temperature (struct AccessPoint* ap, float value);
void add_or_update_humidity (struct AccessPoint* ap, float value);
void add_or_update_pressure (struct AccessPoint* ap, float value);
void add_or_update_co2 (struct AccessPoint* ap, float value);
void add_or_update_brightness (struct AccessPoint* ap, float value);
void add_or_update_voc (struct AccessPoint* ap, float value);
void add_or_update_wifi (struct AccessPoint* ap, int value);
void add_or_update_disk_space (struct AccessPoint* ap, int value);

void get_sensor_string(struct AccessPoint* ap, char* buffer, int buffer_len, int num_args, ...);

#endif