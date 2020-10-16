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
#include <time.h>
#include <math.h>
#include "cJSON.h"
#include "knn.h"

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
        //g_debug("No interface running, skip UDP send");
    }
}

GCancellable *cancellable;
pthread_t listen_thread;


/*
   Mark as superseeded
*/
void mark_superseded(struct OverallState* state, struct AccessPoint* access_point, int64_t device_64, int64_t supersededby)
{
    if (supersededby != 0)
    {
        char mac[18];
        mac_64_to_string(mac, 18, device_64);
        char by[18];
        mac_64_to_string(by, 18, supersededby);
        g_debug("Marking %s in closest as superseded by %s", mac, by);
        for (int j = state->closest_n-1; j >= 0; j--)
        {
            g_assert(state->closest[j].access_point != NULL);
            if (state->closest[j].access_point->id == access_point->id && state->closest[j].device_64 == device_64)
            {
                state->closest[j].supersededby = supersededby;
            }
        }
        g_debug("Marked superseded");
        return;
    }
}

/*
   Add a closest observation (get a lock before you call this)
*/
void add_closest(struct OverallState* state, int64_t device_64, struct AccessPoint* access_point, time_t earliest,
    time_t time, float distance, 
    int8_t category, int64_t supersededby, int count, char* name, bool is_training_beacon)
{
    g_assert(access_point != NULL);
    //g_debug("add_closest(%s, %i, %.2fm, %i)", client_id, access_id, distance, count);
    // First scan back, see if this is an update
    for (int j = state->closest_n-1; j >= 0; j--)
    {
        g_assert(state->closest[j].access_point != NULL);
        if (state->closest[j].device_64 == device_64 
            && state->closest[j].access_point->id == access_point->id
            && state->closest[j].supersededby != supersededby
            && state->closest[j].time == time)
        {
            char mac[18];
            mac_64_to_string(mac, 18, device_64);
            char from[18];
            mac_64_to_string(from, 18, state->closest[j].supersededby);
            char to[18];
            mac_64_to_string(to, 18, supersededby);

            g_debug("*** Received an UPDATE %s: changing %s superseded from %s to %s", access_point->client_id, mac, from, to);
            state->closest[j].supersededby = supersededby;
            return;
        }
    }

    if (supersededby != 0)
    {
        //g_warning("****************** SHOULD NEVER COME HERE *********************");
        char mac[18];
        mac_64_to_string(mac, 18, device_64);
        for (int j = state->closest_n-1; j >= 0; j--)
        {
            g_assert(state->closest[j].access_point != NULL);
            if (state->closest[j].device_64 == device_64 && state->closest[j].access_point->id == access_point->id)
            {
                if (state->closest[j].supersededby != supersededby)
                {
                    g_info("Removing %s from closest for %s as it's superseded", mac, access_point->client_id);
                    state->closest[j].supersededby = supersededby;
                }
            }
        }
        // Could trim them from array (no, it might come back)
        // Need to report this right away

        return;
    }

    if (distance < 0.01)     // erroneous value
        return;

    // If the array is full, shuffle it down one
    if (state->closest_n == CLOSEST_N)
    {
        //g_debug("Trimming closest array");
        memmove(&state->closest[0], &state->closest[1], sizeof(struct ClosestTo) * (CLOSEST_N - 1));
        state->closest_n--;
    }

    g_assert(access_point != NULL);
    state->closest[state->closest_n].access_point = access_point;
    state->closest[state->closest_n].device_64 = device_64;
    state->closest[state->closest_n].distance = distance;
    state->closest[state->closest_n].category = category;
    state->closest[state->closest_n].supersededby = supersededby;
    state->closest[state->closest_n].earliest = earliest;
    state->closest[state->closest_n].time = time;
    state->closest[state->closest_n].count = count;
    state->closest[state->closest_n].is_training_beacon = is_training_beacon;
    strncpy(state->closest[state->closest_n].name, name, META_LENGTH);           // debug only, remove this later
    state->closest_n++;

    // And now clean the remainder of the array, removing any for same access, same device
    for (int i = state->closest_n-2; i >= 0; i--)
    {
        g_assert(state->closest[i].access_point != NULL);
        if (state->closest[i].access_point->id == access_point->id && state->closest[i].device_64 == device_64)
        {
            memmove(&state->closest[i], &state->closest[i+1], sizeof(struct ClosestTo) * (state->closest_n-1 -i));
            state->closest_n--;
            break;        // if we always do this there will only ever be one
        }
    }
}

