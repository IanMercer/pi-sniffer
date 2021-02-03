/*
    Code for calculating on the closest list which is accumulated from all the sensors in the system
*/

#include "closest.h"

#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <glib.h>
#include <json-glib/json-glib.h>

#include <sys/types.h>
#include <stdio.h>
#include <inttypes.h>

#include <time.h>
#include <math.h>
#include "cJSON.h"
#include "knn.h"

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
            && state->closest[j].access_point->id == access_point->id)
        {
            if (state->closest[j].supersededby != supersededby && state->closest[j].time == time)
            {
                char mac[18];
                mac_64_to_string(mac, 18, device_64);
                char from[18];
                mac_64_to_string(from, 18, state->closest[j].supersededby);
                char to[18];
                mac_64_to_string(to, 18, supersededby);

                g_trace("*** Received an UPDATE %s: changing %s superseded from %s to %s", access_point->client_id, mac, from, to);
                state->closest[j].supersededby = supersededby;
                return;
            }
            else if (state->closest[j].count == count && state->closest[j].distance == distance)
            {
                //char mac[18];
                //mac_64_to_string(mac, 18, device_64);
                //g_warning("Update is identical, skipping %s %s", access_point->client_id, mac);
                return;
            }
        }
    }

    // if (supersededby != 0)
    // {
    //     g_warning("****************** SHOULD NEVER COME HERE *********************");
    //     char mac[18];
    //     mac_64_to_string(mac, 18, device_64);
    //     for (int j = state->closest_n-1; j >= 0; j--)
    //     {
    //         g_assert(state->closest[j].access_point != NULL);
    //         if (state->closest[j].device_64 == device_64 && state->closest[j].access_point->id == access_point->id)
    //         {
    //             if (state->closest[j].supersededby != supersededby)
    //             {
    //                 g_debug("Removing %s from closest for %s as it's superseded", mac, access_point->client_id);
    //                 state->closest[j].supersededby = supersededby;
    //             }
    //         }
    //     }
    //     // Could trim them from array (no, it might come back)
    //     // Need to report this right away

    //     return;
    // }

    if (distance < 0.01)     // erroneous value
        return;

    // TODO: Move closest to a circular buffer and stop shifting it in memory

    // If it's already here, remove it
    for (int i = state->closest_n-1; i >= 0; i--)
    {
        g_assert(state->closest[i].access_point != NULL);
        if (state->closest[i].access_point->id == access_point->id && state->closest[i].device_64 == device_64
        // let's try keeping ones that indicate movement and removing any stationary ones
        // let's not && fabs(state->closest[i].distance - distance) < 1.0
         )
        {
            if (i < state->closest_n-1)
            { 
              memmove(&state->closest[i], &state->closest[i+1], sizeof(struct ClosestTo) * (state->closest_n-1 -i));
            }
            state->closest_n--;
            break;        // if we always do this there will only ever be one
        }
    }

    // If the array is full, shuffle it down one
    if (state->closest_n == CLOSEST_N)
    {
        g_debug("Trimming closest array");
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
    strncpy(state->closest[state->closest_n].name, name, NAME_LENGTH);           // debug only, remove this later

    // If it's a known MAC address or name of a beacon, update the name to the alias
    for (struct Beacon* b = state->beacons; b != NULL; b = b->next)
    {
        if (strcmp(b->name, name) == 0 || b->mac64 == device_64)
        {
            g_utf8_strncpy(state->closest[state->closest_n].name, b->alias, NAME_LENGTH);
        }
    }
    state->closest_n++;
}



