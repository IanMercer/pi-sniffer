// Client side implementation of UDP client-server model
#include "udp.h"
#include "utility.h"

// internal
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <math.h>

#define BLOCK_SIZE 1024

#define MAXLINE 1024

void udp_send(int port, const char *message, int message_length)
{
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
            //g_print("    Incomplete message sent to port %i - %i bytes.\n", port, sent);
        }
        close(sockfd);
    }
    else {
        g_debug("No interface running, skip UDP send");
    }
}

GCancellable *cancellable;
pthread_t listen_thread;

// All access points ever seen, in alphabetic order
int access_point_count = 0;
struct AccessPoint accessPoints[256];

static int access_id_sequence = 0;

struct AccessPoint *add_access_point(char *client_id,
                                     const char *description, const char *platform,
                                     float x, float y, float z, int rssi_one_meter, float rssi_factor, float people_distance)
{
    //g_debug("Check for new access point '%s'\n", client_id);
    int found = access_point_count;
    for (int i = 0; i < access_point_count; i++)
    {
        if (strcmp(accessPoints[i].client_id, client_id) == 0)
        {
            found = i;
            break;
        }
    }

    struct AccessPoint *ap = &accessPoints[found];

    if (found == access_point_count)
    {
        if (found == sizeof(accessPoints))
            return NULL; // FULL
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
        ap->people_closest_count = 0.0;
        ap->people_in_range_count = 0.0;
        access_point_count++;

        g_debug("Added access point %s %s %s p=(closest=%.1f,range=%.1f) (%6.1f,%6.1f,%6.1f) RSSI(%3i, %.1f) Dist=%.1f\n", 
          ap->client_id, ap->platform, ap->description,
          ap->people_closest_count, ap->people_in_range_count,
          ap->x, ap->y, ap->z, ap->rssi_one_meter, ap->rssi_factor, ap->people_distance);

        //g_print("ACCESS POINTS\n");
        //for (int k = 0; k < access_point_count; k++){
        //    g_print("%i. %20s (%f,%f,%f)\n", accessPoints[k].id, accessPoints[k].client_id,accessPoints[k].x, accessPoints[k].y,accessPoints[k].z );
        //}
    }
    else
    {
        if (ap->people_distance != people_distance)
        {
            g_print("%s People distance changed from %.1f to %.1f\n", ap->client_id, ap->people_distance, people_distance);
            ap->people_distance = people_distance;
        }
        if (ap->rssi_factor != rssi_factor)
        {
            g_print("%s RSSI factor changed from %.1f to %.1f\n", ap->client_id, ap->rssi_factor, rssi_factor);
            ap->rssi_factor = rssi_factor;
        }
        if (ap->rssi_one_meter != rssi_one_meter)
        {
            g_print("%s RSSI one meter changed from %i to %i\n", ap->client_id, ap->rssi_one_meter, rssi_one_meter);
            ap->rssi_one_meter = rssi_one_meter;
        }
    }
    time(&ap->last_seen);

    return ap;
}

void print_access_points()
{
    time_t now;
    time(&now);
    float people_total = 0.0;
    g_info("ACCESS POINTS          Platform       Close Range (x,y,z)                 Parameters         Last Seen");
    for (int k = 0; k < access_point_count; k++)
    {
        struct AccessPoint ap = accessPoints[k];
        int delta_time = difftime(now, ap.last_seen);
        g_info("%20s %16s (%4.1f %4.1f) (%6.1f,%6.1f,%6.1f) (%3i, %.1f, %.1fm) %is",
        ap.client_id, ap.platform,
        ap.people_closest_count, ap.people_in_range_count,
        ap.x, ap.y, ap.z, ap.rssi_one_meter, ap.rssi_factor, ap.people_distance,
        delta_time);
        //g_print("              %16s %s\n", ap->platform, ap->description);
        people_total += ap.people_closest_count;
    }
    g_info("Total people = %.1f", people_total);
}

