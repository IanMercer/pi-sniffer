// influx.h
#ifndef INFLUX_H
#define INFLUX_H

#include "device.h"

/*
   Append an Influx line message, returns TRUE if successful, FALSE if output is full
*/
bool append_influx_line(char* line, int line_length,  const char* area_category, const char* tags, char* fields, time_t timestamp);

/*
    Post formatted line message
*/
void post_to_influx(struct OverallState* state, char* body, int body_length);

#endif
