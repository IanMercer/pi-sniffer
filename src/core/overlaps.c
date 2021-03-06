#include "overlaps.h"
#include "closest.h"
#include "knn.h"
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
        // int delta_time = difftime(a_earliest, b_latest);
        // TODO: Return a probability distribution for these being related
        return FALSE;
        // return delta_time > 180;  // more than 180s and these are certainly unrelated devices, except not
        // could leave an area and come back
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

    time_t now = time(0);

    for (struct ClosestHead* a = state->closestHead; a != NULL; a=a->next)
    {
        a->debug_supersededby_prior = a->supersededby;
        a->supersededby = 0;
        a->superseded_probability = 0.0;
    }

    for (struct ClosestHead* a = state->closestHead; a != NULL; a=a->next)
    {
        int delta_time = difftime(now, a->closest->latest);

        // Ignore any that are expired
        if (delta_time > 400) continue;

        // TODO: Find the BEST fit and use that, proceed in order, best first, only claim 1 per leading device

        // Examine lower triangle, only looking at ones that were last seen prior to this one's last seen time
        for (struct ClosestHead* b = a->next; b != NULL; b=b->next)
        {
            
// TODO: Crash here - concurrency issue, need a lock around CLOSEST
// TODO: Happened again!

            g_assert(a->mac64 != b->mac64);
            if (b->supersededby != 0) continue;  // already claimed

            // We have an A and a B, now do pairwise comparison of all A's and all B's
            // if any of them overlap then these two devices cannot be successors

            // cannot be the same device if one is public and the other is random address type
            // (or we don't have an address type yet)
            bool haveDifferentAddressTypes = (a->addressType > 0 && b->addressType > 0 && 
                a->addressType != b->addressType);

            // cannot be the same device if either is a public mac address (already know macs are different)
            bool haveDifferentMacAndPublic = (a->addressType == PUBLIC_ADDRESS_TYPE || b->addressType == PUBLIC_ADDRESS_TYPE);

            // cannot be the same if they both have names and the names are different
            // but don't reject _ names as they are temporary and will get replaced
            bool haveDifferentNames =
                // can reject as soon as they both have a partial name that is same type but doesn't match
                // but cannot reject while one or other has a temporary name as it may match
                (a->name_type == b->name_type || 
                    (a->name_type==nt_alias || b->name_type == nt_alias) ||   // if known beacon both obs would know it
                    (a->name_type>=nt_known && b->name_type>=nt_known))       // both well known enough to be same
                    && (g_strcmp0(a->name, b->name) != 0);

            // A paired phone may have a name and be compared with an unpaired instance of itself (HACK)
            if (string_ends_with(a->name, " phone") && g_strcmp0(b->name, "iPhone") == 0)
            {
                haveDifferentNames = false;
            }
            if (string_ends_with(b->name, " phone") && g_strcmp0(a->name, "iPhone") == 0)
            {
                haveDifferentNames = false;
            }

            // cannot be the same if they both have known categories and they are different
            bool haveDifferentCategories = (a->category != b->category) || (a->category == CATEGORY_UNKNOWN);

            bool might_supersede = !(haveDifferentAddressTypes || haveDifferentNames || haveDifferentCategories || 
                    haveDifferentMacAndPublic);

            if (!might_supersede) continue;

            // Require at least one matching access point
            // e.g. two devices at opposite ends of the mesh that never overlapped are unlikely to be the same device

            bool allBlips = true;  // When looking for a blip, all of the readings need to be single values
            bool over = false;

            for (struct ClosestTo* am = a->closest; am != NULL; am = am->next)
            {
                for (struct ClosestTo* bm = b->closest; bm != NULL; bm = bm->next)
                {
                    // Compare same access point records
                    if (am->access_point->id != bm->access_point->id) continue;

                    //g_debug("Compare %s(%i) x %s(%i) for %s", am->name, am->name_type, bm->name, bm->name_type,
                    //    am->access_point->client_id);

                    // Same access point so the times are comparable

                    bool blip2 = justABlip(am->earliest, am->latest, am->count, bm->earliest, bm->latest, bm->count);

                    // We know A was around after B was last seen so only need to check one direction                    
                    // to see if A could be entirely after B
                    bool over2 = overlapsOneWay(am->earliest, bm->latest);

                    // Could model probability based on non-overlap distance
                    // int delta_time = difftime(a_earliest, b_latest);

                    // // How close are the two in distance
                    // double delta = compare_closest(am->device_64, bm->device_64, state);

                    allBlips = allBlips && blip2;
                    over = over || over2;
                }
            }

            if (allBlips || over)
            {
                // could not be same device with new MAC address
                might_supersede = false;
            }

            // How close are the two in distance
            double probability_by_distance = compare_closest(a, b, state);

            char a_mac[18];
            char b_mac[18];
            mac_64_to_string(a_mac, sizeof(a_mac), a->mac64);
            mac_64_to_string(b_mac, sizeof(b_mac), b->mac64);

            // Tune the 0.05 parameter: 5% chance they are the same by distances
            // May improve taking time into account
            if (might_supersede)
            {
                if (probability_by_distance > 0.01)   // 0.046, 0.0212 observed as valid
                {
                    // All of the observations are consistent with being superceded
                    b->supersededby = a->mac64;
                    b->superseded_probability = probability_by_distance;
                    // // if (a->category == CATEGORY_PHONE)
                    // // {
                    // //     char* summary = b->supersededby == b->debug_supersededby_prior ?
                    // //         "superceded" :
                    // //         (b->debug_supersededby_prior == 0 ? "NEW SUPERSEDED" : "REPLACE SUPERSEDED");
                    // //     g_debug("%s:%s %s %s:%s prob %.5f", a_mac, a->name, summary, b_mac, b->name, probability_by_distance);
                    // // }
                    // A can only supersede one of the B
                    break;
                }
                else
                {
                    // // if (a->category == CATEGORY_PHONE)
                    // // {
                    // //     g_debug("%s:%s almost superceded %s:%s prob %.5f", a_mac, a->name, b_mac, b->name, probability_by_distance);
                    // // }

                    if (b->supersededby == 0 || b->superseded_probability < probability_by_distance)
                    {
                        b->superseded_probability = probability_by_distance;
                    }
                }
            }
            else
            {
                // // if (a->category == CATEGORY_PHONE && !over)
                // // {
                // //     g_debug("%s:%s cannot superceded %s:%s %s%s%s%s%s%s prob %.5f", a_mac, a->name, b_mac, b->name, 
                // //         allBlips ? "blip " : "",
                // //         over ? "overlaps " : "",
                // //         haveDifferentAddressTypes ? "addressTypes " : "",
                // //         haveDifferentNames ? "names ": "", 
                // //         haveDifferentCategories ? "categories ":"", 
                // //         haveDifferentMacAndPublic ? "mac ": "",
                // //         probability_by_distance);
                // // }
            }
        }
    }

}
