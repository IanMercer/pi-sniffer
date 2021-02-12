// mac address heuristics - may be able to determine manufacturer from a public mac address and thus the category

#include <stdio.h>
#include "device.h"
#include "utility.h"
#include "heuristics.h"

void apply_mac_address_heuristics (struct Device* device)
{
    // And some MAC addresses
    // B8:BC:5B Samsung TV
    // D4:9D:C0 Samsung TV
    // F8:3F:51 Samsung __
    // 5C:C1:D7 Samsung __
    // C4:F3:12 Texas Instruments

    if (string_starts_with(device->mac, "8C:DE:52"))
    {
        // various IoT
          // Fabless Semi: headsets, car kits, ...
        set_name(device, "ISSC Technologies Corp.", nt_manufacturer);
    }
    else if (string_starts_with(device->mac, "60:03:08"))
    {
        set_name(device, "Apple __", nt_manufacturer);
        // Probably an AppleTV since they have fixed mac addresses
    }
    else if (string_starts_with(device->mac, "0C:8C:DC"))
    {
        // various IoT
        set_name(device, "Suunto", nt_manufacturer);
        device->category = CATEGORY_WEARABLE;
    }
    else if (string_starts_with(device->mac, "CC:93:4A"))
    {
        // various IoT
        set_name(device, "Sierra Wireless", nt_manufacturer);
        device->category = CATEGORY_FIXED;
    }
    else if (string_starts_with(device->mac, "CC:6E:A4"))
    {
        // Samsung phone
        set_name(device, "Samsung", nt_manufacturer);
        // Maybe a TV
        // existing->category = CATEGORY_PHONE;
    }
    else if (string_starts_with(device->mac, "64:b8:53"))
    {
        // Samsung phone
        set_name(device, "Samsung phone", nt_manufacturer);
        device->category = CATEGORY_PHONE;
    }
    else if (string_starts_with(device->mac, "e0:55:3d"))
    {
        // OEM to Sony? HP?
        set_name(device, "Cisco Meraki", nt_manufacturer);
         // Bluetooth and Wifi and beacon
        device->category = CATEGORY_FIXED;
    }
    else if (string_starts_with(device->mac, "0c:96:e6"))
    {
        // OEM to Sony? HP?
        set_name(device, "Cloud Network Tech", nt_manufacturer);// (Samoa) Inc
        device->category = CATEGORY_FIXED;
    }
    else if (string_starts_with(device->mac, "cc:04:b4"))
    {
        set_name(device, "Select Comfort", nt_manufacturer);
        device->category = CATEGORY_FIXED;
    }
    else if (string_starts_with(device->mac, "64:db:a0"))
    {
        set_name(device, "Select Comfort", nt_manufacturer);
        device->category = CATEGORY_FIXED;
    }
    else if (string_starts_with(device->mac, "C0:28:8d"))
    {
        set_name(device, "Logitech", nt_manufacturer);
        device->category = CATEGORY_FIXED;
    }
    else if (string_starts_with(device->mac, "88:c6:26"))
    {
        set_name(device, "Logitech", nt_manufacturer);
        device->category = CATEGORY_FIXED;
    }
    else if (string_starts_with(device->mac, "00:25:52"))
    {
        set_name(device, "VXI Corp", nt_manufacturer);
        device->category = CATEGORY_HEADPHONES;
    }
    else if (string_starts_with(device->mac, "cc:70:ed"))
    {
        set_name(device, "Cisco Systems", nt_manufacturer);
        device->category = CATEGORY_FIXED;
    }
    else if (string_starts_with(device->mac, "6c:9a:c9"))
    {
        set_name(device, "Valentine Radar", nt_manufacturer);
        device->category = CATEGORY_CAR;
    }
    else if (string_starts_with(device->mac, "00:1E:91"))
    {
        set_name(device, "KIMIN Electronic", nt_manufacturer);  // AirPlay - Denon?
        device->category = CATEGORY_CAR;
    }
    else if (string_starts_with(device->mac, "04:EE:03"))
    {
        set_name(device, "Texas Instruments", nt_manufacturer);  // Chipset
    }
    else if (string_starts_with(device->mac, "00:1B:DC"))
    {
        set_name(device, "Vencer", nt_manufacturer);  // Co. Ltd. - BT Headsets but also plant monitors
        soft_set_8(&device->category, CATEGORY_HEADPHONES);
    }
    else if (string_starts_with(device->mac, "4C:87:5D"))
    {
        set_name(device, "Bose", nt_manufacturer);
        soft_set_8(&device->category, CATEGORY_HEADPHONES);
    }
    else if (string_starts_with(device->mac, "88:6b:0f") || string_starts_with(device->mac, "88:6B:0F"))
    {
        char beaconName[NAME_LENGTH];
        snprintf(beaconName, sizeof(beaconName), "%s %.2x:%.x2:%.2x", "Milwaukee",
            (int8_t)(device->mac64 >> 16) & 0xff,
            (int8_t)(device->mac64 >> 8) & 0xff,
            (int8_t)(device->mac64 >> 0) & 0xff);

        set_name(device, beaconName, nt_manufacturer);
        device->category = CATEGORY_BEACON;
    }

    // TODO: Android device names
}