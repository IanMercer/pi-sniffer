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
#include <string.h>

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

            // dispose of side chain
            struct RecentRoom* rr = NULL;
            while ((rr = unlink_head->recent_rooms) != NULL)
            {
                unlink_head->recent_rooms = rr->next;
                g_free(rr);
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
    int8_t category, 
    int known_interval,
    int count, char* name, 
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
        if (c++ > 2*CLOSEST_N) g_error("Stuck scanning head");
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
        head->known_interval = known_interval;
        head->addressType = addressType;
        head->mac64 = device_64;
        head->is_training_beacon = is_training_beacon;
        head->patch = NULL;
        g_utf8_strncpy(head->name, name, NAME_LENGTH);
        head->name_type = name_type;
        head->closest = NULL;
        head->supersededby = 0;
        head->recent_rooms = NULL;

        prune(state, latest);
    }

    // Update the type, latest wins
    if (category != CATEGORY_UNKNOWN) head->category = category;

    // Don't decrease because ESP32 reports 1 instead of 2
    if (addressType > head->addressType) head->addressType = addressType;

    // Upgrade from zero if known
    if (known_interval > head->known_interval) head->known_interval = known_interval;

    // Update the name on the head IF BETTER
    if (name_type > head->name_type)
    {
        g_debug("Updating head name from %s to %s (%i->%i)", head->name, name, head->name_type, name_type);
        g_utf8_strncpy(head->name, name, NAME_LENGTH);
        head->name_type = name_type;
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
                g_trace("  Bump entry for %s on %s", name, access_point->short_client_id);
            }
            else
            {
                g_trace("  Head entry for %s on %s", name, access_point->short_client_id);
            }
            break;
        }
        previous = close;
        if (c++ > N_ACCESS_POINTS) g_error("  Stuck scanning chain %s on %s", name, access_point->short_client_id);
    }

    struct ClosestTo* closest = head->closest;

    // If it's not at the head now, allocate a new one at the head
    if (closest == NULL || closest->access_point != access_point)
    {
        // no such access point observation found, allocate a new one for head
        closest = malloc(sizeof(struct ClosestTo));
        closest->next = head->closest;
        closest->count = 0;
        // Only set earliest on creation, it's per-access point so that's OK
        closest->earliest = earliest;
        head->closest = closest;
        g_trace("  Add entry for %s on %s", name, access_point->short_client_id);
        closest->access_point = access_point;
    }

    // Update it
    closest->count = count == 0 ? closest->count + 1 : count;  // ESP32 doesn't count, we have to do it
    closest->distance = distance;
    closest->latest = latest;

    // closest is now 'top left' - the first in a chain on the first head
}

/*
   Calculates room scores using access point distances
*/
int calculate_location(struct OverallState* state, 
    float accessdistances[N_ACCESS_POINTS],
    float accesstimes[N_ACCESS_POINTS], 
    double average_gap,
    struct top_k* best_three, int best_three_len,
    bool is_training_beacon, bool debug)
{
    (void)accesstimes;

    for (struct patch* patch = state->patches; patch != NULL; patch = patch->next)
    {
        patch->knn_score = 0.0;
    }

    struct AccessPoint* access_points = state->access_points;

    // try confirmed
    int k_found = k_nearest(state->recordings, accessdistances, accesstimes, average_gap, access_points, best_three, best_three_len, TRUE, debug);

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
        double allocation = 1.0;
        for (int bi = 0; bi < k_found; bi++)
        {
            // Adjust this to control how probability is spread over possibiities
            // lower number = less precise, more spread 5 = too much spread
            double scale_factor = 10.0;
            // 0.180 0.179 => 0.001 * 100 = 0.1 
            double pallocation =  allocation * ((bi < k_found-1 ?
                // split 50:50 plus a scale factor favoring the one with the higher score
                fmin(1.0, 0.5 + scale_factor * (best_three[bi].probability_combined - best_three[bi+1].probability_combined)) : 
                1.0));
                // e.g. 0.426, 0.346, 0.289 => 0.080, 0.057 => * 5 => .4, .275 => 0.9 and ...
            best_three[bi].normalized_probability = pallocation;
            allocation = allocation - pallocation;

            if (pallocation > 0.0001)
            {
                best_three[bi].patch->knn_score += pallocation;
                if (best_three[bi].patch->knn_score > 1.0)
                {
                    g_warning("%s knn_score %.2f should be < 1.0", best_three[bi].patch->name, best_three[bi].patch->knn_score);
                }
            }

        }

        return k_found;
    }

    // // Try again including unconfirmed recordings (beacon subdirectory)
    // k_found = k_nearest(state->recordings, accessdistances, accesstimes, average_gap, access_points, best_three, best_three_len, FALSE, debug);

    // if (k_found == 0 || best_three[0].distance > 1.0)
    // {
    //     // No nearest in the beacons directory either

    //     // if it's training beacon add it directly to recordings ... otherwise add it to the beacons directory

    //     if (is_training_beacon)
    //     {
    //         g_info("Adding a possible recording '%s' - TRAINING BEACON", device_name);
    //     }
    //     else if (k_found > 0)
    //     {
    //         g_info("Adding a possible recording '%s' is in %s %.2f", device_name, best_three[0].patch->name, best_three[0].distance);
    //     }
    //     else
    //     {
    //         g_info("Adding a possible recording '%s' - Nothing near", device_name);
    //     }

    //     record("beacons", device_name, accessdistances, access_points);

    //     //if (k_found > 2 && best_three[0].distance > 2.0)
    //     //{
    //         for (int i = 0; i < k_found; i++)
    //         {
    //             g_debug("   KFound %i : %s %.1f", i, best_three[i].patch->name, best_three[i].distance);
    //         }
    // }

    return 0;
}


