// Client side implementation of UDP client-server model
#include "udp.h"
#include "utility.h"
#include "rooms.h"
#include "accesspoints.h"

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
#include "cJSON.h"

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


#define CLOSEST_N 2048

// Most recent 2048 closest to observations
static uint closest_n = 0;
static struct ClosestTo closest[CLOSEST_N];

/*
   Mark as superseeded
*/
void mark_superseded(struct AccessPoint* access_point, int64_t device_64, int64_t supersededby)
{
    if (supersededby != 0)
    {
        char mac[18];
        mac_64_to_string(mac, 18, device_64);
        char by[18];
        mac_64_to_string(by, 18, supersededby);
        g_debug("Marking %s in closest as superseded by %s", mac, by);
        for (int j = closest_n; j >= 0; j--)
        {
            if (closest[j].access_point->id == access_point->id && closest[j].device_64 == device_64)
            {
                closest[j].supersededby = supersededby;
            }
        }
        return;
    }
}

/*
   Add a closest observation (get a lock before you call this)
*/
void add_closest(int64_t device_64, struct AccessPoint* access_point, time_t earliest,
    time_t time, float distance, 
    int8_t category, int64_t supersededby, int count)
{
    //g_debug("add_closest(%s, %i, %.2fm, %i)", client_id, access_id, distance, count);
    // First scan back, see if this is an update
    for (int j = closest_n; j >= 0; j--)
    {
        if (closest[j].device_64 == device_64 
            && closest[j].access_point == access_point
            && closest[j].supersededby != supersededby
            && closest[j].time == time)
        {
            char mac[18];
            mac_64_to_string(mac, 18, device_64);
            char from[18];
            mac_64_to_string(from, 18, closest[j].supersededby);
            char to[18];
            mac_64_to_string(to, 18, supersededby);

            g_debug("*** Received an UPDATE %s: changing %s superseded from %s to %s", access_point->client_id, mac, from, to);
            closest[j].supersededby = supersededby;
            return;
        }
    }

    if (supersededby != 0)
    {
        //g_warning("****************** SHOULD NEVER COME HERE *********************");
        char mac[18];
        mac_64_to_string(mac, 18, device_64);
        for (int j = closest_n; j >= 0; j--)
        {
            if (closest[j].device_64 == device_64 && closest[j].access_point->id == access_point->id)
            {
                if (closest[j].supersededby != supersededby)
                {
                    g_info("Removing %s from closest for %s as it's superseded", mac, access_point->client_id);
                    closest[j].supersededby = supersededby;
                }
            }
        }
        // Could trim them from array (no, it might come back)
        // Need to report this right away

        return;
    }

    if (distance < 0.1)     // erroneous value
        return;

    // If the array is full, shuffle it down one
    if (closest_n == CLOSEST_N)
    {
        //g_debug("Trimming closest array");
        g_memmove(&closest[0], &closest[1], sizeof(struct ClosestTo) * (CLOSEST_N - 1));
        closest_n--;
    }

    closest[closest_n].access_point = access_point;
    closest[closest_n].device_64 = device_64;
    closest[closest_n].distance = distance;
    closest[closest_n].category = category;
    closest[closest_n].supersededby = supersededby;
    closest[closest_n].earliest = earliest;
    closest[closest_n].time = time;
    closest[closest_n].count = count;
    closest_n++;

    // And now clean the remainder of the array, removing any for same access, same device
    for (int i = closest_n-2; i >= 0; i--)
    {
        if (closest[i].access_point->id == access_point->id && closest[i].device_64 == device_64)
        {
            memmove(&closest[i], &closest[i+1], sizeof(struct ClosestTo) * (closest_n-1 -i));
            closest_n--;
            break;        // if we always do this there will only ever be one
        }
    }
}


