// Client side implementation of UDP client-server model
#include "udp.h"
#include "utility.h"
#include "rooms.h"
#include "accesspoints.h"
#include "closest.h"
#include "state.h"

// internal
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <time.h>
#include <math.h>
#include "cJSON.h"
#include "knn.h"
#include "state.h"

#define BLOCK_SIZE 1024

#define MAXLINE 1024

void udp_send(int port, const char *message, int message_length)
{
    if (port == 0) return; // not configured
    if (is_any_interface_up())
    {
        int sockfd;

        const int opt = 1;
        struct sockaddr_in servaddr;

        if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
        {
            perror("socket creation failed");
            return;
        }
        if (setsockopt(sockfd, SOL_SOCKET, SO_BROADCAST, (char *)&opt, sizeof(opt)) < 0)
        {
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

        int sent = sendto(sockfd, message, message_length, 0, (const struct sockaddr *)&servaddr, sizeof(struct sockaddr_in));
        if (sent < message_length)
        {
            // Need some way to detect network is not connected
            g_warning("    Incomplete message sent to port %i - %i bytes.", port, sent);
        }
        close(sockfd);
    }
    else {
        //g_debug("No interface running, skip UDP send");
    }
}

static GCancellable *cancellable;
static pthread_t listen_thread;

void *listen_loop(void *param)
{
    struct OverallState *state = (struct OverallState *)param;
    GError *error = NULL;

    g_info("UDP Mesh port %i", state->udp_mesh_port);

    GInetAddress *iaddr = g_inet_address_new_any(G_SOCKET_FAMILY_IPV4);
    GSocketAddress *addr = g_inet_socket_address_new(iaddr, state->udp_mesh_port);

    GSocket *broadcast_socket = g_socket_new(G_SOCKET_FAMILY_IPV4, G_SOCKET_TYPE_DATAGRAM, G_SOCKET_PROTOCOL_UDP, &error);
    g_assert_no_error(error);

    g_socket_bind(broadcast_socket, addr, TRUE, &error);
    g_assert_no_error(error);

    g_info("LT: Starting listen thread for mesh operation on port %i", state->udp_mesh_port);
    g_info("LT: Local client id is %s", state->local->client_id);
    g_cancellable_reset(cancellable);

    if (!is_any_interface_up()) g_warning("LT: No interface to listen on");

    while (!g_cancellable_is_cancelled(cancellable))
    {
        char buffer[2048];
        int bytes_read = g_socket_receive_from(broadcast_socket, NULL, buffer, sizeof(buffer), cancellable, &error);
        if (bytes_read < 50)
        {
            //g_print("Received bytes on listen thread %i < %i\n", bytes_read, 10);
            continue; // not enough to be a device message
        }

        // Record time received to compare against time sent to check clock-sync
        time_t now;
        time(&now);

        struct Device d;
        d.mac64 = 0;

        struct AccessPoint dummy;
        strncpy(dummy.client_id, "notset", 7);
        strncpy(dummy.description, "notset", 7);
        strncpy(dummy.platform, "notset", 7);
        dummy.id = -1;
        dummy.people_distance = 0.0;
        dummy.rssi_factor = 0.0;
        dummy.rssi_one_meter = 0.0;
        dummy.sequence = 0;

        strncpy(d.mac, "notset", 7);  // access point only messages have no device mac address

        if (device_from_json(buffer, &dummy, &d))
        {
            struct AccessPoint *actual = update_accessPoints(&state->access_points, dummy);
            g_assert(actual != NULL);

            // ignore messages from self
            if (strcmp(actual->client_id, state->local->client_id) == 0)
            {
                //g_debug("Ignoring message from self %s : %s\n", dummy.client_id, d.mac);
                continue;
            }

            if (d.mac64 == 0)
            {
                //g_info("Ignoring access point only message from %s", dummy.client_id);
                continue;
            }

            pthread_mutex_lock(&state->lock);

            // Find matching device and merge in any data it doesn't have

            for (int i = 0; i < state->n; i++)
            {
                if (d.mac64 == state->devices[i].mac64)
                {
                    int delta_time = difftime(now, d.latest_local);

                    merge(&state->devices[i], &d, actual->client_id, delta_time == 0);

                    // This is a current observation, time should match

                    // If the delta time between our clock and theirs is > 0, log it
                    if (delta_time < 0)
                    {
                        // This is problematic, they are ahead of us
                        g_warning("%s '%s' %s dist=%.2fm time=%is", d.mac, d.name, actual->client_id, d.distance, delta_time);
                    }

                    g_assert(actual != NULL);
                   
                    break;
                }
            }

            // Update the closest data structure
            g_assert(actual != NULL);
            //g_debug("UDP: %s %s count=%i ap=%s %s %.1fm", d.mac, d.name, d.count, actual->client_id, dummy.client_id, d.distance);
            add_closest(state, d.mac64, actual, d.earliest, d.latest_local, d.distance, d.category, d.count, d.name, 
                d.name_type, d.address_type,
                d.is_training_beacon);

            pthread_mutex_unlock(&state->lock);
        }
    }
    g_info("LT: Listen thread finished");
    return NULL;
}

/*
    Create Socket Service
    You can monitor the socket output using:  sudo nc -l -u -b -k -4 -D -p 7779
*/
GCancellable *create_socket_service(struct OverallState *state)
{
    g_info("Creating UDP listener on port %i", state->udp_mesh_port);

    cancellable = g_cancellable_new();

    if (pthread_create(&listen_thread, NULL, listen_loop, state))
    {
        fprintf(stderr, "Error creating thread\n");
        return NULL;
    }

    g_info("Created UDP listener on port %i", state->udp_mesh_port);
    return cancellable;
}

/*
    Send device update over UDP broadcast to all other access points
    This allows them to update their information about a device sooner
    without having to wait for it to send it to them directly.
    It is also used to track the closest access point to any device.
*/
void send_device_udp(struct OverallState *state, struct Device *device)
{
    //printf("    Send UDP %i device %s '%s'\n", PORT, device->mac, device->name);
    state->local->sequence++;
    char *json = device_to_json(state->local, device);
    //printf("    %s\n", json);
    udp_send(state->udp_mesh_port, json, strlen(json) + 1);
    free(json);
}


/*
    Update closest (called from a local update)
*/
void update_closest(struct OverallState *state, struct Device *device)
{
    //g_debug("update_closest(%s, %i, %s)", state->local->client_id, state->local->id, device->mac);
    // Add local observations into the same structure
    int64_t id_64 = mac_string_to_int_64(device->mac);
    add_closest(state, id_64, state->local, device->earliest, device->latest_local, device->distance, device->category, 
        device->count, 
        device->name, 
        device->name_type, device->address_type,
        device->is_training_beacon);
}

/*
    Send access point over UDP broadcast to all other access points
*/
void send_access_point_udp(struct OverallState *state)
{
    char *json = access_point_to_json(state->local);
    //g_info("    Send UDP %i access point %s\n", PORT, json);
    //printf("    %s\n", json);
    udp_send(state->udp_mesh_port, json, strlen(json) + 1);
    free(json);
}

/*
    Close Socket Service
*/

void close_socket_service()
{
    g_cancellable_cancel(cancellable);
}