struct AccessPoint *update_accessPoints(struct AccessPoint access_point)
{
    struct AccessPoint* ap = add_access_point(access_point.client_id,
                            access_point.description, access_point.platform,
                            access_point.x, access_point.y, access_point.z,
                            access_point.rssi_one_meter, access_point.rssi_factor, access_point.people_distance);
    ap->people_closest_count = access_point.people_closest_count;
    ap->people_in_range_count = access_point.people_in_range_count;
    strncpy(ap->description, access_point.description, META_LENGTH);
    strncpy(ap->platform, access_point.platform, META_LENGTH);
    time(&access_point.last_seen);
    return ap;
}

/*
    Get access point by id
*/
struct AccessPoint *get_access_point(int id)
{
    for (int ap = 0; ap < access_point_count; ap++)
    {
        if (accessPoints[ap].id == id)
        {
            return &accessPoints[ap];
        }
    }
    return NULL;
}

void access_points_foreach(void (*f)(struct AccessPoint* ap, void *), void *f_data)
{
    for(int i = 0; i < access_point_count; i++) f(&accessPoints[i], f_data);
}

#define CLOSEST_N 2048

// Most recent 2048 closest to observations
static uint closest_n = 0;
static struct ClosestTo closest[CLOSEST_N];

/*
   Add a closest observation (get a lock before you call this)
*/
void add_closest(int64_t device_64, int access_id, time_t time, float distance, int8_t category, int64_t superseededby)
{
    if (superseededby != 0)
    {
        char mac[18];
        mac_64_to_string(mac, 18, device_64);
        g_debug("Removing %s from closest as it's superceeded", mac);
        for (int j = closest_n; j >= 0; j--)
        {
            if (closest[j].device_64 == device_64)
            {
                closest[j].superceededby = superseededby;
            }
        }
        // Could trim them from array
        return;
    }

    if (distance < 0.1)
        return;

    // If the array is full, shuffle it down one
    if (closest_n == CLOSEST_N)
    {
        //g_debug("Trimming closest array");
        g_memmove(&closest[0], &closest[1], sizeof(struct ClosestTo) * (CLOSEST_N - 1));
        closest_n--;
    }

    bool overwrite = FALSE;

    // Update the last value if it's the same ap, same device and within a few seconds
    // prevents array from being flooded with same value over and over
    if (closest_n > 0)
    {
        struct ClosestTo *last = &closest[closest_n - 1];
        if (last->access_id == access_id && last->device_64 == device_64)
        {
            double delta_time = difftime(time, last->time);
            if (delta_time < 10.0)
            {
                //g_print("Overwriting access=%i device=%i", access_id, device_id);
                last->time = time;
                last->distance = distance;
                last->category = category;
                last->superceededby = superseededby;
                overwrite = TRUE;
            }
        }
    }

    if (!overwrite)
    {
        closest[closest_n].access_id = access_id;
        closest[closest_n].device_64 = device_64;
        closest[closest_n].distance = distance;
        closest[closest_n].category = category;
        closest[closest_n].superceededby = superseededby;
        closest[closest_n].time = time;
        closest_n++;
    }
}

/*
   Set count to zero
*/
void set_count_to_zero(struct AccessPoint* ap, void* extra)
{
    (void)extra;
    ap->people_closest_count = 0;
    ap->people_in_range_count = 0;
}

