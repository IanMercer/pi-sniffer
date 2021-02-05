#include "overlaps.h"
#include "closest.h"
#include "device.h"

/*
    Do these two devices overlap in time? If so they cannot be the same device
    (allowed to touch given granularity of time)
*/
bool overlapsClosest(struct ClosestTo *a, struct ClosestTo *b)
{
    // If the earlier device has just one observation it's too soon to say if the latter one is the same device
    // In a transit situation we get many devices passing by with just one ping, these are not superceded
    if (a->count == 1 && a->latest <= b->earliest){
        return TRUE; // not an overlap but unlikely to be same device
    }
    if (b->count == 1 && b->latest <= a->earliest){
        return TRUE; // not an overlap but unlikely to be same device
    }
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

    for (int k = state->closest_n - 1; k > 0; k--)
    {
        bool changed = false;

        for (int i = state->closest_n - 1; i > 0; i--)
        {
            for (int j = i - 1; j > 0; j--)
            {
                struct ClosestTo *a = &state->closest[i];
                struct ClosestTo *b = &state->closest[j];

                if (a->device_64 == b->device_64) continue;  // TODO: HANDLE THESE AS DEVICES NOT CLOSEST OBS

                if (a->column != b->column)
                    continue;

                bool over = overlapsClosest(a, b);

                // cannot be the same device if either has a public address (or we don't have an address type yet)
                bool haveDifferentAddressTypes = (a->addressType > 0 && b->addressType > 0 && 
                    a->addressType != b->addressType);

                // cannot be the same if they both have names and the names are different
                // but don't reject _ names as they are temporary and will get replaced
                bool haveDifferentNames = (strlen(a->name) > 0) && (strlen(b->name) > 0) 
                    // can reject as soon as they both have a partial name that is same type but doesn't match
                    && (a->name_type == b->name_type)              
                    && (g_strcmp0(a->name, b->name) != 0);

                // cannot be the same if they both have known categories and they are different
                // Used to try to blend unknowns in with knowns but now we get category right 99.9% of the time, no longer necessary
                bool haveDifferentCategories = (a->category != b->category); // && (a->category != CATEGORY_UNKNOWN) && (b->category != CATEGORY_UNKNOWN);

                bool haveDifferentMacAndPublic = (a->addressType == PUBLIC_ADDRESS_TYPE && (a->device_64 != b->device_64));

                if (over || haveDifferentAddressTypes || haveDifferentNames || haveDifferentCategories || 
                    haveDifferentMacAndPublic)
                {
                    b->column++;
                    changed = true;
                    // g_print("Compare %i to %i and bump %4i %s to %i, %i %i %i\n", i, j, b->id, b->mac, b->column, over, haveDifferentAddressTypes, haveDifferentNames);
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
            }
            // If earlier was supersededby this one but now it isn't ...
            else if (earlier->supersededby == mac64)
            {
                // Not in same column but has a supersededby value
                // This device used to be superseded by the new one, but now we know it isn't
                earlier->supersededby = 0;
                g_info("%s IS NO LONGER superseded by %s", earlier->name, current->name);
            }
        }
    }
}
