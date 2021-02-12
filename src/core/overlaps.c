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
    if (a_earliest >= b_latest - 3)  // a is entirely after b (3s allowance for clock skew - bug should not need it)
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

    for (struct ClosestHead* a = state->closestHead; a != NULL; a=a->next)
    {
        a->supersededby = 0;
    }

    for (struct ClosestHead* a = state->closestHead; a != NULL; a=a->next)
    {
        // TODO: Find the BEST fit and use that, proceed in order, best first, only claim 1 per leading device

        // Examine lower triangle, only looking at ones that were last seen prior to this one's last seen time
        for (struct ClosestHead* b = a->next; b != NULL; b=b->next)
        {
            if (a->mac64 == b->mac64) continue;
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
                (a->name_type == b->name_type || 
                    (a->name_type==nt_alias || b->name_type == nt_alias) ||   // if known beacon both obs would know it
                    (a->name_type>=nt_known && b->name_type>=nt_known))       // both well known enough to be same
                    && (g_strcmp0(a->name, b->name) != 0);

            // cannot be the same if they both have known categories and they are different
            bool haveDifferentCategories = (a->category != b->category);

            // cannot be the same device if either is a public mac address (already know macs are different)
            bool haveDifferentMacAndPublic = (a->addressType == PUBLIC_ADDRESS_TYPE || b->addressType == PUBLIC_ADDRESS_TYPE);

            bool might_supersede = !(haveDifferentAddressTypes || haveDifferentNames || haveDifferentCategories || 
                    haveDifferentMacAndPublic);

            if (!might_supersede) continue;

            // Require at least one matching access point
            // e.g. two devices at opposite ends of the mesh that never overlapped are unlikely to be the same device

            for (struct ClosestTo* am = a->closest; am != NULL; am = am->next)
            {
                // Triangular comparison against earlier last seen pings
                for (struct ClosestTo* bm = am->next; bm != NULL; bm = bm->next)
                {
                    // Compare same access point records
                    if (am->access_point->id != bm->access_point->id) continue;

                    //g_debug("Compare %s(%i) x %s(%i) for %s", am->name, am->name_type, bm->name, bm->name_type,
                    //    am->access_point->client_id);

                    // Same access point so the times are comparable

                    bool blip = justABlip(am->earliest, am->latest, am->count, bm->earliest, bm->latest, bm->count);
                    bool over = overlapsOneWay(am->earliest, bm->latest);

                    // // How close are the two in distance
                    // double delta = compare_closest(am->device_64, bm->device_64, state);

                    if (blip || over)
                    {
                        // could not be same device with new MAC address
                        might_supersede = false;
                    }
                }
            }

            // How close are the two in distance
            double delta = compare_closest(a, b, state);

            char a_mac[18];
            char b_mac[18];
            mac_64_to_string(a_mac, sizeof(a_mac), a->mac64);
            mac_64_to_string(b_mac, sizeof(b_mac), b->mac64);

            // Tune the 0.05 parameter: 5% chance they are the same by distances
            // May improve taking time into account
            if (might_supersede && delta > 0.05)
            {
                // All of the observations are consistent with being superceded
                b->supersededby = a->mac64;

                //g_debug("%s:%s superceded %s:%s prob %.3f", a_mac, a->name, b_mac, b->name, delta);
                // A can only supersede one of the B
                break;
            }
            else if (might_supersede && delta > 0)  // logging for development
            {
                //g_debug("%s:%s !> %s:%s p=%.3f", a_mac, a->name, b_mac, b->name, delta);
            }

            //Log to see why entries with the same name are failing
            // if (g_strcmp0(a->name, "Apple Watch") == 0 && g_strcmp0(b->name, "Apple Watch") == 0)
            // {
            //     g_debug("%s/%s %s/%s      %s%s%s%s%s delta=%.2f", 
            //         a->name, am->access_point->client_id, 
            //         b->name, bm->access_point->client_id,
            //         might_supersede ? "might supersede" : "not supersede",
            //         haveDifferentAddressTypes ? "addressTypes " : "",
            //         haveDifferentNames ? "names ": "", 
            //         haveDifferentCategories ? "categories ":"", 
            //         haveDifferentMacAndPublic ? "mac ": "",
            //         delta);
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
