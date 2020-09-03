// Client side implementation of UDP client-server model 
#include "udp.h"
#include "device.h"

// internal
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#define BLOCK_SIZE 1024

#define MAXLINE 1024
// This is the MESH PORT for communicating between devices (TODO: Make configurable)
#define PORT 7779

void udp_send(int port, const char* message, int message_length) {
    int sockfd;

    const int opt = 1;
    struct sockaddr_in servaddr;

    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0 ) {
        perror("socket creation failed");
        return;
    }
    if (setsockopt(sockfd, SOL_SOCKET, SO_BROADCAST, (char*)&opt, sizeof(opt)) < 0) {
        perror("setsockopt error");
        close(sockfd); 
        return;
    }

    memset(&servaddr, 0, sizeof(struct sockaddr_in));

    // Filling server information
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(port);
    //servaddr.sin_addr.s_addr = INADDR_ANY;
    servaddr.sin_addr.s_addr = htonl(INADDR_BROADCAST);

    int sent = sendto(sockfd, message, message_length, 0, (const struct sockaddr *) &servaddr, sizeof(struct sockaddr_in));
    if (sent < message_length)
    {
      g_print("    Incomplete message sent to port %i - %i bytes.\n", port, sent);
    }
    close(sockfd);
}

GCancellable *cancellable;
pthread_t listen_thread;

// All access points ever seen, in alphabetic order
int access_point_count = 0;
struct AccessPoint accessPoints[256];

static int access_id_sequence = 0;

struct AccessPoint* add_access_point(char* client_id,
  char* description, char* platform,
  float x, float y, float z, int rssi_one_meter, float rssi_factor, float people_distance)
{
  g_debug("Check for new access point '%s'\n", client_id);
  int found = access_point_count;
  for (int i = 0; i < access_point_count; i++){
      if (strcmp(accessPoints[i].client_id, client_id) == 0){
          found = i;
          break;
      }
  }

  struct AccessPoint* ap = &accessPoints[found];

  if (found == access_point_count) {
      if (found == sizeof(accessPoints)) return NULL;    // FULL
      // Add a new one
      strncpy(ap->client_id, client_id, META_LENGTH);
      strncpy(ap->description, description, META_LENGTH);
      strncpy(ap->platform, platform, META_LENGTH);
      ap->id = access_id_sequence++;
      ap->x = x;
      ap->y = y;
      ap->z = z;
      ap->rssi_one_meter = rssi_one_meter;
      ap->rssi_factor = rssi_factor;
      ap->people_distance = people_distance;
      access_point_count++;

      g_print("Access point: %i. %20s (%6.1f,%6.1f,%6.1f) RSSI(%3i, %.1f) Dist=%.1f\n", ap->id, ap->client_id, ap->x, ap->y,ap->z, ap->rssi_one_meter, ap->rssi_factor, ap->people_distance);
      g_print("              %s", ap->description);
      g_print("              %s", ap->platform);

      //g_print("ACCESS POINTS\n");
      //for (int k = 0; k < access_point_count; k++){
      //    g_print("%i. %20s (%f,%f,%f)\n", accessPoints[k].id, accessPoints[k].client_id,accessPoints[k].x, accessPoints[k].y,accessPoints[k].z );
      //}
  }
  else {
    if (ap->people_distance != people_distance) { g_print("%s People distance changed from %.1f to %.1f\n", ap->client_id, ap->people_distance, people_distance); ap->people_distance = people_distance; }
    if (ap->rssi_factor != rssi_factor) { g_print("%s RSSI factor changed from %.1f to %.1f\n", ap->client_id, ap->rssi_factor, rssi_factor); ap->rssi_factor = rssi_factor; }
    if (ap->rssi_one_meter != rssi_one_meter) { g_print("%s RSSI one meter changed from %i to %i\n", ap->client_id, ap->rssi_one_meter, rssi_one_meter); ap->rssi_one_meter = rssi_one_meter; }
  }
  return ap;
}

struct AccessPoint* update_accessPoints(struct AccessPoint access_point)
{
  return add_access_point(access_point.client_id,
      access_point.description, access_point.platform,
      access_point.x, access_point.y, access_point.z,
      access_point.rssi_one_meter, access_point.rssi_factor, access_point.people_distance);
}

/*
    Get access point by id
*/
struct AccessPoint* get_access_point(int id)
{
  for (int ap = 0; ap < access_point_count; ap++){
    if (accessPoints[ap].id == id){
      return &accessPoints[ap];
    }
  }
  return NULL;
}

#define CLOSEST_N 2048

// Most recent 2048 closest to observations
static uint closest_n = 0;
static struct ClosestTo closest[CLOSEST_N];

/*
   Add a closest observation (get a lock before you call this)
*/
void add_closest(int device_id, int access_id, time_t time, float distance)
{
  if (distance < 0.1) return;

  if (closest_n == CLOSEST_N){
    g_memmove(&closest[0], &closest[1], sizeof(struct ClosestTo)*(CLOSEST_N-1));
    closest_n--;
  }

  bool overwrite = FALSE;

  // Update the last value if it's the same ap, same device and within a few seconds
  // prevents array from being flooded with same value over and over
  if (closest_n > 0) {
    struct ClosestTo* last = &closest[closest_n-1];
    if (last->access_id == access_id && last->device_id == device_id){
       double delta_time = difftime(time, last->time);
       if (delta_time < 10.0) {
         //g_print("Overwriting access=%i device=%i", access_id, device_id);
         last->time = time;
         last->distance = distance;
         overwrite = TRUE;
       }
    }
  }

  if (!overwrite){
    closest[closest_n].access_id = access_id;
    closest[closest_n].device_id = device_id;
    closest[closest_n].distance = distance;
    closest[closest_n].time = time;
    closest_n++;
  }
}

