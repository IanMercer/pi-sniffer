/*
    Code for calculating on the closest list which is accumulated from all the sensors in the system
*/

#include "closest.h"

#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <glib.h>

#include <sys/types.h>
#include <stdio.h>
#include <inttypes.h>

#include <time.h>
#include <math.h>
#include "cJSON.h"
#include "knn.h"
#include "overlaps.h"
#include "aggregate.h"

/*
    Get the closest recent observation for a device
    TODO: Could just copy patch name from head now?
    Except this is independent from the closest algorithm which only runs on a gateway
*/
struct ClosestTo *get_closest_64(struct OverallState* state, int64_t device_64)
{
    struct ClosestTo *best = NULL;

    // Working backwards in time through the array
    for (struct ClosestHead* head = state->closestHead; head != NULL; head = head->next)
    {
        if (head->mac64 != device_64) continue;

        for (struct ClosestTo* test = head->closest; test != NULL; test = test->next)
        {
            g_assert(test->access_point != NULL);

            if (best == NULL)
            {
                best = test;
            }
            else if (best->access_point->id == test->access_point->id)
            {
                // continue, latest hit on access point is most relevant
                // but if the distance is much better and it's fairly recent,
                // go with that distance instead
                double delta_time = difftime(best->latest, test->latest);
                if (best->distance > test->distance && delta_time < 30){
                    best->distance = test->distance;
                }
            }
            else if (best->distance > test->distance)
            {
                // TODO: Check time too, only recent ones
                double delta_time = difftime(best->latest, test->latest);

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
*  Prune based on time, removing old data, not strictly necessary but keeps memory requirement lower
*  and saves unnecessary CPU cycles
*/
void prune(struct OverallState* state, time_t latest)
{
    struct ClosestHead* cut_off_after = NULL;
    struct ClosestHead* pc = NULL;
    int debug_cutoff_index = 0;
    int c = 0;
    for (struct ClosestHead* t = state->closestHead; t != NULL && t->next != NULL; t = t->next)
    {
        if (t->closest != NULL)  // First item isn't initialized yet
        {
            c++;
            int age = difftime(latest, t->closest->latest); // latest is a substitute for now, close enough
            if (age > MAX_AGE)
            {
                cut_off_after = pc;  // not t, the one before
                //g_debug("Prune head array (too old) starting at %i. %s", debug_cutoff_index, t->name);
                break;
            }
            if (c > CLOSEST_N)
            {
                cut_off_after = pc;  // not t, the one before
                //g_debug("Prune head array (too many) starting at %i. %s", debug_cutoff_index, t->name);
                break;
            }
          
            debug_cutoff_index++;
            pc = t;
        }
    }

    if (cut_off_after != NULL)
    {
        // dispose of head chain
        struct ClosestHead* unlink_head = NULL;
        while ((unlink_head = cut_off_after->next) != NULL)
        {
            // dispose of side chain
            struct ClosestTo* unlink = NULL;
            while ((unlink = unlink_head->closest) != NULL)
            {
                unlink_head->closest = unlink->next;
                g_free(unlink);
            }

            cut_off_after->next = unlink_head->next;
            g_free(unlink_head);
        }
    }
}

/*
   Add a closest observation (get a lock before you call this)
*/
void add_closest(struct OverallState* state, int64_t device_64, struct AccessPoint* access_point, 
    time_t earliest, time_t latest, float distance, 
    int8_t category, int count, char* name, 
    enum name_type name_type, int8_t addressType,
    bool is_training_beacon)
{
    g_assert(access_point != NULL);

    // Find the matching mac address in the ClosestTo head list

    struct ClosestHead* previousHead = NULL;
    int c = 0;
    for(struct ClosestHead* h = state->closestHead; h != NULL; h=h->next)
    {
        if (h->mac64 == device_64) 
        {
            // If it was superseeded, it isn't now
            if (h->supersededby != 0)
            {
                char mac[18];
                mac_64_to_string(mac, sizeof(mac), h->mac64);

                char othermac[18];
                mac_64_to_string(othermac, sizeof(mac), h->supersededby);

                g_warning("%s %s was marked superseded by %s but it's no longer", mac, h->name, othermac);
                h->supersededby = 0;
            }

            // Move it to the head of the chain so that they sort in time order always
            if (previousHead != NULL)
            {
                // unlink from chain
                previousHead->next = h->next;
                // and put it on the front
                h->next = state->closestHead;
                state->closestHead = h;
            }
            // else, already at head of chain
            break; 
        }
        previousHead = h;
        if (c++ > 2*CLOSEST_N) g_error("Stuck scanning head for %s", access_point->client_id);
    }

    // We moved it to the head so it should be here
    struct ClosestHead* head = state->closestHead;

    if (head == NULL || head->mac64 != device_64)
    {
        g_trace("Add new head for %s", name);

        head = malloc(sizeof(struct ClosestHead));
        // Add it to the front of the list
        head->next = state->closestHead;
        state->closestHead = head;
        // Initialize it
        head->category = category;
        head->addressType = addressType;
        head->mac64 = device_64;
        head->is_training_beacon = is_training_beacon;
        head->patch = NULL;
        g_utf8_strncpy(head->name, name, NAME_LENGTH);
        head->name_type = name_type;
        head->closest = NULL;
        head->supersededby = 0;

        prune(state, latest);
    }

    // Update the type, latest wins
    if (category != CATEGORY_UNKNOWN) head->category = category;
    head->addressType = addressType;

    // Update the name on the head IF BETTER
    if (name_type > head->name_type)
    {
        strncpy(head->name, name, NAME_LENGTH);
        head->name_type = name_type;
    }

    // Use apply_known_beacons method instead?
    for (struct Beacon* b = state->beacons; b != NULL; b = b->next)
    {
        if (strcmp(b->name, name) == 0 || b->mac64 == device_64)
        {
            g_utf8_strncpy(head->name, b->alias, NAME_LENGTH);
            head->name_type = nt_alias;
        }
    }


    // Now scan along

    // Remove same access point if present
    struct ClosestTo* previous = NULL;
    c = 0;
    for (struct ClosestTo* close = head->closest; close != NULL; close=close->next)
    {
        if (close->access_point == access_point)
        {
            // unlink from chain
            if (previous != NULL)
            {
                previous->next = close->next;
                // and put it on the front
                close->next = head->closest;
                head->closest = close;
                g_trace("  Bump entry for %s on %s", name, access_point->client_id);
            }
            else
            {
                g_trace("  Head entry for %s on %s", name, access_point->client_id);
            }
            break;
        }
        previous = close;
        if (c++ > N_ACCESS_POINTS) g_error("  Stuck scanning chain %s on %s", name, access_point->client_id);
    }

    struct ClosestTo* closest = head->closest;

    // If it's not at the head now, allocate a new one at the head
    if (closest == NULL || closest->access_point != access_point)
    {
        // no such access point observation found, allocate a new one for head
        closest = malloc(sizeof(struct ClosestTo));
        closest->next = head->closest;
        head->closest = closest;
        g_trace("  Add entry for %s on %s", name, access_point->client_id);
        closest->access_point = access_point;
    }

    // Update it
    closest->count = count;
    closest->distance = distance;
    closest->earliest = earliest;
    closest->latest = latest;

    // closest is now 'top left' - the first in a chain on the first head

    if (closest->count == count && closest->distance == distance)
    {
        // unchanged
        //g_warning("Update is identical, skipping %s %s", access_point->client_id, mac);
        return;
    }
}


/*
   Calculates room scores using access point distances
*/
int calculate_location(struct OverallState* state, 
    struct ClosestHead* head,
    float accessdistances[N_ACCESS_POINTS],
    float accesstimes[N_ACCESS_POINTS], 
    struct top_k* best_three, int best_three_len,
    bool is_training_beacon, bool debug)
{
    (void)accesstimes;

    for (struct patch* patch = state->patches; patch != NULL; patch = patch->next)
    {
        patch->knn_score = 0.0;
    }

    struct AccessPoint* access_points = state->access_points;

    char* device_name = head->name;
    //const char* category = category_from_int(closest->category);

    // try confirmed
    int k_found = k_nearest(state->recordings, accessdistances, accesstimes, access_points, best_three, best_three_len, TRUE, debug);

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
        // Allocate probabilities to patches instead of 1.0 and 0.0 only
        double allocation = 1.0;
        for (int bi = 0; bi < k_found; bi++)
        {
            // Adjust this to control how probability is spread over possibiities
            // lower number = less precise, more spread 5 = too much spread
            double scale_factor = 20.0;
            // 0.180 0.179 => 0.001 * 100 = 0.1 
            double pallocation =  allocation * (bi < k_found-1 ?
                fmin(1.0, 0.5 + scale_factor * (best_three[bi].distance - best_three[bi+1].distance)) : 
                1.0);
                // e.g. 0.426, 0.346, 0.289 => 0.080, 0.057 => * 5 => .4, .275 => 0.9 and ...
            best_three[bi].probability = pallocation;
            allocation = allocation - pallocation;

            if (pallocation > 0.0001)
            {
                for (struct patch* patch = state->patches; patch != NULL; patch = patch->next)
                {
                    if (strcmp(patch->name, best_three[bi].patch_name) == 0)
                    {
                        //found = TRUE;
                        patch->knn_score += pallocation;
                        if (patch -> knn_score > 1.0)
                        {
                            g_warning("knn_score %.2f should be < 1.0", patch->knn_score);
                        }
                        break;
                    }
                }
            }

        }

        return k_found;
    }

    // Try again including unconfirmed recordings (beacon subdirectory)
    k_found = k_nearest(state->recordings, accessdistances, accesstimes, access_points, best_three, best_three_len, FALSE, debug);

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

    return 0;
}


// ? static time_t last_run;

/*
    Find counts by patch, room and group
*/
bool print_counts_by_closest(struct OverallState* state)
{
    time_t last_run = state->last_summary;
    time(&state->last_summary);

    //g_debug("pack_closest_columns()");
    pack_closest_columns(state);

    struct AccessPoint* access_points_list = state->access_points;
    struct Beacon* beacon_list = state->beacons;
    struct patch* patch_list = state->patches;

    float total_count = 0.0;

    //g_debug("Clear room totals");

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
    bool ok = read_observations ("/var/sniffer/recordings", state, TRUE);
    if (!ok) g_warning("Failed to read recordings files back");
    for (struct recording* r = state->recordings; r != NULL; r=r->next)
    {
        count_recordings++;
    }

    // and then layer the found beacons on top
    ok = read_observations ("/var/sniffer/beacons", state, FALSE);
    if (!ok) g_warning("Failed to read beacon files back");
    for (struct recording* r = state->recordings; r != NULL; r=r->next)
    {
        count_recordings_and_beacons++;
    }

    g_info(" ");
    g_info("COUNTS (recordings: %i, beacons: %i)", count_recordings, count_recordings_and_beacons);
    
    if (count_recordings == 0)
    {
        for (struct AccessPoint* ap = state->access_points; ap != NULL; ap=ap->next)
        {
            // Create patches: close to ap, between ap1 and ap2, ...?
        }

        //g_warning("No patches found, please deploy patches files");

        char* local_id = state->local->client_id; // Used as group_id

        struct patch* current_patch = get_or_create_patch("Close", "4m", local_id, "group=inside", &state->patches, &state->groups, TRUE);
        struct recording* ralloc = malloc(sizeof(struct recording));
        ralloc->confirmed = TRUE;
        g_utf8_strncpy(ralloc->patch_name, current_patch->name, META_LENGTH);
        ralloc->next = state->recordings;
        state->recordings = ralloc;
        ralloc->access_point_distances[0] = 2.0;

        //--------- 3.5m

        current_patch = get_or_create_patch("Near", "7m", local_id, "group=inside", &state->patches, &state->groups, TRUE);
        ralloc = malloc(sizeof(struct recording));
        ralloc->confirmed = TRUE;
        g_utf8_strncpy(ralloc->patch_name, current_patch->name, META_LENGTH);
        ralloc->next = state->recordings;
        state->recordings = ralloc;
        ralloc->access_point_distances[0] = 5.0;

        //--------- 7m

        current_patch = get_or_create_patch("Far", "9m", local_id, "group=outside", &state->patches, &state->groups, TRUE);
        ralloc = malloc(sizeof(struct recording));
        ralloc->confirmed = TRUE;
        g_utf8_strncpy(ralloc->patch_name, current_patch->name, META_LENGTH);
        ralloc->next = state->recordings;
        state->recordings = ralloc;
        ralloc->access_point_distances[0] = 9.0;

        //--------- 10.5m

        current_patch = get_or_create_patch("Distant", "Far", local_id, "group=outside", &state->patches, &state->groups, TRUE);
        ralloc = malloc(sizeof(struct recording));
        ralloc->confirmed = TRUE;
        g_utf8_strncpy(ralloc->patch_name, current_patch->name, META_LENGTH);
        ralloc->next = state->recordings;
        state->recordings = ralloc;
        ralloc->access_point_distances[0] = 12.0;
    }
    
    time_t now = time(0);

    // // Unmark every entry in the closest array
    // for (int i = state->closest_n - 1; i >= 0; i--)
    // {
    //     g_assert(state->closest[i].access_point != NULL);
    //     state->closest[i].mark = false;
    // }

    int count_examined = 0;
    int count_not_marked = 0;
    int count_in_age_range = 0;
    // Log the first N items, enough to cover a small site, limit output for a large site
    // TODO: Make logging configurable, turn off over time?
    int log_n = 15;

    for (struct ClosestHead* ahead = state->closestHead; ahead != NULL; ahead = ahead->next)
    {
        // parallel array of distances and times
        float access_distances[N_ACCESS_POINTS];       // latest observation distance
        float access_times[N_ACCESS_POINTS];           // latest observation time

        // Set all distances to zero
        for (struct AccessPoint* ap = access_points_list; ap != NULL; ap = ap->next)
        {
            access_distances[ap->id] = EFFECTIVE_INFINITE;  // effective infinite
            access_times[ap->id] = 0.0;
        }

        struct ClosestTo* test = ahead->closest;
        count_examined++;

        count_not_marked++;

        // Calculate frequency with which this device normally transmits

        int sum_readings = 0;
        int sum_duration = 0;

        for (struct ClosestTo* other = ahead->closest; other != NULL; other = other->next)
        {
            sum_duration += difftime(other->latest, other->earliest);
            sum_readings += other->count;
        }

        // How long does this device typically go between transmits
        double average_gap = sum_duration / sum_readings;

        int delta_time = difftime(now, test->latest);
        // beacons and other fixed devices last longer, tend to transmit less often
        double x_scale = (
                // Very fixed devices don't decay, some are very infrequent transmitters
                ahead->category == CATEGORY_LIGHTING ||
                ahead->category == CATEGORY_APPLIANCE ||
                ahead->category == CATEGORY_POS ||
                ahead->category == CATEGORY_SPRINKLERS ||
                ahead->category == CATEGORY_TOOTHBRUSH ||
                ahead->category == CATEGORY_BEACON || 
                ahead->category == CATEGORY_FIXED
            ) ? 240.0 :
            (
                ahead->category == CATEGORY_HEALTH ||
                ahead->category == CATEGORY_FITNESS ||
                ahead->category == CATEGORY_PRINTER ||
                ahead->category == CATEGORY_TV
            ) ? 160 :
            (
                ahead->category == CATEGORY_COMPUTER ||
                ahead->category == CATEGORY_TABLET 
            )? 120 :
             90.0;

        double time_score = 0.55 - atan(delta_time / x_scale - 4.0) / 3.0;
        // A curve that stays a 1.0 for a while and then drops rapidly around 3 minutes out
        if (time_score > 0.90) time_score = 1.0;

        if (time_score < 0.1) continue;

        // First n phones get logged
        bool logging = false;

        if (
            difftime(test->latest, last_run) > 0
            //|| ahead->category == CATEGORY_PHONE 
            //|| ahead->category == CATEGORY_BEACON
            //ahead->category == CATEGORY_COVID || 
            //ahead->category == CATEGORY_PENCIL
            )
        {
            if (log_n-- > 0)
            {
                logging = true;
            }
        }
   
        bool detailedLogging = false;

        int age = difftime(now, test->latest);
        // If this hasn't been seen in > 300s (5min), skip it
        // and therefore all later instances of it (except beacons, keep them in the list longer)
        // if (age > 400 && test->category != CATEGORY_BEACON) continue;
        // else if (age > 600) continue;

        count_in_age_range++;

        char mac[18];
        mac_64_to_string(mac, sizeof(mac), ahead->mac64);
        int count = 0;

        char* category = category_from_int(ahead->category);

        if (logging)
        {
            char prob_s[32];
            prob_s[0] = '\0';
            if (ahead->supersededby != 0)
            {
                char sup_mac[18];
                sup_mac[0]='\0';
                mac_64_to_string(sup_mac, 18, ahead->supersededby);
                snprintf(prob_s, sizeof(prob_s), "p(%.3f) x %s", ahead->superseded_probability, sup_mac);
            }

            time_t earliest = test->earliest;
            for (struct ClosestTo* other = test; other != NULL; other = other->next)
            {
                if (other->earliest < earliest) earliest = other->earliest;
            }

            g_debug("%s%s %10s [%5li-%5li] (%4i)     %23s %s  (%.1fs)", 
                    mac,
                    ahead->addressType == PUBLIC_ADDRESS_TYPE ? "*" : " ",
                    category, 
                    now - earliest,
                    now - test->latest,
                    test->count,
                    ahead->name,
                    prob_s,
                    average_gap
                    );
        }

        int earliest = difftime(now, test->earliest);

        int count_same_mac = 0;
        bool skipping = false;

        // BY DEFINITION WE ONLY CARE ABOUT SUPERSEDED ON THE FIRST INSTANCE OF A MAC ADDRESS
        // ALL EARLIER POSSIBLE SUPERSEEDED VALUES ARE IRRELEVANT IF ONE CAME IN LATER ON A
        // DIFFERENT ACCESS POINT Right?

        // Examine all instances of this mac address
        for (struct ClosestTo* other = test; other != NULL; other = other->next)
        {
            // Once we get beyond 300s we start skipping any other matches
            if (skipping) continue;

            count += other->count;

            // Is this a better match than the current one?
            int time_diff = difftime(test->latest, other->latest);
            int abs_diff = difftime(now, other->latest);
            // Should always be +ve as we are scanning back in time

            count_same_mac++;

            //if (time_diff < 300)      // only interested in where it has been recently // handled above in outer loop
            {

                //Verbose logging
                if (logging && ahead->supersededby == 0)
                {
                    struct AccessPoint *ap2 = other->access_point;
                    g_debug(" %15s distance %5.1fm [%5li-%5li] (%3i)", 
                    ap2->client_id, other->distance, 
                    now - other->earliest,
                    now - other->latest,
                    other->count);
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

        //struct AccessPoint *ap = test->access_point;

        if (time_score > 0)
        {
            bool debug = false; //strcmp(test->name, "F350") == 0;

            struct top_k best_three[3];
            int k_found = calculate_location(state, ahead, access_distances, access_times,
                best_three, 3,
                ahead->is_training_beacon, debug);

            for (int bi = 0; bi < k_found; bi++)
            {
                if (best_three[bi].distance > best_three[0].distance * 0.5)
                {
                    if (logging || (detailedLogging && bi == 0))
                    {
                        g_debug("%15s sc: %.3f p=%.3f x %.3f -> %.3f",
                            best_three[bi].patch_name, best_three[bi].distance, 
                            best_three[bi].probability, time_score,
                            best_three[bi].probability * time_score);
                        // TODO: How to get persistent patch addresses closest->patch = best_three[bi].patch;
                    }
                }
            }

            // JSON - in a suitable format for copying into a recording
            char *json = NULL;
            cJSON *jobject = cJSON_CreateObject();

            cJSON *jdistances = cJSON_AddObjectToObject(jobject, "distances");
 
            for (struct AccessPoint* current = access_points_list; current != NULL; current = current->next)
            {
                if (access_distances[current->id] < EFFECTIVE_INFINITE)
                {
                    cJSON_AddRounded(jdistances, current->client_id, access_distances[current->id]);
                }
            }

            //cJSON_AddRounded(jobject, "quality", best.distance);

            json = cJSON_PrintUnformatted(jobject);
            cJSON_Delete(jobject);
            // Summary of access distances
            if (logging) 
            {
              g_debug("%s", json);
            }
            free(json);

            if (ahead->supersededby != 0)
            {
                if (logging)
                {
                    g_info("Superseded (earliest=%4is, chosen=%4is latest=%4is) count=%i score=%.2f", -earliest, -delta_time, -age, count, time_score);
                }
            }
            else if (ahead->category == CATEGORY_TABLET)
            {
                for (struct patch* rcurrent = patch_list; rcurrent != NULL; rcurrent = rcurrent->next)
                {
                    rcurrent->tablet_total += rcurrent->knn_score * time_score;        // probability x incidence
                    // if (logging && rcurrent->knn_score > 0)
                    // {
                    //     g_info("Tablet in %s +%.2f x %.2f", rcurrent->name, rcurrent->knn_score, score);
                    // }
                }
            }
            else if (ahead->category == CATEGORY_COMPUTER)
            {
                for (struct patch* rcurrent = patch_list; rcurrent != NULL; rcurrent = rcurrent->next)
                {
                    rcurrent->computer_total += rcurrent->knn_score * time_score;        // probability x incidence
                }
            }
            else if (ahead->category == CATEGORY_WATCH)
            {
                for (struct patch* rcurrent = patch_list; rcurrent != NULL; rcurrent = rcurrent->next)
                {
                    //if (rcurrent->knn_score > 0)
                    //    g_debug("Watch in %s +%.2f x %.2f", rcurrent->name, rcurrent->knn_score, score);
                    rcurrent->watch_total += rcurrent->knn_score * time_score;        // probability x incidence
                }
            }
            else if (ahead->category == CATEGORY_WEARABLE || ahead->category == CATEGORY_FITNESS)
            {
                for (struct patch* rcurrent = patch_list; rcurrent != NULL; rcurrent = rcurrent->next)
                {
                    //g_debug("Wearable in %s +%.2f x %.2f", rcurrent->name, rcurrent->knn_score, score);
                    rcurrent->wearable_total += rcurrent->knn_score * time_score;        // probability x incidence
                }
            }
            else if (ahead->category == CATEGORY_COVID)
            {
                for (struct patch* rcurrent = patch_list; rcurrent != NULL; rcurrent = rcurrent->next)
                {
                    //g_debug("Covid in %s +%.2f x %.2f", rcurrent->name, rcurrent->knn_score, score);
                    rcurrent->covid_total += rcurrent->knn_score * time_score;        // probability x incidence
                }
            }
            else if (ahead->category == CATEGORY_BEACON)
            {
                for (struct patch* rcurrent = patch_list; rcurrent != NULL; rcurrent = rcurrent->next)
                {
                    rcurrent->beacon_total += rcurrent->knn_score * time_score;        // probability x incidence
                }
            }
            else if (ahead->category == CATEGORY_PHONE)
            {
                //g_info("Cluster (earliest=%4is, chosen=%4is latest=%4is) count=%i score=%.2f", -earliest, -delta_time, -age, count, score);
                total_count += time_score;
                for (struct patch* rcurrent = patch_list; rcurrent != NULL; rcurrent = rcurrent->next)
                {
                    rcurrent->phone_total += rcurrent->knn_score * time_score;        // probability x incidence
                }
            }
            else //if (test->distance < 7.5)
            {
                for (struct patch* rcurrent = patch_list; rcurrent != NULL; rcurrent = rcurrent->next)
                {
                    rcurrent->other_total += rcurrent->knn_score * time_score;
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
                if (strcmp(b->alias, ahead->name) == 0 || strcmp(b->name, ahead->name) == 0 || b->mac64 == ahead->mac64)
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
                        beacon->last_seen = test->latest;
                    }
                }
            }

        }
        else
        {
            if (logging) 
            {
              g_debug("   score %.2f", time_score);
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
        // This makes reception hard: if (any_present(s))
        {
            cJSON* item = cJSON_CreateObject();
            cJSON_AddStringToObject(item, "name", s->category);
            cJSON_AddStringToObject(item, "group", s->extra);
            cJSON_AddSummary(item, s);
            cJSON_AddItemToArray(jrooms, item);
        }
    }
    free_summary(&summary);

    // Summarize by group
    summary = NULL;
    summarize_by_group(patch_list, &summary);

    g_info("              phones     covid percent   watches   tablets wearables computers   beacons     other");
    for (struct summary* s=summary; s!=NULL; s=s->next)
    {
        cJSON* item = cJSON_CreateObject();
        cJSON_AddStringToObject(item, "name", s->category);
        //cJSON_AddStringToObject(item, "tag", s->extra);
        cJSON_AddSummary(item, s);
        cJSON_AddItemToArray(jzones, item);
        g_info("%10s %9.1f %9.1f    %3.0f%% %9.1f %9.1f %9.1f %9.1f %9.1f %9.1f", s->category, 
            s->phone_total, 
            s->covid_total, 
            s->phone_total > 0 ? 100 * s->covid_total / s->phone_total : 0,
            s->watch_total,
            s->tablet_total, 
            s->wearable_total,
            s->computer_total,
            s->beacon_total,
            s->other_total
            );
    }
    free_summary(&summary);

    if (state->beacons != NULL)
    {
        // First compute a hash, see if any beacon has moved or been updated within the last n minutes
        uint32_t beacon_hash = 0;
        for (struct Beacon* b = state->beacons; b != NULL; b=b->next)
        {
            // TODO: Proper seconds from time_t calculation
            //struct tm *tm = localtime (&b->last_seen);
            int minutes = 0; // ONLY SEND WHEN ROOM CHANGES ... b->last_seen == 0 ? 1 : (b->last_seen) / 60;
            beacon_hash = beacon_hash * 37 + (((intptr_t)b->patch) & 0x7fffffff) + minutes;
        }

        // Pack beacon data (todo: make this only on changes?)
        for (struct Beacon* b = state->beacons; b != NULL; b=b->next)
        {
            char ago[20];
            double diff = b->last_seen == 0 ? -1 : difftime(now, b->last_seen) / 60.0;
            if (b->last_seen == 0) snprintf(ago, sizeof(ago), "---");
            else if (diff < 2) snprintf(ago, sizeof(ago), "now");
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

            g_info ("              Beacon               Patch              Room            Group     When");
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
    else { g_debug("No assets to track");}

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
    //g_info("Total %.2f people: %s", total_count, json_groups);
    //g_info("%s ", json_complete);
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
