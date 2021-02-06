#include "overlaps.h"
#include "closest.h"
#include "aggregate.h"
#include "device.h"
#include <math.h>

/*
    Do these two devices overlap in time? If so they cannot be the same device
    (allowed to touch given granularity of time)
*/
bool overlapsClosest(time_t a_earliest, time_t a_latest, time_t b_earliest, time_t b_latest)
{
    if (a_earliest >= b_latest)  // a is entirely after b
    {
        return FALSE;
        int delta_time = difftime(a_earliest, b_latest);
        return delta_time > 60;  // more than 60s and these are probably unrelated devices
    }
    if (b_earliest >= a_latest) // b is entirely after a
    {
        return FALSE;
        int delta_time = difftime(b_earliest, a_latest);
        return delta_time > 60;  // more than 60s and these are probably unrelated devices
    }
    return TRUE;      // must overlap if not entirely after or before
}

// When we know A is after B, only need to compare one way
bool overlapsOneWay(time_t a_earliest, time_t b_latest)
{
    if (a_earliest >= b_latest)  // a is entirely after b
    {
        return FALSE;
        int delta_time = difftime(a_earliest, b_latest);
        return delta_time > 60;  // more than 60s and these are probably unrelated devices
    }
    return TRUE;      // must overlap if not entirely after or before
}


/*
    Was this just a blip?
*/
bool justABlip(time_t a_earliest, time_t a_latest, int a_count, 
               time_t b_earliest, time_t b_latest, int b_count)
{
    // If the earlier device has just one observation it's too soon to say if the latter one is the same device
    // In a transit situation we get many devices passing by with just one ping, these are not superceded
    double delta = fabs(difftime(a_earliest, b_latest));
    // under 2s - unlikely to get two transmissions on different macs from same device
    // over 90s - unlikely to be the same device (first left, second arrived)
    if (a_count == 1 && a_latest <= b_earliest && (delta < 2 || delta > 90))
    {
        return TRUE; // unlikely to be same device
    }
    if (b_count == 1 && b_latest <= a_earliest && (delta < 2 || delta > 90))
    {
        return TRUE; // unlikely to be same device
    }
    return FALSE;
}