void debug_print_heading(struct ClosestHead* ahead, time_t now, float average_gap)
{
    struct ClosestTo* latest_observation = ahead->closest;

    char mac[18];
    mac_64_to_string(mac, sizeof(mac), ahead->mac64);

    char* category = category_from_int(ahead->category);

    char prob_s[32];
    prob_s[0] = '\0';
    if (ahead->supersededby != 0)
    {
        char sup_mac[18];
        sup_mac[0]='\0';
        mac_64_to_string(sup_mac, 18, ahead->supersededby);
        snprintf(prob_s, sizeof(prob_s), "p(%.3f) x %s", ahead->superseded_probability, sup_mac);
    }

    time_t earliest = ahead->closest->earliest;
    for (struct ClosestTo* other = ahead->closest; other != NULL; other = other->next)
    {
        if (other->earliest < earliest) earliest = other->earliest;
    }

    const char* room_name = (ahead->recent_rooms == NULL) ? "" : ahead->recent_rooms->name;

    g_debug(" ");
    g_info("%s%s %10s [%5li-%5li] (%4i)     %23s %s  (%.1fs) %s", 
            mac,
            ahead->addressType == PUBLIC_ADDRESS_TYPE ? "*" : " ",
            category, 
            now - earliest,
            now - latest_observation->latest,
            latest_observation->count,
            ahead->name,
            prob_s,
            average_gap,
            room_name
            );
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
        ralloc->patch = current_patch;
        ralloc->next = state->recordings;
        state->recordings = ralloc;
        ralloc->access_point_distances[0] = 2.0;

        //--------- 3.5m

        current_patch = get_or_create_patch("Near", "7m", local_id, "group=inside", &state->patches, &state->groups, TRUE);
        ralloc = malloc(sizeof(struct recording));
        ralloc->confirmed = TRUE;
        ralloc->patch = current_patch;
        ralloc->next = state->recordings;
        state->recordings = ralloc;
        ralloc->access_point_distances[0] = 5.0;

        //--------- 7m

        current_patch = get_or_create_patch("Far", "9m", local_id, "group=outside", &state->patches, &state->groups, TRUE);
        ralloc = malloc(sizeof(struct recording));
        ralloc->confirmed = TRUE;
        ralloc->patch = current_patch;
        ralloc->next = state->recordings;
        state->recordings = ralloc;
        ralloc->access_point_distances[0] = 9.0;

        //--------- 10.5m

        current_patch = get_or_create_patch("Distant", "Far", local_id, "group=outside", &state->patches, &state->groups, TRUE);
        ralloc = malloc(sizeof(struct recording));
        ralloc->confirmed = TRUE;
        ralloc->patch = current_patch;
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

        count_examined++;

        count_not_marked++;

        // Calculate frequency with which this device normally transmits

        int sum_readings = 0;
        int sum_duration = 0;
        // How long does this device typically go between transmits
        double average_gap = 60;

        if (ahead -> known_interval > 0 && ahead->known_interval < 2000)
        {
            average_gap = ahead->known_interval;
        }
        else
        {
            for (struct ClosestTo* other = ahead->closest; other != NULL; other = other->next)
            {
                int dt = difftime(other->latest, other->earliest); 
                int dc = other->count;

                // Correct very small values
                if (dt / dc < 30) {dt = 30 * dc;}
                // Sometimes get a large value for seen ... long gap ... seen again
                if (dt / dc > 300) {dt = 300*dc;}

                sum_duration += dt;
                sum_readings += dc;
            }
            average_gap = sum_duration / sum_readings;

            // Milwaukee beacons need to last longer
            if (ahead->category == CATEGORY_BEACON && average_gap < 45) average_gap = 45.0;
        }

        struct ClosestTo* latest_observation = ahead->closest;

        int delta_time = difftime(now, latest_observation->latest);

        // First n phones get logged
        bool logging = ahead->category == CATEGORY_PHONE || ahead->category == CATEGORY_BEACON;

        // Using beacons to calibrate so need to see logs
        bool detailedLogging = ahead->category == CATEGORY_PHONE;

        if (delta_time > 5 * average_gap) 
        {
            logging = false;
        }

        // Could also suppress logging for anything that hasn't updated since last time
        //     //difftime(test->latest, last_run) > 0

        if (ahead->supersededby != 0)
        {
            if (logging)
            {
                g_debug(" ");
                g_info("'%s' Superseded (age=%4is) @ %s", ahead->name, delta_time,
                    (ahead->patch != NULL ? ahead->patch->name : "unknown"));
            }
            logging = false;
            detailedLogging = false;
        }

        // if (
        //     //difftime(test->latest, last_run) > 0
        //     ahead->category == CATEGORY_PHONE 
        //     || ahead->category == CATEGORY_COMPUTER
        //     || ahead->category == CATEGORY_BEACON
        //     // || 
        //     //ahead->category == CATEGORY_PENCIL
        //     )
        // {
        //     if (log_n-- > 0)
        //     {
        //         logging = true;
        //     }
        // }

        count_in_age_range++;
        int count = 0;
        bool heading_printed = false;

        // Only print headings when printing a move
        // if (logging && !heading_printed)
        // {
        //     heading_printed = true;
        //     debug_print_heading(ahead, now, average_gap);
        // }

        int count_same_mac = 0;

        // BY DEFINITION WE ONLY CARE ABOUT SUPERSEDED ON THE FIRST INSTANCE OF A MAC ADDRESS
        // ALL EARLIER POSSIBLE SUPERSEEDED VALUES ARE IRRELEVANT IF ONE CAME IN LATER ON A
        // DIFFERENT ACCESS POINT Right?

        // Examine all observations of this same mac address
        for (struct ClosestTo* other = ahead->closest; other != NULL; other = other->next)
        {
            count += other->count;

            int time_diff = difftime(latest_observation->latest, other->latest);
            int abs_diff = difftime(now, other->latest);
            // Should always be +ve as we are scanning back in time

            count_same_mac++;

            bool worth_including = 
                // must use at least one no matter how old
                count_same_mac < 2 ||
                // only interested in where it has been recently, but if average_gap is stupidly small bump it to 25s
                (time_diff < 5 * average_gap);

            if (!worth_including) continue;

            //Verbose logging
            if (logging && detailedLogging)
            {
                if (!heading_printed)
                {
                    heading_printed = true;
                    debug_print_heading(ahead, now, average_gap);
                }

                struct AccessPoint *ap2 = other->access_point;
                g_debug("%7.7s %18s @ %5.1fm [%5li-%5li] (%3i)%s", 
                ap2->alternate_name,
                ap2->short_client_id,
                other->distance, 
                now - other->earliest,
                now - other->latest,
                other->count, worth_including ? "" : " (ignore)");
            }

            // add this one to the ones to calculate location on

            int index = other->access_point->id;
            access_distances[index] = other->distance; // was why? round(other->distance * 10.0) / 10.0;
            access_times[index] = abs_diff;
        }

        //struct AccessPoint *ap = test->access_point;

        // Same calculation in knn.c
        double p_gone_away = delta_time < 4 * average_gap ? 0.0 : atan(10*delta_time/average_gap)/3.14159*2;
        double time_score = 1.0 - p_gone_away;

        if (time_score > 0.01)
        {
            bool debug = ahead->category == CATEGORY_PHONE;

            struct top_k best_few[7];
            int k_found = calculate_location(state, 
                access_distances, access_times,
                average_gap,
                best_few, 7,
                ahead->is_training_beacon, debug);


            // MOVING ROOM?

            struct patch* best_patch = best_few[0].patch;
            bool moving = false;

            if (best_patch != NULL && 
                (ahead->recent_rooms == NULL || strcmp(ahead->recent_rooms->name, best_patch->room) != 0))
            {
                struct RecentRoom* recent = malloc(sizeof(struct RecentRoom));
                g_utf8_strncpy(recent->name, best_patch->room, NAME_LENGTH);
                recent->next = ahead->recent_rooms;
                ahead->recent_rooms = recent;

                moving = true;
                // prune

                int keep = 10;

                struct RecentRoom* last_room = ahead->recent_rooms;
                while ((last_room != NULL) && (keep-- > 0))
                {
                    last_room = last_room->next;
                }

                // unlink after here
                if (last_room != NULL)
                {
                    // dispose of tail
                    struct RecentRoom* rr = NULL;
                    while ((rr = last_room->next) != NULL)
                    {
                        g_debug("Pruning %s", rr->name);
                        last_room->next = rr->next;
                        rr->next = NULL;
                        g_free(rr);
                    }
                }
            }

            if (logging || moving) 
            {
                if (!heading_printed)
                {
                    heading_printed = true;
                    debug_print_heading(ahead, now, average_gap);
                }

                if (moving && ahead->recent_rooms->next != NULL)
                {
                    g_debug("Moved %s to %s from %s", ahead->name, best_patch->room, ahead->recent_rooms->next->name);
                }

                // Log the details explaining why it moved
                for (int bi = 0; bi < k_found; bi++)
                {
                    g_debug("%15s +%.3f - %.3f = %.3f, p=%.3f x %.3f -> %.3f",
                        best_few[bi].patch->name, 

                        best_few[bi].probability_is,
                        best_few[bi].probability_isnt,
                        best_few[bi].probability_combined,
                         
                        best_few[bi].normalized_probability, 
                        time_score,
                        best_few[bi].normalized_probability * time_score);
                }

                // JSON - in a suitable format for copying into a recording
                char *json = NULL;
                cJSON *jobject = cJSON_CreateObject();

                cJSON *jdistances = cJSON_AddObjectToObject(jobject, "distances");
    
                for (struct ClosestTo* other = ahead->closest; other != NULL; other = other->next)
                {
                    struct AccessPoint* ap = other->access_point;
                    int access_id = ap->id;
                    // Includes only those that are within sensible time interval (worth_including above)
                    if (access_distances[access_id] < EFFECTIVE_INFINITE)
                    {
                        cJSON_AddRounded(jdistances, ap->short_client_id, access_distances[access_id]);
                    }
                }

                json = cJSON_PrintUnformatted(jobject);
                cJSON_Delete(jobject);
                // Summary of access distances
                g_debug("%s", json);
                free(json);
            }

            // Update statistics

            if (ahead->supersededby == 0)
            {
                for (int bi = 0; bi < k_found; bi++)
                {
                    struct patch* patch = best_few[bi].patch;
                    double probability = best_few[bi].normalized_probability * time_score;
                    if (probability > 0)
                    {
                        switch (ahead->category)
                        {
                            case CATEGORY_TABLET:
                                patch->tablet_total += probability;
                                break;
                            case CATEGORY_PHONE:
                                total_count += probability;
                                patch->phone_total += probability;
                                // g_debug("%s Increase phone total on %s by %.2f to %.2f", mac, patch->name, probability, total_count);
                                break;
                            case CATEGORY_COMPUTER:
                                patch->computer_total += probability;
                                break;
                            case CATEGORY_WATCH:
                                patch->watch_total += probability;
                                break;
                            case CATEGORY_WEARABLE:
                                patch->wearable_total += probability;
                                break;
                            case CATEGORY_COVID:
                                patch->covid_total += probability;
                                break;
                            case CATEGORY_BEACON:
                                patch->beacon_total += probability;
                                break;
                            default:
                                patch->other_total += probability;
                                break;
                            
                        }
                    }
                }

                // If not changed room, still want to update last seen state for beacons (so this can't go further up)

                for (struct Beacon* b = beacon_list; b != NULL; b=b->next)
                {
                    if (strcmp(b->alias, ahead->name) == 0 || strcmp(b->name, ahead->name) == 0 || b->mac64 == ahead->mac64)
                    {
                        if (b->patch != best_patch)
                        {
                            // g_debug("Moving beacon '%s' to %s", b->alias, best_patch->room);
                            b->patch = best_patch;
                        }
                        b->last_seen = latest_observation->latest;
                        break;
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
            s->phone_total > 0 ? 100 * fmin(1.0, s->covid_total / s->phone_total) : 0,
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
            double diff = (b->last_seen == 0) ? -1 : difftime(now, b->last_seen) / 60.0;
            if (diff < 0) snprintf(ago, sizeof(ago), "---");
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

    // Add all access points to json
    cJSON *jaccess = cJSON_AddArrayToObject(jobject, "access");

    for (struct AccessPoint* ap = state->access_points; ap != NULL; ap=ap->next)
    {
        cJSON* item = cJSON_CreateObject();
        cJSON_AddStringToObject(item, "id", ap->client_id);
        cJSON_AddStringToObject(item, "sid", ap->short_client_id);
        cJSON_AddNumberToObject(item, "t", ap->last_seen);
        if (ap->ap_class != ap_class_unknown)
        {
            cJSON_AddNumberToObject(item, CJ_AP_CLASS, ap->ap_class);
        }

        for (struct Sensor* sensor = ap->sensors; sensor != NULL; sensor = sensor->next)
        {
            if (isnan(sensor->value_float))
            {
                cJSON_AddNumberToObject(item, sensor->id, sensor->value_int);
            }
            else
            {
                cJSON_AddRounded(item, sensor->id, sensor->value_float);
            }
        }

        cJSON_AddItemToArray(jaccess, item);
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
