#ifndef ACCESSPOINTS_H
#define ACCESSPOINTS_H
/*
    Access Points
*/

struct AccessPoint* get_or_create_access_point(struct AccessPoint** access_points_list, const char* client_id, bool* created);

struct AccessPoint* add_access_point(struct AccessPoint** access_points_list, char* client_id, const char* description, const char* platform, 
    float x, float y, float z, int rssi_one_meter, float rssi_factor, float people_distance);

struct AccessPoint *get_access_point(struct AccessPoint* access_point_list, int id);

void print_access_points(struct AccessPoint* access_points_list);

int get_index(struct AccessPoint* head, int id);

struct AccessPoint *update_accessPoints(struct AccessPoint** access_point_list, struct AccessPoint access_point);

#endif