/*
    Get the closest recent observation for a device
*/
struct ClosestTo* get_closest(int device_id)
{
  struct ClosestTo* best = NULL;

    for (int i = closest_n-1; i > 0; i--)
    {
      struct ClosestTo* test = &closest[i];

      if (test->device_id == device_id){
        if (best == NULL){
          best = test;
        } else if (best->access_id == test->access_id) {
          // continue, latest hit on access point is most relevant
        } 
        else if (best->distance > test->distance) {
          // TODO: Check time too, only recent ones
          double delta_time = difftime(best->time, test->time);

          struct AccessPoint* aptest = get_access_point(test->access_id);
          struct AccessPoint* apbest = get_access_point(best->access_id);

          if (delta_time < 60.0) {
            if (aptest && apbest){
              //g_print(" %% Closer to '%s' than '%s', %.1fm < %.1fm after %.1fs\n", aptest->client_id, apbest->client_id, test->distance, best->distance, delta_time);
            }
            best = test;
          }
          else {
            if (aptest && apbest){
              //g_print(" %% IGNORED Closer to '%s' than '%s', %.1fm < %.1fm after %.1fs\n", aptest->client_id, apbest->client_id, test->distance, best->distance, delta_time);
            }
          }
        }
      }
    }

    return best;
}


void *listen_loop(void *param)
{
  struct OverallState* state = (struct OverallState*) param;
  GError *error = NULL;

  GInetAddress *iaddr = g_inet_address_new_any(G_SOCKET_FAMILY_IPV4);
  GSocketAddress *addr = g_inet_socket_address_new(iaddr, PORT);

  GSocket* broadcast_socket = g_socket_new(G_SOCKET_FAMILY_IPV4, G_SOCKET_TYPE_DATAGRAM, G_SOCKET_PROTOCOL_UDP, &error);
  g_assert_no_error (error);

  g_socket_bind(broadcast_socket, addr, TRUE, &error);
  g_assert_no_error (error);

  g_print("Starting listen thread for mesh operation on port %i\n", PORT);
  g_print("Local client id is %s\n", state->local->client_id);
  g_cancellable_reset(cancellable);

  while (!g_cancellable_is_cancelled(cancellable))
  {
    char buffer[2048];
    int bytes_read = g_socket_receive_from(broadcast_socket, NULL, buffer, sizeof(buffer), cancellable, &error);
    if (bytes_read < 10)
    {
      g_print("Received bytes on listen thread %i < %i\n", bytes_read, 10);
      continue; // not enough to be a device message
    }

    struct Device d;
    struct AccessPoint a;
    strncpy(a.client_id, "notset", 7);
    strncpy(a.description, "notset", 7);
    strncpy(a.platform, "notset", 7);
    a.x = -1;
    a.y = -1;
    a.z = -1;
    a.people_distance = 0.0;
    a.rssi_factor = 0.0;
    a.rssi_one_meter = 0.0;

    if (device_from_json(buffer, &a, &d))
    {
      struct AccessPoint* actual = update_accessPoints(a);
      // Replace with the local interned copy of ap
      a = *actual;

      // ignore messages from self
      if (strcmp(a.client_id, state->local->client_id) == 0) {
        //g_print("Ignoring message from self %s : %s\n", a.client_id, d.mac);
        continue;
      }

      time_t now;
      time(&now);

      bool found = true;

      pthread_mutex_lock(&state->lock);

      for(int i = 0; i < state->n; i++)
      {
         if (strncmp(d.mac, state->devices[i].mac, 18) == 0)
         {
            //g_print("%s '%s' dt=%3li", d.mac, d.name, now-d.latest);
            merge(&state->devices[i], &d);
            // use the local id for the device not any remote id
            add_closest(state->devices[i].id, a.id, d.latest, d.distance);
            struct ClosestTo* closest = get_closest(state->devices[i].id);

             if (closest) { // && (closest->distance < state->devices[i].distance)) {
               struct AccessPoint* ap = get_access_point(closest->access_id);
               if (ap) {
                  //g_print(" * Closest overall is '%s' at %.2f\n", ap->client_id, closest->distance);
               }
             }

             break;
         }
      }

      pthread_mutex_unlock(&state->lock);

      if (!found){
        g_print("Add foreign device %s\n", d.mac);
        //struct Device added;
        //added.id = -1;
      }

      printf("\n");

    }
  }
  printf("Listen thread finished\n");
  return NULL;
}

/*
    Create Socket Service
    TEST:  sudo nc -l -u -b -k -4 -D -p 7779
*/
GCancellable* create_socket_service (struct OverallState* state)
{
  cancellable = g_cancellable_new();

  if (pthread_create(&listen_thread, NULL, listen_loop, state)) {
    fprintf(stderr, "Error creating thread\n");
    return NULL;
  }

  g_print("Created UDP listener on port %i", PORT);
  return cancellable;
}

/*
    Send device update over UDP broadcast to all other access points
*/
void send_device_udp(struct OverallState* state, struct Device* device) 
{
    //printf("    Send UDP %i device %s '%s'\n", PORT, device->mac, device->name);
    char* json = device_to_json(state->local, device);
    //printf("    %s\n", json);
    udp_send(PORT, json, strlen(json)+1);
    free(json);

    // Add local observations into the same structure
    add_closest(device->id, state->local->id, device->latest, device->distance);
}

/*
    Close Socket Service
*/

void close_socket_service()
{
  g_cancellable_cancel(cancellable);
}