/*
   Calculates room scores using access point distances
*/
void calculate_location(struct OverallState* state, struct ClosestTo* closest, double accessdistances[N_ACCESS_POINTS], double accesstimes[N_ACCESS_POINTS], float time_score)
{
    (void)accesstimes;

    struct patch* room_list = state->patches;
    struct AccessPoint* access_points = state->access_points;

    char* device_name = closest->name;
    //const char* category = category_from_int(closest->category);
    bool is_training_beacon = FALSE;

    struct top_k best_three[3];
    int k_found = k_nearest(state->recordings, accessdistances, access_points, best_three, 3);

    if (k_found == 0) { g_warning("Did not find a nearest k"); }

    struct top_k best = best_three[0];
    // TODO: Not just top 1, use top few

    bool found = FALSE;

    for (struct patch* room = room_list; room != NULL; room = room->next)
    {
        if (strcmp(room->name, best.patch_name) == 0)
        {
            found = TRUE;
            room->knn_score = 1.0;
            // don't break, need to set rest to zero
        }
        else
        {
            room->knn_score = 0.0;
        }
    }

    if (!found) { g_warning("Did not find %s", best.patch_name); }


    time_t now = time(0);
    if (difftime(now, closest->time) > 60)
    {
        g_debug("Old, nearest to '%s' score=%.2f * %.2f", best.patch_name, best.distance, time_score);
        //g_debug("Skip CSV, old data %fs", difftime(now, closest->time));
    }
    else 
    {
        //g_debug("CSV: %s", csv);

        // TODO: Record Beacons separately?

        // RECORD TRAINING DATA
        if ((is_training_beacon) && (strcmp(closest->name, "iPhone") != 0))
        {
            if (best.distance < 1.0 && strncmp(best.patch_name, device_name, META_LENGTH) == 0)
            {
                // skip, we already have a good enough recording with the SAME name
               g_debug("Training: Skip, nearest to '%s' score=%.2f * %.2f", best.patch_name, best.distance, time_score);
            }
            else
            {
                record("recordings", device_name, accessdistances, access_points, device_name);
                g_debug("Training: Nearest was '%s', score=%.2f * %.2f", best.patch_name, best.distance, time_score);
            }
        }
        else if (closest->category == CATEGORY_BEACON)
        {
            record("beacons", device_name, accessdistances, access_points, device_name);
            g_debug("Beacon: Nearest was '%s', score=%.2f * %.2f", best.patch_name, best.distance, time_score);
        }
        else
        {
            g_debug("Nearest to '%s' score=%.2f * %.2f", best.patch_name, best.distance, time_score);
        }
        //g_debug("Scores: %s", line);

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
    update_group_summaries combines all the room totals into area totals
*/
void update_group_summaries(struct OverallState* state)
{
    // Create group summaries (a flat list of a hierarchy of groups)
    for (struct area* g = state->areas; g != NULL; g = g->next)
    {
        g->beacon_total = 0.0;
        g->computer_total = 0.0;
        g->phone_total = 0.0;
        g->tablet_total = 0.0;
        g->watch_total = 0.0;
    }

    for (struct patch* r = state->patches; r != NULL; r = r->next)
    {
        struct area* g = r->area;
        g->beacon_total += r->beacon_total;
        g->computer_total += r->computer_total;
        g->phone_total += r->phone_total;
        g->tablet_total += r->tablet_total;
        g->watch_total += r->watch_total;
    }
}

/*
    Add a one decimal value to a JSON object
*/
void cJSON_AddRounded(cJSON * item, const char* label, double value)
{
    char print_num[18];
    snprintf(print_num, 18, "%.1f", value);
    cJSON_AddRawToObject(item, label, print_num);
}

/*
    Find counts by access point
*/
void print_counts_by_closest(struct OverallState* state)
{
    struct AccessPoint* access_points_list = state->access_points;
    struct Beacon* beacon_list = state->beacons;
    struct patch* patch_list = state->patches;

    float total_count = 0.0;

//    g_debug("Clear room totals");

    for (struct patch* current = patch_list; current != NULL; current = current->next)
    {
        current->phone_total = 0.0;
        current->tablet_total = 0.0;
        current->computer_total = 0.0;
        current->watch_total = 0.0;
        current->beacon_total = 0.0;
    }

    // TODO: Make this lazy, less often?
    free_list(&state->recordings);
    int count_recordings = 0;
    //if (state->recordings == NULL)
    {
        g_info("Re-read observations files");
        bool ok = read_observations ("recordings", state->access_points, &state->recordings, &state->patches, &state->areas);
        if (!ok) g_warning("Failed to read file back");
        for (struct recording* r = state->recordings; r != NULL; r=r->next)
        {
            count_recordings++;
        }
    }

    g_info(" ");
    g_info("COUNTS (closest contains %i, recordings contains %i)", state->closest_n, count_recordings);
    time_t now = time(0);

    for (int i = state->closest_n - 1; i >= 0; i--)
    {
        g_assert(state->closest[i].access_point != NULL);
        state->closest[i].mark = false;
    }

    int count_examined = 0;
    int count_not_marked = 0;
    int count_in_age_range = 0;

    for (int i = state->closest_n - 1; i >= 0; i--)
    {
        g_assert(state->closest[i].access_point != NULL);

        // parallel array of distances and times
        double access_distances[N_ACCESS_POINTS];       // latest observation distance
        double access_times[N_ACCESS_POINTS];           // latest observation time
        for (struct AccessPoint* ap = access_points_list; ap != NULL; ap = ap->next)
        {
            access_distances[ap->id] = 0.0;
            access_times[ap->id] = 0.0;
        }

        struct ClosestTo* test = &state->closest[i];

        // moved down to counting ... if (test->category != CATEGORY_PHONE) continue;

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

        g_debug("------------------------ %s --- %s ---- %s -----------------", mac, category, test->name);

        int earliest = difftime(now, test->earliest);

        bool superseded = false;
        int count_same_mac = 0;
        bool skipping = false;

        // mark remainder of array as claimed
        for (int j = i; j >= 0; j--)
        {
            struct ClosestTo* other = &state->closest[j];

            // Same MAC, different name - ignore so you can take an iPhone around as a beacon and change name
            if (other->device_64 == test->device_64 && strcmp(other->name, test->name)!=0)
            {
                other->mark = true;
                if (skipping) continue;
            }
            else if (other->device_64 == test->device_64 && strcmp(other->name, test->name)==0)
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

                //if (time_diff < 300)      // only interested in where it has been recently // handled above in outer loop
                {
                    char other_mac[18];
                    mac_64_to_string(other_mac, 18, other->supersededby);

                    struct AccessPoint *ap2 = other->access_point;
                    g_debug(" %10s distance %5.1fm at=%3is dt=%3is count=%3i %s%s", ap2->client_id, other->distance, abs_diff, time_diff, other->count,
                        // lazy concat
                        other->supersededby==0 ? "" : "superseeded=", 
                        other->supersededby==0 ? "" : other_mac);

                    if (time_diff > 300)
                    {
                        //g_debug("Skip remainder, delta time %i > 300", time_diff);
                        skipping = true;
                        continue;
                    }

                    int index = other->access_point->id;
                    access_distances[index] = round(other->distance * 10.0) / 10.0;
                    access_times[index] = abs_diff;

                    // other needs to be better than test by at least as far as test could have moved in that time interval
                    if (other->distance < test->distance - distance_dilution)
                    {
                        //g_debug("   Moving %s from %.1fm to %.1fm dop=%.2fm dot=%is", mac, test->distance, closest[j].distance, distance_dilution, time_diff);
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
            // remove 'if' for analysis consumption if fixed columns are needed
            if (access_distances[current->id] != 0)
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
            score = score * 0.5;
        }
        else if (count < 3)
        {
            score = score * 0.75;
        }
        else if (count < 4)
        {
            score = score * 0.95;
        }

        if (score > 0)
        {
            calculate_location(state, test, access_distances, access_times, score);

            // g_debug("   %s %s is at %16s for %3is at %4.1fm score=%.1f count=%i%s", mac, category, ap->client_id, delta_time, test->distance, score,
            //     count, test->distance > 7.5 ? " * TOO FAR *": "");

            // Several observations, some have it superseded, some don't either because they know it
            // wasn't or because they didn't see the later mac address.
            if (superseded)
            {
                g_info("Superseded (earliest=%4is, chosen=%4is latest=%4is) count=%i score=%.2f", -earliest, -delta_time, -age, count, score);
            }
            else if (test->category == CATEGORY_TABLET)
            {
                for (struct patch* rcurrent = patch_list; rcurrent != NULL; rcurrent = rcurrent->next)
                {
                    rcurrent->tablet_total += rcurrent->knn_score * score;        // probability x incidence
                }
            }
            else if (test->category == CATEGORY_COMPUTER)
            {
                for (struct patch* rcurrent = patch_list; rcurrent != NULL; rcurrent = rcurrent->next)
                {
                    rcurrent->computer_total += rcurrent->knn_score * score;        // probability x incidence
                }
            }
            else if (test->category == CATEGORY_WEARABLE)
            {
                for (struct patch* rcurrent = patch_list; rcurrent != NULL; rcurrent = rcurrent->next)
                {
                    if (rcurrent->knn_score > 0)
                        g_debug("Watch in %s +%.2f x %.2f", rcurrent->name, rcurrent->knn_score, score);
                    rcurrent->watch_total += rcurrent->knn_score * score;        // probability x incidence
                }
            }
            else if (test->category == CATEGORY_BEACON)
            {
                for (struct patch* rcurrent = patch_list; rcurrent != NULL; rcurrent = rcurrent->next)
                {
                    rcurrent->beacon_total += rcurrent->knn_score * score;        // probability x incidence
                }
            }
            else if (test->category == CATEGORY_PHONE)
            {
                //g_info("Cluster (earliest=%4is, chosen=%4is latest=%4is) count=%i score=%.2f", -earliest, -delta_time, -age, count, score);

                total_count += score;
                ap->people_closest_count = ap->people_closest_count + score;
                ap->people_in_range_count = ap->people_in_range_count + score;  // TODO:

                //g_debug("Update room total %i", room_count);
                for (struct patch* rcurrent = patch_list; rcurrent != NULL; rcurrent = rcurrent->next)
                {
                    if (rcurrent->knn_score > 0)
                        g_debug("Phone in %s +%.2f x %.2f", rcurrent->name, rcurrent->knn_score, score);
                    rcurrent->phone_total += rcurrent->knn_score * score;        // probability x incidence
                }
            }
            else //if (test->distance < 7.5)
            {
                // only count phones for occupancy
            }
            // else 
            // {
            //     // Log too-far away macs ?
            //     g_info("%s Far %s (earliest=%4is, chosen=%4is latest=%4is) count=%i dist=%.1f score=%.2f", mac, category, -earliest, -delta_time, -age, count, test->distance, score);
            //     g_info("  %s", json);
            // }

            // Is this a known Device that we want to track?
            struct Beacon* beacon = NULL;
            for (struct Beacon* b = beacon_list; b != NULL; b=b->next)
            {
                if (strcmp(b->name, test->name) == 0 || b->mac64 == test->device_64)
                {
                    beacon = b;
                    break;
                }
            }

            if (beacon != NULL)
            {
                double best_room = 0.0; // for beacon
                for (struct patch* rcurrent = patch_list; rcurrent != NULL; rcurrent = rcurrent->next)
                {
                    // If this is a known beacon and the score is > best, place it in this room
                    if (rcurrent->knn_score > best_room)
                    {
                        best_room = rcurrent->knn_score;
                        beacon->patch = rcurrent;
                        beacon->last_seen = test->time;
                    }
                }
            }

        }
        else
        {
            g_debug("   score %.2f", score);
        }
        g_debug(" ");

    }

    // Now display totals
    update_group_summaries(state);

    char *json_rooms = NULL;
    cJSON *jobject = cJSON_CreateObject();

    cJSON *jareas = cJSON_AddArrayToObject(jobject, "areas");
    cJSON *jzones = cJSON_AddArrayToObject(jobject, "categories");
    cJSON *jbeacons = cJSON_AddArrayToObject(jobject, "assets");

    for (struct patch* r = patch_list; r != NULL; r = r->next)
    {
        if (r->phone_total + r->computer_total + r->tablet_total + r->watch_total == 0.0) continue;

        cJSON* item = cJSON_CreateObject();
        cJSON_AddStringToObject(item, "name", r->name);
        cJSON_AddStringToObject(item, "category", r->area->category);
        cJSON_AddStringToObject(item, "tags", r->area->tags);

        cJSON_AddRounded(item, "phones", r->phone_total);
        cJSON_AddRounded(item, "watches", r->watch_total);
        cJSON_AddRounded(item, "tablets", r->tablet_total);
        cJSON_AddRounded(item, "computers", r->computer_total);
        cJSON_AddItemToArray(jareas, item);
    }

    struct summary* summary = NULL;

    for (struct area* a = state->areas; a != NULL; a = a->next)
    {
        // Include zeros - easier for some to parse
        update_summary(&summary, a->category, a->phone_total);
    }

    for (struct summary* s = summary; s != NULL; s = s->next)
    {
        cJSON* item = cJSON_CreateObject();
        cJSON_AddStringToObject(item, "name", s->category);
        cJSON_AddRounded(item, "phones", s->total);
        cJSON_AddItemToArray(jzones, item);
    }

    free_summary(&summary);

    g_info(" ");
    g_info("==============================================================================================");

    g_info ("              Beacon                Area             Zone        When");
    g_info("----------------------------------------------------------------------------------------------");
    for (struct Beacon* b = state->beacons; b != NULL; b=b->next)
    {
        if (b->patch != NULL)
        {
            char ago[20];
            double diff = b->last_seen == 0 ? -1 : difftime(now, b->last_seen) / 60.0;
            if (diff < 2) snprintf(ago, sizeof(ago), "now");
            else if (diff < 60) snprintf(ago, sizeof(ago), "%.0f min ago", diff);
            else if (diff < 24*60) snprintf(ago, sizeof(ago), "%.1f hours ago", diff / 60.0);
            else snprintf(ago, sizeof(ago), "%.1f days ago", diff / 24.0 / 60.0);

            const char* room_name =  (b->patch == NULL) ? "---" : b->patch->name;
            const char* category = (b->patch == NULL) ? "---" : ((b->patch->area == NULL) ? "???" : b->patch->area->category);

            g_info("%20s  %18s %16s        %s",
                b->alias, room_name, 
                category,
                ago);
            
            cJSON* item = cJSON_CreateObject();
            cJSON_AddStringToObject(item, "alias", b->alias);
            cJSON_AddStringToObject(item, "room", room_name);
            cJSON_AddStringToObject(item, "category", category);
            cJSON_AddStringToObject(item, "ago", ago);
            cJSON_AddNumberToObject(item, "t", b->last_seen);

            cJSON_AddItemToArray(jbeacons, item);
        }
    }

    g_info(" ");
    g_info("==============================================================================================");
    g_info(" ");
    g_debug("Examined %i > %i > %i > %i", state->closest_n, count_examined, count_not_marked, count_in_age_range);

    json_rooms = cJSON_PrintUnformatted(jobject);
    //json_rooms = cJSON_Print(jobject);
    cJSON_Delete(jobject);

    if (state->json != NULL)
    {
        // free(json_rooms); but on next cycle
        free(state->json);
    }
    state->json = json_rooms;

    g_info("Summary by room: %s", json_rooms);
    g_info(" ");
    g_info("Total people present %.2f", total_count);
    g_info(" ");
}



/*
    Get the closest recent observation for a device
*/
struct ClosestTo *get_closest_64(struct OverallState* state, int64_t device_64)
{
    struct ClosestTo *best = NULL;

    // Working backwards in time through the array
    for (int i = state->closest_n - 1; i > 0; i--)
    {
        struct ClosestTo *test = &state->closest[i];
        g_assert(test->access_point != NULL);

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
struct ClosestTo *get_closest(struct OverallState* state, struct Device* device)
{
    return get_closest_64(state, mac_string_to_int_64(device->mac));
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
        struct AccessPoint dummy;
        strncpy(dummy.client_id, "notset", 7);
        strncpy(dummy.description, "notset", 7);
        strncpy(dummy.platform, "notset", 7);
        dummy.id = -1;
        dummy.x = -1;
        dummy.y = -1;
        dummy.z = -1;
        dummy.people_distance = 0.0;
        dummy.people_closest_count = 0.0;
        dummy.people_in_range_count = 0.0;
        dummy.rssi_factor = 0.0;
        dummy.rssi_one_meter = 0.0;

        strncpy(d.mac, "notset", 7);  // access point only messages have no device mac address

        if (device_from_json(buffer, &dummy, &d))
        {
            struct AccessPoint *actual = update_accessPoints(&state->access_points, dummy);
            g_assert(actual != NULL);

            // ignore messages from self
            if (strcmp(actual->client_id, state->local->client_id) == 0)
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

                    merge(&state->devices[i], &d, actual->client_id, delta_time == 0);

                    if (d.supersededby != 0)
                    {
                        // remove from closest
                        int64_t id_64 = mac_string_to_int_64(d.mac);
                        mark_superseded(state, actual, id_64, d.supersededby);
                    }
                    else 
                    {
                        // This is a current observation, time should match

                        // If the delta time between our clock and theirs is > 0, log it
                        if (delta_time < 0)
                        {
                            // This is problematic, they are ahead of us
                            g_warning("%s '%s' %s dist=%.2fm time=%is", d.mac, d.name, actual->client_id, d.distance, delta_time);
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
                        g_assert(actual != NULL);

                        // char* cat = category_from_int(d.category);
                        // g_debug("Update from UDP: %s %s %s\n", d.mac, d.name, cat);

                        add_closest(state, id_64, actual, d.earliest, d.latest, d.distance, d.category, d.supersededby, d.count, d.name, d.is_training_beacon);
                    }
                   
                    break;
                }
            }

            if (!found && strncmp(d.mac, "notset", 6) != 0)
            {
                // char* cat = category_from_int(d.category);
                // g_debug("Add from UDP: %s %s %s\n", d.mac, d.name, cat);

                int64_t id_64 = mac_string_to_int_64(d.mac);
                g_assert(actual != NULL);
                add_closest(state, id_64, actual, d.earliest, d.latest, d.distance, d.category, d.supersededby, d.count, d.name, d.is_training_beacon);
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
    add_closest(state, id_64, state->local, device->earliest, device->latest, device->distance, device->category, device->supersededby, device->count, device->name, device->is_training_beacon);
}

/*
    Update superseded
*/
void update_superseded(struct OverallState *state, struct Device *device)
{
    //g_debug("update_superseded(%i, %s)", state->local->id, device->mac);
    // Add local observations into the same structure
    int64_t id_64 = mac_string_to_int_64(device->mac);
    g_assert(state->local != NULL);
    add_closest(state, id_64, state->local, device->earliest, device->latest, device->distance, device->category, device->supersededby, device->count, device->name, device->is_training_beacon);
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
