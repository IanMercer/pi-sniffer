#ifndef state_h
#define state_h

#include "device.h"
#include <pthread.h>

// Shared device state object (one globally for app, thread safe access needed)
struct OverallState
{
   int n; // current devices
   bool network_up;        // Is the network up
   bool web_polling;        // website is running locally
   pthread_mutex_t lock;
   struct Device devices[N];

   struct AccessPoint *local; // the local access point (in the access points struct)

   int reboot_hour;           // Hour to reboot each day in local time (or zero for no reboot)

   // TODO: The following will all move to a new systemd service running on the
   // other end of DBUS.

   // Set only if you want to broadcast people to whole of LAN
   int udp_sign_port;      // The display for this group of sensors
   int udp_mesh_port;      // The mesh port for this group of sensors
   float udp_scale_factor; // Scale factor to multiply people by to send to screen
   // TODO: Settable parameters for the display

   char *mqtt_topic;
   char *mqtt_server;
   char *mqtt_username;
   char *mqtt_password;
   enum Verbosity verbosity;

   // No less than this many seconds between sends
   int influx_min_period_seconds;
   // No more than this many seconds between sends
   int influx_max_period_seconds;
   // time last sent
   time_t influx_last_sent;
   // server domain name
   char* influx_server;
   int influx_port;
   char* influx_database;
   char* influx_username;
   char* influx_password;

   // No less than this many seconds between sends
   int webhook_min_period_seconds;
   // No more than this many seconds between sends
   int webhook_max_period_seconds;
   // time last sent
   time_t webhook_last_sent;
   // Optional webhook for posting room counts to digital signage displays
   char* webhook_domain;
   int webhook_port;
   char* webhook_path;
   char* webhook_username;
   char* webhook_password;

   // path to config.json
   char* configuration_file_path;

   // linked list of access points
   struct AccessPoint* access_points;

   // linked list of rooms
   struct patch* patches;

   // linked list of groups
   struct group* groups;

   // linked list of recorded locations for k-means
   struct recording* recordings;

   // Most recent 2048 closest to observations
   int closest_n;

   // Hash value of patch scores to calculate if changes have happened
   int patch_hash;

   struct ClosestTo closest[CLOSEST_N];

   // linked list of beacons
   struct Beacon* beacons;

   // Latest JSON for sending over DBUS on request
   char* json;

   // minimum gap between sending updates
   int min_gap_seconds;

   // max gap between sending updates
   int max_gap_seconds;

   // seconds remaining on boosted sending schedule that ignores min_gap_seconds
   int boost_for_seconds;



};


/*
    Initialize state from environment
*/
void initialize_state(struct OverallState* state);

/*
   Log state
*/
void display_state(struct OverallState* state);

#endif
