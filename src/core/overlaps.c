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
        //return FALSE;
        int delta_time = difftime(a_earliest, b_latest);
        return delta_time > 180;  // more than 180s and these are certainly unrelated devices
        // Apple Watch 137s apart - this is where probability would come in, reduce over time
    }
    return TRUE;      // must overlap if not entirely after or too far after
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

    Pairwise iteration (A, B)
    Multiple instances of A and B each with an access point name A0, A1, A2, ...  B0, B2, ...

    A and B could be successors iff ...
        A and B have matching names, categories, ...
        Every matching pair satisfies A before B
        Probability is based on lowest gap between A(ap) and B(ap)


*/
void pack_closest_columns(struct OverallState* state)
{
    // Working backwards in time through the array
    // Push every device back to column zero as category may have changed
    for (int i = state->closest_n - 1; i > 0; i--)
    {
        struct ClosestTo *a = &state->closest[i];
        a->mark_pass_2 = false;
        a->supersededby = 0;
    }

    for (int i = state->closest_n - 1; i > 0; i--)
    {
        struct ClosestTo *a = &state->closest[i];
        if (a->mark_pass_2) continue;   // already did this A as an Am lower down

        // TODO: Find the BEST fit and use that, proceed in order, best first, only claim 1 per leading device

        for (int j = i - 1; j >= 0; j--)
        {
            struct ClosestTo *b = &state->closest[j];
            if (a->device_64 == b->device_64) continue;
            if (b->mark_pass_2) continue;
            if (b->supersededby != 0) continue;  // already claimed

            // We have an A and a B, now do pairwise comparison of all A's and all B's
            // if any of them overlap then these two devices cannot be successors

            // cannot be the same device if one is public and the other is random address type
            // (or we don't have an address type yet)
            bool haveDifferentAddressTypes = (a->addressType > 0 && b->addressType > 0 && 
                a->addressType != b->addressType);

            // cannot be the same if they both have names and the names are different
            // but don't reject _ names as they are temporary and will get replaced
            bool haveDifferentNames =
                // can reject as soon as they both have a partial name that is same type but doesn't match
                // but cannot reject while one or other has a temporary name as it may match
                (a->name_type == b->name_type || (a->name_type>=nt_known && b->name_type>=nt_known)) && 
                    (g_strcmp0(a->name, b->name) != 0);

            // cannot be the same if they both have known categories and they are different
            bool haveDifferentCategories = (a->category != b->category);

            // cannot be the same device if either is a public mac address (already know macs are different)
            bool haveDifferentMacAndPublic = (a->addressType == PUBLIC_ADDRESS_TYPE || b->addressType == PUBLIC_ADDRESS_TYPE);

            bool might_supersede = !(haveDifferentAddressTypes || haveDifferentNames || haveDifferentCategories || 
                    haveDifferentMacAndPublic);

            // Require at least one matching access point
            // e.g. two devices at opposite ends of the mesh that never overlapped are unlikely to be the same device
            bool atLeastOneMatch = false;

            // scan all records with b as their mac address
            for (int ii = i; ii >= 0; ii--)
            {
                struct ClosestTo *am = &state->closest[ii];
                if (am->device_64 != a->device_64) continue;  // not an 'A'
                am->mark_pass_2 = true;  // mark seen so outer loop skips all A's

                // After marking As we can skip any comparisons if they cannot work
                if (!might_supersede)
                {
                    continue;
                }
                for (int jj = j; jj >= 0; jj--)
                {
                    struct ClosestTo *bm = &state->closest[jj];
                    if (bm->device_64 != b->device_64) continue;  // not a 'B'

                    if (am->access_point->id != bm->access_point->id) continue;

                    atLeastOneMatch = true;
                    //g_debug("Compare %s(%i) x %s(%i) for %s", am->name, am->name_type, bm->name, bm->name_type,
                    //    am->access_point->client_id);

                    // Same access point so the times are comparable

                    bool blip = justABlip(am->earliest, am->latest, a->count, bm->earliest, bm->latest, bm->count);
                    bool over = overlapsOneWay(am->earliest, bm->latest);

                    if (blip || over)
                    {
                        // could not be same device with new MAC address
                        might_supersede = false;
                    }
                }
            }

            if (might_supersede && atLeastOneMatch)
            {
                // All of the observations are consistent with being superceded
                b->supersededby = a->device_64;
                //g_debug("Mark supersede %s x %s", a->name, b->name);

                // A can only supersede one of the B
                break;
            }

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
        } // for j
    }  // for i

}
