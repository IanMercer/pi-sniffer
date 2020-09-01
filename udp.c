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
char* access_point_name;

// All access points ever seen, in alphabetic order
uint access_point_count = 0;
struct AccessPoint accessPoints[256];

void add_access_point(const char* name, float x, float y, float z) {
  strcpy(accessPoints[access_point_count].client_id, name);
  accessPoints[access_point_count].x = x;
  accessPoints[access_point_count].y = y;
  accessPoints[access_point_count].z = z;
  access_point_count++;
}

void update_accessPoints(struct AccessPoint access_point)
{
  g_debug("Check for new access point '%s'\n", access_point.client_id);
  uint found = access_point_count;
  for (uint i = 0; i < access_point_count; i++){
      if (strcmp(accessPoints[i].client_id, access_point.client_id) >= 0){
          found = i;
          break;
      }
  }

  g_debug("Found %i in check for new access point\n", found);
  if (found == access_point_count || strcmp(accessPoints[found].client_id, access_point.client_id) > 0) {
      // insert access point here, shift others down, increase count
      int to_move = access_point_count - found;
      if (to_move > 0){
          g_debug("Moving %i from %i up\n", to_move, found);
          memmove(&accessPoints[found+1], &accessPoints[found], sizeof(struct AccessPoint) * to_move);
      }
      memcpy(&accessPoints[found], &access_point, sizeof(struct AccessPoint));
      access_point_count++;

      g_print("ACCESS POINTS\n");
      for (uint k = 0; k < access_point_count; k++){
          g_print("%i. %32s (%f,%f,%f)\n", k, accessPoints[k].client_id,accessPoints[k].x, accessPoints[k].y,accessPoints[k].z );
      }
  }
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
  g_print("Local client id is %s\n", state->client_id);
  g_cancellable_reset(cancellable);

  add_access_point("barn",  -1000, -1000,   2.0);
  add_access_point("garage",  10.0,  0.5,   2.0);
  add_access_point("kitchen", 46.0,-24.0, 2.0);
  add_access_point("livingroom", 43.0,-4.0, 2.0);
  add_access_point("pileft",  31,    8.0,  -6.0);
  add_access_point("store",   32,    0.5,   2.0);
  add_access_point("study",   51,    7.0,   2.0);
  add_access_point("tiger",   53,   20.0,  -6.0);
  add_access_point("ubuntu",  32,    7.0,  -6.0);
 
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
    a.x = -1;
    a.y = -1;
    a.z = -1;

    if (device_from_json(buffer, &a, &d))
    {
      update_accessPoints(a);

      // ignore messages from self
      if (strcmp(a.client_id, access_point_name) == 0) continue;

      time_t now;
      time(&now);

      // TODO: Lock the structure
      for(int i = 0; i < state->n; i++)
      {
         if (strncmp(d.mac, state->devices[i].mac, 18) == 0)
         {
             g_print("%s '%s' dt=%3li", d.mac, d.name, now-d.latest);
             merge(&state->devices[i], &d);
             float ourdistance = state->devices[i].distance;
             if (d.distance < ourdistance)
             {
               g_print(" * closest to %s->%s %.1fm:%.1fm *\n", a.client_id, access_point_name, d.distance, ourdistance);
             } else 
             {
               g_print(" * closest to %s<-%s %.1fm:%.1fm *\n", access_point_name, a.client_id, ourdistance, d.distance);
             }
             break;
         }
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
  access_point_name = state->client_id;

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
void send_device_udp(struct Device* device) 
{
    //printf("    Send UDP %i device %s '%s'\n", PORT, device->mac, device->name);
    char* json = device_to_json(device, access_point_name);
    //printf("    %s", json);
    udp_send(PORT, json, strlen(json)+1);
    free(json);
}

/*
    Close Socket Service
*/

void close_socket_service()
{
  g_cancellable_cancel(cancellable);
}