/*
    Find counts by access point
*/
void print_counts_by_closest()
{
    float total_count = 0.0;
    access_points_foreach(&set_count_to_zero, NULL);

    g_info("Counts");
    time_t now = time(0);

    for (int i = closest_n - 1; i >= 0; i--)
    {
        closest[i].mark = false;
    }

    for (int i = closest_n - 1; i >= 0; i--)
    {
        if (closest[i].category != CATEGORY_PHONE) continue;
        if (closest[i].mark) continue;  // already claimed
        struct ClosestTo *test = &closest[i];

        char mac[18];
        mac_64_to_string(mac, sizeof(mac), test->device_64);

        // mark remainder of array as claimed
        for (int j = i; j > 0; j--)
        {
            if (closest[j].device_64 == test->device_64)
            {
                closest[j].mark = true;

                // Is this a better match than the current one?
                int time_diff = difftime(test->time, closest[j].time);
                float distance_dilution = time_diff / 10.0;  // 0.1 m/s  1.4m/s human speed

                // e.g. test = 10.0m, current = 3.0m, 30s ago => 3m

                if (closest[j].distance < test->distance - distance_dilution)
                {
                    // g_debug("   Moving %s from %.1fm to %.1fm dop=%.2fm dot=%is", mac, test->distance, closest[j].distance, distance_dilution, time_diff);
                    test = &closest[j];
                }
            }
        }

        struct AccessPoint *ap = get_access_point(test->access_id);

        char* category = category_from_int(closest[i].category);

        int delta_time = difftime(now, test->time);
        double score = 0.55 - atan(delta_time / 42.0 - 4.0) / 3.0;
        // A curve that stays a 1.0 for a while and then drops rapidly around 3 minutes out
        if (score > 0.99) score = 1.0;
        if (score < 0.1) score = 0.0;

        if (score > 0)
        {
            g_debug("  %s %s is at %s for %is at %.1fm score=%.1f", mac, category, ap->client_id, delta_time, test->distance, score);

            total_count += score;
            ap->people_closest_count = ap->people_closest_count + score;
            ap->people_in_range_count = ap->people_in_range_count + score;  // TODO:
        }
    }

    g_info("Total people present %.1f", total_count);

}



/*
    Get the closest recent observation for a device
*/
struct ClosestTo *get_closest_64(int64_t device_64)
{
    struct ClosestTo *best = NULL;

    // Working backwards in time through the array
    for (int i = closest_n - 1; i > 0; i--)
    {
        struct ClosestTo *test = &closest[i];

        if (test->device_64 == device_64)
        {
            if (best == NULL)
            {
                best = test;
            }
            else if (best->access_id == test->access_id)
            {
                // continue, latest hit on access point is most relevant
                // but if the distance is much better and it's fairly recent,
                // go with that distance instead
                double delta_time = difftime(best->time, test->time);
                if (best->distance > test->distance && delta_time < 30){
                    best->distance = test->distance;
                }
            }
            else if (best->distance > test->distance)
            {
                // TODO: Check time too, only recent ones
                double delta_time = difftime(best->time, test->time);

                struct AccessPoint *aptest = get_access_point(test->access_id);
                struct AccessPoint *apbest = get_access_point(best->access_id);

                if (delta_time < 120.0)
                {
                    if (aptest && apbest)
                    {
                        //g_print(" %% Closer to '%s' than '%s', %.1fm < %.1fm after %.1fs\n", aptest->client_id, apbest->client_id, test->distance, best->distance, delta_time);
                    }
                    best = test;
                }
                else
                {
                    if (aptest && apbest)
                    {
                        //g_print(" %% IGNORED Closer to '%s' than '%s', %.1fm < %.1fm after %.1fs\n", aptest->client_id, apbest->client_id, test->distance, best->distance, delta_time);
                    }
                }
            }
        }
    }

    return best;
}


/*
    Get the closest recent observation for a device
*/
struct ClosestTo *get_closest(struct Device* device)
{
    return get_closest_64(mac_string_to_int_64(device->mac));
}