// Mock ML model for calculating which room a set of observations are most likely to be
double room_probability(struct room* r, char * ap_name, double distance)
{
    struct weight* weight = NULL;
    for (weight = r->weights; weight != NULL; weight = weight->next)
    {
        if (strcmp(weight->name, ap_name) == 0)
        {
            double expected_distance = weight->weight;
            // "Activation function"
            if (distance == 0.0 && expected_distance < 0) return 1.0;       // did not expect to see it and cannot see it
            else if (distance == 0.0) {
                // expected a reading, didn't get one
                if (expected_distance > 10.0) return 0.8;       // far out values are unreliable, may come and go
                if (expected_distance > 8.0) return 0.6;
                if (expected_distance > 7.5) return 0.5;
                if (expected_distance > 5.0) return 0.3;
                return 0.2; // expected a reading, didn't get one, could be faulty sensor
            }
            else
            {
                if (expected_distance < 0) return 0.0001;                  // must not be able to see this a/p from this location
                double delta_distance = fabs(distance - expected_distance);
                if (delta_distance < 0.1 * expected_distance) return 0.95;
                if (delta_distance < 0.2 * expected_distance) return 0.90;
                if (delta_distance < 0.3 * expected_distance) return 0.80;
                if (delta_distance < 0.5 * expected_distance) return 0.70;
                return 0.4; // 1.0 / fabs(distance - r->weights[i].weight);
            }
        }
    }
    // did not match, so we have a reading and didn't expect one for this location
    // if we didn't have one that's good
    if (distance == 0) return 1.0;  // wasn't supposed to be here anyway so all good
    else if (distance > 20.0) return 0.99; // far enough away
    // closer it is to somewhere it should not be = higher score
    else if (distance > 10.0) return 0.5;
    else if (distance > 5.0) return 0.2;
    // it should not be here
    else return 0.01;
}