/*
    Compute the minimum number of devices present by assigning each in a non-overlapping manner to columns
*/
void pack_closest_columns(struct OverallState* state)
{
    // Working backwards in time through the array
    // Push every device back to column zero as category may have changed
    for (int i = state->closest_n - 1; i > 0; i--)
    {
        struct ClosestTo *a = &state->closest[i];
        a->column = 0;
    }

    // Should take many fewer iterations than this! This is worst case max.
    for (int iterations = 0; iterations < state->closest_n; iterations++)
    {
        bool changed = false;

        for (int i = state->closest_n - 1; i > 0; i--)
        {
            struct ClosestTo *a = &state->closest[i];

            // Find all earlier instances and get the earliest possible time stamp
            time_t a_earliest = a->earliest;
            time_t a_latest = a->latest;

            for (int ii = i-1; ii >=0; ii--)
            {
                struct ClosestTo *b = &state->closest[ii];
                if (a->device_64 == b->device_64 &&
                    b->earliest < a_earliest) a_earliest = b->earliest;
            }

            for (int j = i - 1; j >= 0; j--)
            {
                struct ClosestTo *b = &state->closest[j];

                if (a->device_64 == b->device_64) continue;

                // How to handle two observations from the different access points
                //if (a->access_point->id != b->access_point->id) continue;

                if (a->column != b->column)
                    continue;

                // TODO: How to skip all the repeats of the same mac address as B
                // otherwise wasted effort

                // No need to scan down the B's
                //  B must be earlier than A because of the order we are scanning in
                //  So only B's latest time matters in relation to A's earliest time

                bool blip = justABlip(a_earliest, a_latest, a->count, b->earliest, b->latest, b->count);
                bool over = overlapsOneWay(a_earliest, b->latest);

                // cannot be the same device if one is public and the other is random address type
                // (or we don't have an address type yet)
                bool haveDifferentAddressTypes = (a->addressType > 0 && b->addressType > 0 && 
                    a->addressType != b->addressType);

                // cannot be the same if they both have names and the names are different
                // but don't reject _ names as they are temporary and will get replaced
                bool haveDifferentNames =
                    // can reject as soon as they both have a partial name that is same type but doesn't match
                    // but cannot reject while one or other has a temporary name as it may match
                    (a->name_type == b->name_type) && (g_strcmp0(a->name, b->name) != 0);

                // cannot be the same if they both have known categories and they are different
                bool haveDifferentCategories = (a->category != b->category);

                // cannot be the same device if they have different mac addresses and one is a public mac
                bool haveDifferentMacAndPublic = (a->device_64 != b->device_64) &&
                    (a->addressType == PUBLIC_ADDRESS_TYPE || b->addressType == PUBLIC_ADDRESS_TYPE);

                if (blip || over || haveDifferentAddressTypes || haveDifferentNames || haveDifferentCategories || 
                    haveDifferentMacAndPublic)
                {
                    b->column++;
                    changed = true;
                    // Log to see why entries with the same name are failing
                    // if (g_strcmp0(a->name, "Apple Pencil") == 0 && g_strcmp0(b->name, "Apple Pencil") == 0)
                    // {
                    //     g_debug("%i.%s/%s %i.%s/%s Bump to (%i, %i),      %s%s%s%s%s%s", 
                    //         i, a->name, a->access_point->client_id, 
                    //         j, b->name, b->access_point->client_id,
                    //         a->column, b->column,
                    //         blip ? "blip " : "", 
                    //         over ? "over " : "",
                    //         haveDifferentAddressTypes ? "addressTypes " : "",
                    //         haveDifferentNames ? "names ": "", 
                    //         haveDifferentCategories ? "categories ":"", 
                    //         haveDifferentMacAndPublic ? "mac ": "");
                    // }
                    // if (g_strcmp0(a->name, "Covid Trace") == 0 && g_strcmp0(b->name, "Covid Trace") == 0)
                    // {
                    //     g_debug("%i.%s/%s %i.%s/%s Bump to (%i, %i),      %s%s%s%s%s%s", 
                    //         i, a->name, a->access_point->client_id, 
                    //         j, b->name, b->access_point->client_id,
                    //         a->column, b->column,
                    //         blip ? "blip " : "", 
                    //         over ? "over " : "",
                    //         haveDifferentAddressTypes ? "addressTypes " : "",
                    //         haveDifferentNames ? "names ": "", 
                    //         haveDifferentCategories ? "categories ":"", 
                    //         haveDifferentMacAndPublic ? "mac ": "");
                    // }
                }
            }
        }
        if (!changed)
            break;
    }

    for (int i = state->closest_n - 1; i > 0; i--)
    {
        struct ClosestTo *current = &state->closest[i];

        // Can't mark superseded until we know for sure it's a phone etc.
        if (current->category == CATEGORY_UNKNOWN) continue;

        int64_t mac64 = current->device_64;
        for (int j = i - 1; j >= 0; j--)
        {
            struct ClosestTo *earlier = &state->closest[j];

            if (current->device_64 == earlier->device_64) continue;  // TODO: HANDLE THESE AS DEVICES NOT CLOSEST OBS

            if (current->column == earlier->column)
            {
                earlier->supersededby = mac64;
                // No need to scan back marking the rest as superseeded, right? even though they are.
                break;
            }
            // If earlier was superseded by this one but now it isn't ...
            else if (earlier->supersededby == mac64)
            {
                // Not in same column but has a supersededby value
                // This device used to be superseded by the new one, but now we know it isn't
                earlier->supersededby = 0;
                g_info("%i.%s IS NO LONGER superseded by %i.%s", j, earlier->name, i, current->name);
            }
        }
    }
}
