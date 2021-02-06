#include "overlaps.h"
#include "closest.h"
#include "device.h"

/*
    Do these two devices overlap in time? If so they cannot be the same device
    (allowed to touch given granularity of time)
*/
bool overlapsClosest(struct ClosestTo *a, struct ClosestTo *b)
{
    if (a->earliest >= b->latest)  // a is entirely after b
    {
        int delta_time = difftime(a->earliest, b->latest);
        return delta_time > 10;  // more than 10s and these are probably unrelated devices
    }
    if (b->earliest >= a->latest) // b is entirely after a
    {
        int delta_time = difftime(b->earliest, a->latest);
        return delta_time > 10;  // more than 10s and these are probably unrelated devices
    }
    return TRUE;      // must overlap if not entirely after or before
}

/*
    Was this just a blip?
*/
bool justABlip(struct ClosestTo *a, struct ClosestTo *b)
{
    // If the earlier device has just one observation it's too soon to say if the latter one is the same device
    // In a transit situation we get many devices passing by with just one ping, these are not superceded
    int delta = abs(difftime(a->earliest, b->latest));
    // under 5s - unlikely to get two transmissions on different macs from same device
    // over 60s - unlikely to be the same device (first left, second arrived)
    if (a->count == 1 && a->latest <= b->earliest && (delta < 5 || delta > 60))
    {
        return TRUE; // unlikely to be same device
    }
    if (b->count == 1 && b->latest <= a->earliest && (delta < 5 || delta > 60))
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
            for (int j = i - 1; j >= 0; j--)
            {
                struct ClosestTo *a = &state->closest[i];
                struct ClosestTo *b = &state->closest[j];

                if (a->device_64 == b->device_64) continue;  // TODO: HANDLE THESE AS DEVICES NOT CLOSEST OBS

                // How to handle two observations from the different access points
                //if (a->access_point->id != b->access_point->id) continue;

                if (a->column != b->column)
                    continue;

                bool blip = justABlip(a,b);
                bool over = overlapsClosest(a, b);

                // cannot be the same device if one is public and the other is random address type
                // (or we don't have an address type yet)
                bool haveDifferentAddressTypes = (a->addressType > 0 && b->addressType > 0 && 
                    a->addressType != b->addressType);

                // cannot be the same if they both have names and the names are different
                // but don't reject _ names as they are temporary and will get replaced
                bool haveDifferentNames = (strlen(a->name) > 0) && (strlen(b->name) > 0) 
                    // can reject as soon as they both have a partial name that is same type but doesn't match
                    // but cannot reject while one or other has a temporary name as it may match
                    && (a->name_type >= nt_device && b->name_type >= nt_device)
                    && (g_strcmp0(a->name, b->name) != 0);

                // cannot be the same if they both have known categories and they are different
                // Used to try to blend unknowns in with knowns but now we get category right 99.9% of the time,
                //  no longer necessary
                bool haveDifferentCategories = (a->category != b->category); // && (a->category != CATEGORY_UNKNOWN) && (b->category != CATEGORY_UNKNOWN);

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
            }
            // If earlier was supersededby this one but now it isn't ...
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
