#ifndef KALMAN_FILE
#define KALMAN_FILE

/*
      Kalman filter
*/

struct Kalman
{
    double err_measure;
    double err_estimate;
    double q;
    double current_estimate;
    double last_estimate;
    double kalman_gain;
};

void kalman_initialize(struct Kalman *k);

float kalman_update(struct Kalman *k, double mea);

#endif