void *listen_loop(void *param)
{
    struct OverallState *state = (struct OverallState *)param;
    GError *error = NULL;

    GInetAddress *iaddr = g_inet_address_new_any(G_SOCKET_FAMILY_IPV4);
    GSocketAddress *addr = g_inet_socket_address_new(iaddr, state->udp_mesh_port);

    GSocket *broadcast_socket = g_socket_new(G_SOCKET_FAMILY_IPV4, G_SOCKET_TYPE_DATAGRAM, G_SOCKET_PROTOCOL_UDP, &error);
    g_assert_no_error(error);

    g_socket_bind(broadcast_socket, addr, TRUE, &error);
    g_assert_no_error(error);

    g_info("Starting listen thread for mesh operation on port %i\n", state->udp_mesh_port);
    g_info("Local client id is %s\n", state->local->client_id);
    g_cancellable_reset(cancellable);

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
        struct AccessPoint a;
        strncpy(a.client_id, "notset", 7);
        strncpy(a.description, "notset", 7);
        strncpy(a.platform, "notset", 7);
        a.x = -1;
        a.y = -1;
        a.z = -1;
        a.people_distance = 0.0;
        a.people_closest_count = 0.0;
        a.people_in_range_count = 0.0;
        a.rssi_factor = 0.0;
        a.rssi_one_meter = 0.0;

        strncpy(d.mac, "notset", 7);  // access point only messages have no device mac address

        if (device_from_json(buffer, &a, &d))
        {
            struct AccessPoint *actual = update_accessPoints(a);
            // Replace with the local interned copy of ap
            a = *actual;

            // ignore messages from self
            if (strcmp(a.client_id, state->local->client_id) == 0)
            {
                //g_print("Ignoring message from self %s : %s\n", a.client_id, d.mac);
                continue;
            }

            // TODO: Ignore messages with no device, access point matrix already updated
            if (d.mac == NULL || strlen(d.mac) == 0){
                g_info("Ignoring access point only message\n");
                continue;
            }

            bool found = false;

            pthread_mutex_lock(&state->lock);

            for (int i = 0; i < state->n; i++)
            {
                if (strncmp(d.mac, state->devices[i].mac, 18) == 0)
                {
                    found = true;

                    int delta_time = difftime(now, d.latest);

                    merge(&state->devices[i], &d, a.client_id, delta_time == 0);

                    if (d.superceededby != 0)
                    {
                        // remove from closest
                        // Use an int64 version of the mac address
                        int64_t id_64 = mac_string_to_int_64(d.mac);
                        add_closest(id_64, a.id, d.latest, d.distance, d.category, d.superceededby);
                    }
                    else 
                    {
                        // This is a current observation, time should match

                        // If the delta time between our clock and theirs is > 0, log it
                        if (delta_time < 0)
                        {
                            // This is problematic, they are ahead of us
                            g_warning("%s '%s' %s dist=%.2fm time=%is", d.mac, d.name, a.client_id, d.distance, delta_time);
                        }
                        else if (delta_time > 1)
                        {
                            // Could be a problem, they appear to be somewhat behind us
                            g_warning("%s '%s' %s dist=%.2fm time=%is", d.mac, d.name, a.client_id, d.distance, delta_time);
                        }
                        else if (delta_time > 0)
                        {
                            g_debug("%s '%s' %s dist=%.2fm time=%is", d.mac, d.name, a.client_id, d.distance, delta_time);
                        }
                        else
                        {
                            // silent, right on zero time difference
                            //g_debug("%s '%s' %s dist=%.2fm time=%is", d.mac, d.name, a.client_id, d.distance, delta_time);
                        }

                        // Use an int64 version of the mac address
                        int64_t id_64 = mac_string_to_int_64(d.mac);
                        add_closest(id_64, a.id, d.latest, d.distance, d.category, d.superceededby);
                    }
                   
                    break;
                }
            }

            if (!found && strncmp(d.mac, "notset", 6) != 0)
            {
                //char* cat = category_from_int(d.category);
                //g_debug("Add foreign device %s %s\n", d.mac, cat);

                int64_t id_64 = mac_string_to_int_64(d.mac);
                add_closest(id_64, a.id, d.latest, d.distance, d.category, d.superceededby);
            }

            pthread_mutex_unlock(&state->lock);
        }
    }
    g_info("Listen thread finished\n");
    return NULL;
}

/*
    Create Socket Service
    You can monitor the socket output using:  sudo nc -l -u -b -k -4 -D -p 7779
*/
GCancellable *create_socket_service(struct OverallState *state)
{
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
    char *json = device_to_json(state->local, device);
    //printf("    %s\n", json);
    udp_send(state->udp_mesh_port, json, strlen(json) + 1);
    free(json);

    // Add local observations into the same structure
    int64_t id_64 = mac_string_to_int_64(device->mac);
    add_closest(id_64, state->local->id, device->latest, device->distance, device->category, device->superceededby);
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
