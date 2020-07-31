#include "kalman.h"
#include <stdbool.h>
#include <stdio.h>
#include <math.h>

/*
      Kalman filter
*/

void kalman_initialize(struct Kalman *k)
{
    k->current_estimate = -999; // marker value used by time interval check
    k->last_estimate = -999; // marker value, so first real value overrides it
    k->err_measure = 10.0;
    k->err_estimate = 10.0;
    k->q = 0.25;   // was 0.25 which was too slow
}

float kalman_update(struct Kalman *k, double mea)
{
    // First time through, use the measured value as the actual value
    if (k->last_estimate == -999)
    {
        k->last_estimate = mea;
        return mea;
    }
    //g_print("%f %f %f %f\n", k->err_measure, k->err_estimate, k->q, mea);
    k->kalman_gain = k->err_estimate / (k->err_estimate + k->err_measure);
    k->current_estimate = k->last_estimate + k->kalman_gain * (mea - k->last_estimate);
    k->err_estimate = (1.0 - k->kalman_gain) * k->err_estimate + fabs(k->last_estimate - k->current_estimate) * k->q;
    k->last_estimate = k->current_estimate;

    return k->current_estimate;
}

