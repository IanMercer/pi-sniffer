// influx.h
#ifndef INFLUX_H
#define INFLUX_H

#include "device.h"

/*
   Append an Influx line message
*/
void append_influx_line(struct OverallState* state, char* line, int line_length,  const char* group, const char* topic, double value, time_t timestamp);

/*
    Post formatted line message
*/
void post_to_influx(struct OverallState* state, char* body, int body_length);

#endif