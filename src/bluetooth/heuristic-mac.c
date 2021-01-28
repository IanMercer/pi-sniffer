// mac address heuristics - may be able to determine manufacturer from a public mac address and thus the category

#include <stdio.h>
#include "device.h"
#include "utility.h"
#include "heuristics.h"

void apply_mac_address_heuristics (struct Device* existing)
{
    // And some MAC addresses
    // B8:BC:5B Samsung TV
    // D4:9D:C0 Samsung TV
    // F8:3F:51 Samsung __
    // 5C:C1:D7 Samsung __
    // C4:F3:12 Texas Instruments

    if (string_starts_with(existing->mac, "8C:DE:52"))
    {
        // various IoT
          // Fabless Semi: headsets, car kits, ...
        set_name(existing, "ISSC Technologies Corp.", nt_manufacturer);
    }
    else if (string_starts_with(existing->mac, "60:03:08"))
    {
        set_name(existing, "Apple __", nt_manufacturer);
        // Probably an AppleTV since they have fixed mac addresses
    }
    else if (string_starts_with(existing->mac, "0C:8C:DC"))
    {
        // various IoT
        set_name(existing, "Suunto", nt_manufacturer);
        existing->category = CATEGORY_WEARABLE;
    }
    else if (string_starts_with(existing->mac, "CC:93:4A"))
    {
        // various IoT
        set_name(existing, "Sierra Wireless", nt_manufacturer);
        existing->category = CATEGORY_FIXED;
    }

    else if (string_starts_with(existing->mac, "64:b8:53"))
    {
        // Samsung phone
        set_name(existing, "Samsung phone", nt_manufacturer);
        existing->category = CATEGORY_PHONE;
    }
    else if (string_starts_with(existing->mac, "e0:55:3d"))
    {
        // OEM to Sony? HP?
        set_name(existing, "Cisco Meraki", nt_manufacturer);
         // Bluetooth and Wifi and beacon
        existing->category = CATEGORY_FIXED;
    }
    else if (string_starts_with(existing->mac, "0c:96:e6"))
    {
        // OEM to Sony? HP?
        set_name(existing, "Cloud Network Tech", nt_manufacturer);// (Samoa) Inc
        existing->category = CATEGORY_FIXED;
    }
    else if (string_starts_with(existing->mac, "cc:04:b4"))
    {
        set_name(existing, "Select Comfort", nt_manufacturer);
        existing->category = CATEGORY_FIXED;
    }
    else if (string_starts_with(existing->mac, "64:db:a0"))
    {
        set_name(existing, "Select Comfort", nt_manufacturer);
        existing->category = CATEGORY_FIXED;
    }
    else if (string_starts_with(existing->mac, "C0:28:8d"))
    {
        set_name(existing, "Logitech", nt_manufacturer);
        existing->category = CATEGORY_FIXED;
    }
    else if (string_starts_with(existing->mac, "88:c6:26"))
    {
        set_name(existing, "Logitech", nt_manufacturer);
        existing->category = CATEGORY_FIXED;
    }
    else if (string_starts_with(existing->mac, "00:25:52"))
    {
        set_name(existing, "VXI Corp", nt_manufacturer);
        existing->category = CATEGORY_HEADPHONES;
    }
    else if (string_starts_with(existing->mac, "cc:70:ed"))
    {
        set_name(existing, "Cisco Systems", nt_manufacturer);
        existing->category = CATEGORY_FIXED;
    }
    else if (string_starts_with(existing->mac, "6c:9a:c9"))
    {
        set_name(existing, "Valentine Radar", nt_manufacturer);
        existing->category = CATEGORY_CAR;
    }
    else if (string_starts_with(existing->mac, "00:1E:91"))
    {
        set_name(existing, "KIMIN Electronic", nt_manufacturer);  // AirPlay - Denon?
        existing->category = CATEGORY_CAR;
    }
    else if (string_starts_with(existing->mac, "04:EE:03"))
    {
        set_name(existing, "Texas Instruments", nt_manufacturer);  // Chipset
    }
    else if (string_starts_with(existing->mac, "00:1B:DC"))
    {
        set_name(existing, "Vencer", nt_manufacturer);  // Co. Ltd. - BT Headsets but also plant monitors
        soft_set_8(&existing->category, CATEGORY_HEADPHONES);
    }
    else if (string_starts_with(existing->mac, "4C:87:5D"))
    {
        set_name(existing, "Bose", nt_manufacturer);
        soft_set_8(&existing->category, CATEGORY_HEADPHONES);
    }
    else if (string_starts_with(existing->mac, "88:6b:0f") || string_starts_with(existing->mac, "88:6B:0F"))
    {
        char beaconName[NAME_LENGTH];
        snprintf(beaconName, sizeof(beaconName), "%s %.2x:%.x2:%.2x", "Milwaukee",
            (int8_t)(existing->mac64 >> 16) & 0xff,
            (int8_t)(existing->mac64 >> 8) & 0xff,
            (int8_t)(existing->mac64 >> 0) & 0xff);

        set_name(existing, beaconName, nt_manufacturer);
        existing->category = CATEGORY_BEACON;
    }

    // TODO: Android device names
}