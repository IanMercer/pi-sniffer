#ifndef SERIALIZATION_H
#define SERIALIZATION_H

#include "../model/device.h"
#include "utility.h"
#include "../model/accesspoints.h"

char *device_to_json(struct AccessPoint *a, struct Device *device);

char *access_point_to_json(struct AccessPoint *a);

struct AccessPoint* device_from_json(const char* json, struct OverallState* state, struct Device* device);


#endif