/*
   Calculates room scores using access point distances
*/
void calculate_location(struct OverallState* state, struct ClosestTo* closest, 
    double accessdistances[N_ACCESS_POINTS], double accesstimes[N_ACCESS_POINTS], 
    bool is_training_beacon, bool loggingOn, bool debug)
{
    (void)accesstimes;

    for (struct patch* patch = state->patches; patch != NULL; patch = patch->next)
    {
        patch->knn_score = 0.0;
    }

    struct AccessPoint* access_points = state->access_points;

    char* device_name = closest->name;
    //const char* category = category_from_int(closest->category);

    struct top_k best_three[3];
    // try confirmed
    int k_found = k_nearest(state->recordings, accessdistances, access_points, best_three, 3, TRUE, debug);

    // if (k_found < 3)
    // {
    //     // Try again including unconfirmed recordings (beacon subdirectory)
    //     k_found = k_nearest(state->recordings, accessdistances, access_points, best_three, 3, FALSE);
    // }

    if (k_found == 0) 
    { 
        g_warning("Did not find a nearest k"); 
    }
    else 
    {
        // if (!found) 
        // { 
        //     g_warning("Did not find a patch called %s, creating one on the fly", best->patch_name);
        //     char* world = strdup("World");
        //     char* tags = strdup("zone=here");
        //     struct patch* patch = get_or_create_patch(best->patch_name, best->patch_name, world, tags, &state->patches, &state->groups, FALSE);
        //     patch->knn_score = 1.0;
        //     best->distance = 1.0;
        //     strncpy(best->patch_name, patch->name, META_LENGTH);
        // }

        // Not really logging on, it's whether the readings have been updated since last scan
        //if (!loggingOn) return;

        time_t now = time(0);
        if (difftime(now, closest->time) > 300)
        {
            g_debug("Old '%s' score: %.3f", best_three[0].patch_name, best_three[0].distance);
        }
        else 
        {
            // Allocate probabilities to patches instead of 1.0 and 0.0 only
            double allocation = 1.0;
            for (int bi = 0; bi < k_found; bi++)
            {
                // Adjust this to control how probability is spread over possibiities
                // lower number = less precise, more spread 5 = too much spread
                double scale_factor = 10.0;
                // 0.180 0.179 => 0.001 * 100 = 0.1 
                double pallocation =  allocation * (bi < k_found-1 ?
                    fmin(1.0, 0.5 + 10 * (best_three[bi].distance - best_three[bi+1].distance)) : 
                    1.0);
                    // e.g. 0.426, 0.346, 0.289 => 0.080, 0.057 => * 5 => .4, .275 => 0.9 and ...
                if (best_three[bi].distance > best_three[0].distance * 0.5)
                {
                    if (loggingOn)
                    {
                        if (pallocation > 0.3)
                            g_info("'%s' score: %.3f (%i) p=%.3f", best_three[bi].patch_name, best_three[bi].distance, bi, pallocation);
                        else
                            g_debug("'%s' score: %.3f (%i) p=%.3f", best_three[bi].patch_name, best_three[bi].distance, bi, pallocation);
                    }
                }
                allocation = allocation - pallocation;

                if (pallocation > 0.0001)
                {
                    for (struct patch* patch = state->patches; patch != NULL; patch = patch->next)
                    {
                        if (strcmp(patch->name, best_three[bi].patch_name) == 0)
                        {
                            //found = TRUE;
                            patch->knn_score += pallocation;
                            break;
                        }
                    }
                }

            }
            return;
        }
    }

    // Try again including unconfirmed recordings (beacon subdirectory)
    k_found = k_nearest(state->recordings, accessdistances, access_points, best_three, 3, FALSE, debug);

    if (k_found == 0 || best_three[0].distance > 1.0)
    {
        // No nearest in the beacons directory either

        // if it's training beacon add it directly to recordings ... otherwise add it to the beacons directory

        if (is_training_beacon)
        {
            g_info("Adding a possible recording '%s' - TRAINING BEACON", device_name);
        }
        else if (k_found > 0)
        {
            g_info("Adding a possible recording '%s' is in %s %.2f", device_name, best_three[0].patch_name, best_three[0].distance);
        }
        else
        {
            g_info("Adding a possible recording '%s' - Nothing near", device_name);
        }

        record("beacons", device_name, accessdistances, access_points, device_name);

        //if (k_found > 2 && best_three[0].distance > 2.0)
        //{
            for (int i = 0; i < k_found; i++)
            {
                g_debug("   KFound %i : %s %.1f", i, best_three[i].patch_name, best_three[i].distance);
            }
    }
}


// ? static time_t last_run;

