#ifndef ACCESSPOINTS_H
#define ACCESSPOINTS_H
/*
    Access Points
*/

#include "device.h"
#include "state.h"

struct AccessPoint* get_or_create_access_point(struct AccessPoint** access_points_list, const char* client_id, bool* created);

struct AccessPoint* add_access_point(struct AccessPoint** access_points_list, char* client_id, const char* description, const char* platform, 
    int rssi_one_meter, float rssi_factor, float people_distance);

struct AccessPoint *get_access_point(struct AccessPoint* access_point_list, int id);

void print_access_points(struct AccessPoint* access_points_list);

void print_min_distance_matrix(struct OverallState* state);

int get_index(struct AccessPoint* head, int id);

struct AccessPoint *update_accessPoints(struct AccessPoint** access_point_list, struct AccessPoint access_point);

#endif