void calculate_location(struct room* room_list, struct AccessPoint* access_points, double accessdistances[N_ACCESS_POINTS])
{
    char line[120];
    line[0] = '\0';

    for (struct room* room = room_list; room != NULL; room = room->next)
    {
        double room_score = 1.0;
        //g_debug("  calculating %s", room->name);
        for (struct AccessPoint* current = access_points; current!=NULL; current = current->next)
        {
            double distance = accessdistances[current->id];
            double score = room_probability(room, current->client_id, distance);
            room_score *= score;
            //g_debug("    %s  %s %.2fm s=%.2f", room->name, accessPoints[j].client_id, distance, score);
        }
        room->room_score = room_score;
    }

    double total_score = 0.00000001;  // non-zero
    double top = 0.0;
    for (struct room* room = room_list; room != NULL; room = room->next)
    {
        total_score += room->room_score;
    }

    // Normalize
    for (struct room* room = room_list; room != NULL; room = room->next)
    {
        room->room_score = room->room_score / total_score;
        if (room->room_score > top) top = room->room_score;
    }

    // log the results in sorted order so they are easier to read

    struct room* sorted[MAX_ROOMS];

    int k = top_k_by_room_score(sorted, MAX_ROOMS, room_list);

    for (int i = 0; i < k; i++)
    {
        struct room* room = sorted[i];    
        // Log that are at least 30% of top score
        if (room->room_score > top / 30.0)
        {
            append_text(line, sizeof(line), "%s:%.2f, ", room->name, room->room_score);
        }
    }

    g_debug("Scores: %s", line);
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
void print_counts_by_closest(struct AccessPoint* access_points_list, struct room* room_list)
{
    float total_count = 0.0;

//    g_debug("Clear room totals");

    for (struct room* current = room_list; current != NULL; current = current->next)
    {
        current->room_total = 0.0;
    }

    g_info(" ");
    g_info("COUNTS (closest contains %i)", closest_n);
    time_t now = time(0);

    for (int i = closest_n - 1; i >= 0; i--)
    {
        closest[i].mark = false;
    }

    int count_examined = 0;
    int count_not_marked = 0;
    int count_in_age_range = 0;

    for (int i = closest_n - 1; i >= 0; i--)
    {
        // parallel array of distances
        double access_distances[N_ACCESS_POINTS];
        for (struct AccessPoint* ap = access_points_list; ap != NULL; ap = ap->next)
        {
            access_distances[ap->id] = 0.0;
        }

        struct ClosestTo* test = &closest[i];

        if (test->category != CATEGORY_PHONE) continue;

        count_examined++;
        if (test->mark) continue;  // already claimed
        count_not_marked++;

        int age = difftime(now, test->time);
        // If this hasn't been seen in > 300s (5min), skip it
        // and therefore all later instances of it 
        if (age > 300) continue;
        count_in_age_range++;

        char mac[18];
        mac_64_to_string(mac, sizeof(mac), test->device_64);
        int count = 0;

        char* category = category_from_int(test->category);

        g_debug("----------------------------------%s-%s--------------------------", mac, category);

        int earliest = difftime(now, test->earliest);

        bool superseded = false;
        int count_same_mac = 0;
        bool skipping = false;

        // mark remainder of array as claimed
        for (int j = i; j >= 0; j--)
        {
            struct ClosestTo* other = &closest[j];

            if (other->device_64 == test->device_64)
            {
                // other is test on first iteration
                other->mark = true;
                if (skipping) continue;

                count += other->count;

                // Is this a better match than the current one?
                int time_diff = difftime(test->time, other->time);
                int abs_diff = difftime(now, other->time);
                // Should always be +ve as we are scanning back in time

                float distance_dilution = time_diff / 10.0;  // 0.1 m/s  1.4m/s human speed
                // e.g. test = 10.0m, current = 3.0m, 30s ago => 3m

                // TODO: Issue: was superseded on one AP but has been seen since in good health
                superseded = superseded | ((other->supersededby != 0) && (count_same_mac < 2));
                // ignore a superseded value if it was a long time ago and we've seen other reports since then from it
                count_same_mac++;

                if (time_diff > 300)
                {
                    g_debug("Skip remainder, delta time %i > 300", time_diff);
                    skipping = true;
                    continue;
                }

                //if (time_diff < 300)      // only interested in where it has been recently // handled above in outer loop
                {
                    char other_mac[18];
                    mac_64_to_string(other_mac, 18, other->supersededby);

                    struct AccessPoint *ap2 = other->access_point;

                    g_debug(" %10s distance %5.1fm at=%3is dt=%3is count=%3i %s%s", ap2->client_id, other->distance, abs_diff, time_diff, other->count,
                        // lazy concat
                        other->supersededby==0 ? "" : "superseeded=", 
                        other->supersededby==0 ? "" : other_mac);

                    int index = other->access_point->id;
                    access_distances[index] = round(other->distance * 10.0) / 10.0;

                    // other needs to be better than test by at least as far as test could have moved in that time interval
                    if (other->distance < test->distance - distance_dilution)
                    {
                        // g_debug("   Moving %s from %.1fm to %.1fm dop=%.2fm dot=%is", mac, test->distance, closest[j].distance, distance_dilution, time_diff);
                        test = other;
                    }
                }
            }
        }

        // JSON
        char *json = NULL;
        cJSON *jobject = cJSON_CreateObject();

        for (struct AccessPoint* current = access_points_list; current != NULL; current = current->next)
        {
            // remove 'if' for machine consumption
            //if (access_distances[current->id] != 0)
            {
                cJSON_AddNumberToObject(jobject, current->client_id, access_distances[current->id]);
            }
        }

        json = cJSON_PrintUnformatted(jobject);
        cJSON_Delete(jobject);

        // Summary of access distances
        g_debug("%s", json);
        free(json);

        struct AccessPoint *ap = test->access_point;

        int delta_time = difftime(now, test->time);
        double score = 0.55 - atan(delta_time / 42.0 - 4.0) / 3.0;
        // A curve that stays a 1.0 for a while and then drops rapidly around 3 minutes out
        if (score > 0.99) score = 1.0;
        if (score < 0.1) score = 0.0;

        // Model the uncertainty when a new device arrives. On first tick it could
        if (count < 2)
        {
            score = score * 0.25;
        }
        else if (count < 3)
        {
            score = score * 0.5;
        }
        else if (count < 4)
        {
            score = score * 0.75;
        }

        if (score > 0)
        {
            // SVM model goes here to convert access point distances to locations


            calculate_location(room_list, access_points_list, access_distances);

            // g_debug("   %s %s is at %16s for %3is at %4.1fm score=%.1f count=%i%s", mac, category, ap->client_id, delta_time, test->distance, score,
            //     count, test->distance > 7.5 ? " * TOO FAR *": "");

            // Several observations, some have it superseded, some don't either because they know it
            // wasn't or because they didn't see the later mac address.
            if (superseded)
            {
                g_info("Superseded (earliest=%4is, chosen=%4is latest=%4is) count=%i score=%.2f", -earliest, -delta_time, -age, count, score);
            }
            else //if (test->distance < 7.5)
            {
                g_info("Cluster (earliest=%4is, chosen=%4is latest=%4is) count=%i dist=%.1f score=%.2f", -earliest, -delta_time, -age, count, test->distance, score);

                total_count += score;
                ap->people_closest_count = ap->people_closest_count + score;
                ap->people_in_range_count = ap->people_in_range_count + score;  // TODO:

                //g_debug("Update room total %i", room_count);
                for (struct room* rcurrent = room_list; rcurrent != NULL; rcurrent = rcurrent->next)
                {
                    rcurrent->room_total += rcurrent->room_score * score;        // probability x incidence
                }
            }
            // else 
            // {
            //     // Log too-far away macs ?
            //     g_info("%s Far %s (earliest=%4is, chosen=%4is latest=%4is) count=%i dist=%.1f score=%.2f", mac, category, -earliest, -delta_time, -age, count, test->distance, score);
            //     g_info("  %s", json);
            // }
        }
        else
        {
            g_debug("   score %.2f", score);
        }
        g_debug(" ");

    }

    // Now display totals

    char *json_rooms = NULL;
    cJSON *jobject_rooms = cJSON_CreateObject();

    for (struct room* r = room_list; r != NULL; r = r->next)
    {
        if (r->room_total > 0.0)
        {
            cJSON_AddNumberToObject(jobject_rooms, r->name, round(r->room_total*10.0) / 10.0);
        }
    }

    json_rooms = cJSON_PrintUnformatted(jobject_rooms);
    cJSON_Delete(jobject_rooms);

    g_info(" ");
    g_info("==============================================================================================");
    g_info(" ");
    g_debug("Examined %i > %i > %i > %i", closest_n, count_examined, count_not_marked, count_in_age_range);
    g_info("Summary by room: %s", json_rooms);

    free(json_rooms);

    g_info(" ");
    g_info("Total people present %.2f", total_count);
    g_info(" ");
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
            else if (best->access_point->id == test->access_point->id)
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

                struct AccessPoint *aptest = test->access_point;
                struct AccessPoint *apbest = best->access_point;

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
            struct AccessPoint *actual = update_accessPoints(&state->access_points, a);

            // ignore messages from self
            if (strcmp(a.client_id, state->local->client_id) == 0)
            {
                //g_print("Ignoring message from self %s : %s\n", a.client_id, d.mac);
                continue;
            }

            // TODO: Ignore messages with no device, access point matrix already updated
            if (d.mac == NULL || strlen(d.mac) == 0){
                g_info("Ignoring access point only message");
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

                    if (d.supersededby != 0)
                    {
                        // remove from closest
                        int64_t id_64 = mac_string_to_int_64(d.mac);
                        mark_superseded(actual, id_64, d.supersededby);
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
                        // Problematic now we send older updates for superseding events
                        // else if (delta_time > 1)
                        // {
                        //     // Could be a problem, they appear to be somewhat behind us
                        //     g_warning("UPDATE %s '%s' %s dist=%.2fm time=%is", d.mac, d.name, a.client_id, d.distance, delta_time);
                        // }
                        // else if (delta_time > 0)
                        // {
                        //     g_debug("UPDATE %s '%s' %s dist=%.2fm time=%is", d.mac, d.name, a.client_id, d.distance, delta_time);
                        // }
                        // else
                        // {
                        //     // silent, right on zero time difference
                        //     //g_debug("%s '%s' %s dist=%.2fm time=%is", d.mac, d.name, a.client_id, d.distance, delta_time);
                        // }

                        // Use an int64 version of the mac address
                        int64_t id_64 = mac_string_to_int_64(d.mac);
                        add_closest(id_64, actual, d.earliest,
                            d.latest, d.distance, d.category, d.supersededby, d.count);
                    }
                   
                    break;
                }
            }

            if (!found && strncmp(d.mac, "notset", 6) != 0)
            {
                //char* cat = category_from_int(d.category);
                //g_debug("Add foreign device %s %s\n", d.mac, cat);

                int64_t id_64 = mac_string_to_int_64(d.mac);
                add_closest(id_64, actual, d.earliest, d.latest, d.distance, d.category, d.supersededby, d.count);
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
}


/*
    Update closest
*/
void update_closest(struct OverallState *state, struct Device *device)
{
    //g_debug("update_closest(%s, %i, %s)", state->local->client_id, state->local->id, device->mac);
    // Add local observations into the same structure
    int64_t id_64 = mac_string_to_int_64(device->mac);
    add_closest(id_64, state->local, device->earliest, device->latest, device->distance, device->category, device->supersededby, device->count);
}

/*
    Update superseded
*/
void update_superseded(struct OverallState *state, struct Device *device)
{
    //g_debug("update_superseded(%i, %s)", state->local->id, device->mac);
    // Add local observations into the same structure
    int64_t id_64 = mac_string_to_int_64(device->mac);
    add_closest(id_64, state->local, device->earliest, device->latest, device->distance, device->category, device->supersededby, device->count);
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