/*
    Find counts by patch, room and group
*/
bool print_counts_by_closest(struct OverallState* state)
{
    time_t last_run = state->last_summary;
    time(&state->last_summary);

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
        current->beacon_total = 0.0;
        current->watch_total = 0.0;
        current->wearable_total = 0.0;
        current->covid_total = 0.0;
        current->other_total = 0.0;
    }

    // TODO: Make this lazy, less often?
    //if (state->recordings == NULL)
    free_list(&state->recordings);
    int count_recordings = 0;
    int count_recordings_and_beacons = 0;
    g_trace("Re-read observations files");
    bool ok = read_observations ("recordings", state, TRUE);
    if (!ok) g_warning("Failed to read recordings files back");
    for (struct recording* r = state->recordings; r != NULL; r=r->next)
    {
        count_recordings++;
    }

    // and then layer the found beacons on top
    ok = read_observations ("beacons", state, FALSE);
    if (!ok) g_warning("Failed to read beacon files back");
    for (struct recording* r = state->recordings; r != NULL; r=r->next)
    {
        count_recordings_and_beacons++;
    }

    g_info(" ");
    g_info("COUNTS (closest contains %i, recordings: %i, beacons: %i)", state->closest_n, count_recordings, count_recordings_and_beacons);
    
    if (count_recordings == 0)
    {
        for (struct AccessPoint* ap = state->access_points; ap != NULL; ap=ap->next)
        {
            // Create patches: close to ap, between ap1 and ap2, ...?

        }

        //g_warning("No patches found, please deploy patches files");

        char* local_id = state->local->client_id; // Used as group_id

        struct patch* current_patch = get_or_create_patch("Close", "Close", local_id, "group=inside", &state->patches, &state->groups, TRUE);
        struct recording* ralloc = malloc(sizeof(struct recording));
        ralloc->confirmed = TRUE;
        g_utf8_strncpy(ralloc->patch_name, current_patch->name, META_LENGTH);
        ralloc->next = state->recordings;
        state->recordings = ralloc;
        ralloc->access_point_distances[0] = 2.0;    // < 3m

        current_patch = get_or_create_patch("Near", "Near", local_id, "group=inside", &state->patches, &state->groups, TRUE);
        ralloc = malloc(sizeof(struct recording));
        ralloc->confirmed = TRUE;
        g_utf8_strncpy(ralloc->patch_name, current_patch->name, META_LENGTH);
        ralloc->next = state->recordings;
        state->recordings = ralloc;
        ralloc->access_point_distances[0] = 6.0;    // < 9m

        current_patch = get_or_create_patch("Far", "Far", local_id, "group=outside", &state->patches, &state->groups, TRUE);
        ralloc = malloc(sizeof(struct recording));
        ralloc->confirmed = TRUE;
        g_utf8_strncpy(ralloc->patch_name, current_patch->name, META_LENGTH);
        ralloc->next = state->recordings;
        state->recordings = ralloc;
        ralloc->access_point_distances[0] = 12.0;    // > 9m

        current_patch = get_or_create_patch("Distant", "Distant", local_id, "group=outside", &state->patches, &state->groups, TRUE);
        ralloc = malloc(sizeof(struct recording));
        ralloc->confirmed = TRUE;
        g_utf8_strncpy(ralloc->patch_name, current_patch->name, META_LENGTH);
        ralloc->next = state->recordings;
        state->recordings = ralloc;
        ralloc->access_point_distances[0] = 18.0;    // > 14m
    }
    
    time_t now = time(0);

    // Unmark every entry in the closest array
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

        // Set all distances to zero
        for (struct AccessPoint* ap = access_points_list; ap != NULL; ap = ap->next)
        {
            access_distances[ap->id] = EFFECTIVE_INFINITE;  // effective infinite
            access_times[ap->id] = 0.0;
        }

        struct ClosestTo* test = &state->closest[i];

        bool loggingOn = test->time > last_run;

        // moved down to counting ... if (test->category != CATEGORY_PHONE) continue;

        count_examined++;
        if (test->mark) continue;  // already claimed (by second scan on mac address)
        count_not_marked++;

        int age = difftime(now, test->time);
        // If this hasn't been seen in > 300s (5min), skip it
        // and therefore all later instances of it (except beacons, keep them in the list longer)
        // if (age > 400 && test->category != CATEGORY_BEACON) continue;
        // else if (age > 600) continue;

        count_in_age_range++;

        char mac[18];
        mac_64_to_string(mac, sizeof(mac), test->device_64);
        int count = 0;

        char* category = category_from_int(test->category);

        if (loggingOn)
        {
          g_info("--- %s --- %s --- %s", mac, category, test->name);
        }

        int earliest = difftime(now, test->earliest);

        bool superseded = false;
        int count_same_mac = 0;
        bool skipping = false;

        // find all instances on this mac address in the array
        // mark them as seen as we go so they can't seed a new scan
        for (int j = i; j >= 0; j--)
        {
            struct ClosestTo* other = &state->closest[j];

            if (other->device_64 == test->device_64)
            {
                //g_debug("i=%i j=%i", i, j);
                // other is test on first iteration
                other->mark = true;

                // Once we get beyond 300s we start skipping any other matches
                if (skipping) continue;

                count += other->count;

                // Is this a better match than the current one?
                int time_diff = difftime(test->time, other->time);
                int abs_diff = difftime(now, other->time);
                // Should always be +ve as we are scanning back in time

                // TODO: Issue: was superseded on one AP but has been seen since in good health
                superseded = superseded | ((other->supersededby != 0) && (count_same_mac < 2));
                // ignore a superseded value if it was a long time ago and we've seen other reports since then from it
                count_same_mac++;

                //if (time_diff < 300)      // only interested in where it has been recently // handled above in outer loop
                {
                    char other_mac[18];
                    mac_64_to_string(other_mac, 18, other->supersededby);

                    //Verbose logging
                    if (loggingOn) {
                      struct AccessPoint *ap2 = other->access_point;
                      g_debug(" %15s distance %5.1fm at=%3is dt=%3is count=%3i %s%s", ap2->client_id, other->distance, abs_diff, time_diff, other->count,
                        other->supersededby==0 ? "" : "superseeded=", 
                        other->supersededby==0 ? "" : other_mac);
                    }

                    if (time_diff > 600) // or we have seen enough?
                    {
                        if (!skipping)
                        {
                            //g_debug("Skip remainder, %s '%s' delta time %i > 600", mac, test->name, time_diff);
                            skipping = true;
                        }
                        continue;
                    }

                    // So, instead of this, with duplicates in the list ...
                    // build a list of all the points that contribute and their times

                    int index = other->access_point->id;
                    access_distances[index] = other->distance; // was why? round(other->distance * 10.0) / 10.0;
                    access_times[index] = abs_diff;
                }
            }
        }

        struct AccessPoint *ap = test->access_point;

        int delta_time = difftime(now, test->time);
        double x_scale = (test->category == CATEGORY_BEACON || 
            test->category == CATEGORY_LIGHTING ||
            test->category == CATEGORY_APPLIANCE ||
            test->category == CATEGORY_POS ||
            test->category == CATEGORY_PRINTER ||
            test->category == CATEGORY_SPRINKLERS ||
            test->category == CATEGORY_TV || 
            test->category == CATEGORY_FIXED) ? 160.0 :
            test->category == CATEGORY_TABLET ? 120 :
             80.0;       // beacons and other fixed devices last longer, tend to transmit less often

        double score = 0.55 - atan(delta_time / x_scale - 4.0) / 3.0;
        // A curve that stays a 1.0 for a while and then drops rapidly around 3 minutes out
        if (score > 0.90) score = 1.0;
        if (score < 0.1) score = 0.0;

        if (score > 0)
        {
            bool debug = strcmp(test->name, "F350") == 0;

            calculate_location(state, test, access_distances, access_times, test->is_training_beacon, loggingOn, debug);

            // JSON - in a suitable format for copying into a recording
            char *json = NULL;
            cJSON *jobject = cJSON_CreateObject();

            //cJSON_AddStringToObject(jobject, "patch", best.patch_name);

            // char buf[64];
            // strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%S", localtime(&now));
            // cJSON_AddStringToObject(jobject, "time", buf);
            cJSON *jdistances = cJSON_AddObjectToObject(jobject, "distances");
 
            for (struct AccessPoint* current = access_points_list; current != NULL; current = current->next)
            {
                if (access_distances[current->id] != EFFECTIVE_INFINITE)
                {
                    cJSON_AddRounded(jdistances, current->client_id, access_distances[current->id]);
                }
            }

            //cJSON_AddRounded(jobject, "quality", best.distance);

            json = cJSON_PrintUnformatted(jobject);
            cJSON_Delete(jobject);
            // Summary of access distances
            if (loggingOn) {
              g_debug("%s x %.2f", json, score);
            }
            free(json);

            // Several observations, some have it superseded, some don't either because they know it
            // wasn't or because they didn't see the later mac address.
            if (superseded)
            {
                if (loggingOn)
                {
                    g_info("Superseded (earliest=%4is, chosen=%4is latest=%4is) count=%i score=%.2f", -earliest, -delta_time, -age, count, score);
                }
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
            else if (test->category == CATEGORY_WATCH)
            {
                for (struct patch* rcurrent = patch_list; rcurrent != NULL; rcurrent = rcurrent->next)
                {
                    //if (rcurrent->knn_score > 0)
                    //    g_debug("Watch in %s +%.2f x %.2f", rcurrent->name, rcurrent->knn_score, score);
                    rcurrent->watch_total += rcurrent->knn_score * score;        // probability x incidence
                }
            }
            else if (test->category == CATEGORY_WEARABLE)
            {
                for (struct patch* rcurrent = patch_list; rcurrent != NULL; rcurrent = rcurrent->next)
                {
                    //g_debug("Wearable in %s +%.2f x %.2f", rcurrent->name, rcurrent->knn_score, score);
                    rcurrent->wearable_total += rcurrent->knn_score * score;        // probability x incidence
                }
            }
            else if (test->category == CATEGORY_COVID)
            {
                for (struct patch* rcurrent = patch_list; rcurrent != NULL; rcurrent = rcurrent->next)
                {
                    //g_debug("Covid in %s +%.2f x %.2f", rcurrent->name, rcurrent->knn_score, score);
                    rcurrent->covid_total += rcurrent->knn_score * score;        // probability x incidence
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
                    //g_info("Phone in %s +%.2f x %.2f", rcurrent->name, rcurrent->knn_score, score);
                    rcurrent->phone_total += rcurrent->knn_score * score;        // probability x incidence
                }
            }
            else //if (test->distance < 7.5)
            {
                for (struct patch* rcurrent = patch_list; rcurrent != NULL; rcurrent = rcurrent->next)
                {
                    rcurrent->other_total += rcurrent->knn_score * score;
                }
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
                // g_debug("'%s' '%s' '%s'", b->name, b->alias, test->name);
                if (strcmp(b->alias, test->name) == 0 || strcmp(b->name, test->name) == 0 || b->mac64 == test->device_64)
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
            if (loggingOn) 
            {
              g_debug("   score %.2f", score);
            }
        }
        //g_debug(" ");

    }

    char *json_complete = NULL;
    cJSON *jobject = cJSON_CreateObject();

    cJSON *jrooms = cJSON_AddArrayToObject(jobject, "rooms");
    cJSON *jzones = cJSON_AddArrayToObject(jobject, "groups");
    cJSON *jbeacons = cJSON_AddArrayToObject(jobject, "assets");

    // Summarize by room

    struct summary* summary = NULL;
    summarize_by_room(patch_list, &summary);

    for (struct summary* s=summary; s!=NULL; s=s->next)
    {
        cJSON* item = cJSON_CreateObject();
        cJSON_AddStringToObject(item, "name", s->category);
        cJSON_AddStringToObject(item, "group", s->extra);
        cJSON_AddSummary(item, s);
        cJSON_AddItemToArray(jrooms, item);
    }
    free_summary(&summary);

    // Summarize by group

    summary = NULL;
    summarize_by_group(patch_list, &summary);

    for (struct summary* s=summary; s!=NULL; s=s->next)
    {
        cJSON* item = cJSON_CreateObject();
        cJSON_AddStringToObject(item, "name", s->category);
        //cJSON_AddStringToObject(item, "tag", s->extra);
        cJSON_AddSummary(item, s);
        cJSON_AddItemToArray(jzones, item);
    }
    free_summary(&summary);

    if (state->beacons != NULL)
    {
        // First compute a hash, see if any beacon has moved or been updated within the last minute
        uint32_t beacon_hash = 0;
        for (struct Beacon* b = state->beacons; b != NULL; b=b->next)
        {
            // TODO: Proper seconds from time_t calculation
            //struct tm *tm = localtime (&b->last_seen);
            int minutes = b->last_seen == 0 ? 1 : (b->last_seen) / 60;
            beacon_hash = beacon_hash * 37 + (((intptr_t)b->patch) & 0x7fffffff) + minutes;
        }

        // Pack beacon data (todo: make this only on changes?)
        for (struct Beacon* b = state->beacons; b != NULL; b=b->next)
        {
            char ago[20];
            double diff = b->last_seen == 0 ? -1 : difftime(now, b->last_seen) / 60.0;
            if (diff < 2) snprintf(ago, sizeof(ago), "now");
            else if (diff < 60) snprintf(ago, sizeof(ago), "%.0f min ago", diff);
            else if (diff < 24*60) snprintf(ago, sizeof(ago), "%.1f hours ago", diff / 60.0);
            else snprintf(ago, sizeof(ago), "%.1f days ago", diff / 24.0 / 60.0);

            const char* room_name = (b->patch == NULL) ? "---" : b->patch->room;
            const char* category = (b->patch == NULL) ? "---" : ((b->patch->group == NULL) ? "???" : b->patch->group->name);

            cJSON* item = cJSON_CreateObject();
            cJSON_AddStringToObject(item, "name", b->alias);
            cJSON_AddStringToObject(item, "room", room_name);
            cJSON_AddStringToObject(item, "group", category);
            cJSON_AddStringToObject(item, "ago", ago);
            cJSON_AddNumberToObject(item, "t", b->last_seen);
            cJSON_AddRounded(item, "d", diff);

            cJSON_AddItemToArray(jbeacons, item);
        }

        // Log beacon information
        if (state->beacon_hash != beacon_hash)
        {
            state->beacon_hash = beacon_hash;

            g_info(" ");
            g_info("==============================================================================================");

            g_info ("              Beacon               Patch        Room            Group        When");
            g_info("----------------------------------------------------------------------------------------------");
            for (struct Beacon* b = state->beacons; b != NULL; b=b->next)
            {
                char ago[20];
                double diff = b->last_seen == 0 ? -1 : difftime(now, b->last_seen) / 60.0;
                if (diff < 2) snprintf(ago, sizeof(ago), "now");
                else if (diff < 60) snprintf(ago, sizeof(ago), "%.0f min ago", diff);
                else if (diff < 24*60) snprintf(ago, sizeof(ago), "%.1f hours ago", diff / 60.0);
                else snprintf(ago, sizeof(ago), "%.1f days ago", diff / 24.0 / 60.0);

                const char* patch_name =  (b->patch == NULL) ? "---" : b->patch->name;
                const char* room_name = (b->patch == NULL) ? "---" : b->patch->room;
                const char* category = (b->patch == NULL) ? "---" : ((b->patch->group == NULL) ? "???" : b->patch->group->name);

                g_info("%20s %18s %18s %16s      %s",
                    b->alias, patch_name, room_name, 
                    category,
                    ago);
            }

            g_info(" ");
            g_info("==============================================================================================");
            g_info(" ");
            //g_debug("Examined %i > %i > %i > %i", state->closest_n, count_examined, count_not_marked, count_in_age_range);
        }

    }

    // debug
    char* json_groups = cJSON_PrintUnformatted(jzones);

    // Add metadata for the sign to consume (so that signage can be adjusted remotely)
    cJSON* sign_meta = cJSON_AddObjectToObject(jobject, "signage");
    // TODO: More levels etc. settable remotely
    cJSON_AddRounded(sign_meta, "scale_factor", state->udp_scale_factor);

    json_complete = cJSON_PrintUnformatted(jobject);
    //json_rooms = cJSON_Print(jobject);
    cJSON_Delete(jobject);

    if (state->json != NULL)
    {
        // free(json_rooms); but on next cycle
        free(state->json);
    }
    state->json = json_complete;

    //g_info("Summary by room: %s", json_rooms);
    //g_info(" ");
    g_info("Total %.2f people: %s", total_count, json_groups);
    g_info("%s ", json_complete);
    g_info(" ");

    free(json_groups); // string listing each group and count

    // Compute a hash to see if changes have happened (does not have to be perfect, we will send every n minutes regardless)
    // Round to nearest quarter, or 0.1 for phones
    int patch_hash = 0;
    for (struct patch* current = patch_list; current != NULL; current = current->next)
    {
        patch_hash = patch_hash * 7;
        patch_hash += round(current->phone_total * 10) + round(current->tablet_total * 4) 
            + round(current->computer_total * 4) 
            + round(current->covid_total * 4)
            + round(current->beacon_total * 4) + round(current->watch_total * 4) 
            + round(current->wearable_total * 4) 
            + round(current->other_total * 4);
    }

    if (state->patch_hash  != patch_hash)
    {
        state->patch_hash = patch_hash;
        return true;
    }
    return false;
}
