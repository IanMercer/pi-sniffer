// influx.h
#ifndef INFLUX_H
#define INFLUX_H

#include "device.h"

void post_to_influx(struct OverallState* state, const char* topic, double value, time_t timestamp);

#endif