#ifndef webhook_h
#define webhooh_h

#include "device.h"

/*
    posts count by zone and beacon locations to an endpoint as JSON
*/

void post_to_webhook (struct OverallState* state);

#endif
