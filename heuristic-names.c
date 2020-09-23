// name heuristics

// TODO: Allocate categories by name automatically
// for(int i = 0; i < sizeof(phones); i++){
//     if (strcmp(name, phones[i] == 0)) existing->category = CATEGORY_PHONE;
// }

#include "device.h"
#include "utility.h"

void apply_name_heuristics (struct Device* existing, const char* name)
{
    if (string_contains_insensitive(name, "iPhone"))
        existing->category = CATEGORY_PHONE;
    else if (string_endswith(name, "'s phone"))  // some people name them
        // TODO: Remove the name for privacy reasons
        existing->category = CATEGORY_PHONE;
    else if (string_endswith(name, "s phone"))  // some people name them
        // TODO: Remove the name for privacy reasons
        existing->category = CATEGORY_PHONE;
    else if (string_contains_insensitive(name, "Galaxy Note"))
        existing->category = CATEGORY_PHONE;
    else if (string_contains_insensitive(name, "Galaxy A20"))
        existing->category = CATEGORY_PHONE;

    else if (string_contains_insensitive(name, "iPad"))
        existing->category = CATEGORY_TABLET;
    else if (string_contains_insensitive(name, "MacBook pro"))
        existing->category = CATEGORY_COMPUTER;
    else if (string_starts_with(name, "BOOTCAMP"))
        existing->category = CATEGORY_COMPUTER;

    // Watches
    else if (string_starts_with(name, "iWatch"))
        existing->category = CATEGORY_WEARABLE;
    else if (string_starts_with(name, "Apple Watch"))
        existing->category = CATEGORY_WEARABLE;
    else if (string_starts_with(name, "Galaxy Watch"))
        existing->category = CATEGORY_WEARABLE;
    else if (string_starts_with(name, "Galaxy Fit"))
        existing->category = CATEGORY_WEARABLE;
    else if (string_starts_with(name, "Gear S3"))
        existing->category = CATEGORY_WEARABLE;
    else if (string_starts_with(name, "fenix"))
        existing->category = CATEGORY_WEARABLE;
    else if (strncmp(name, "Ionic", 5) == 0)
        existing->category = CATEGORY_WEARABLE; // FITBIT
    else if (strncmp(name, "Versa", 5) == 0)
        existing->category = CATEGORY_WEARABLE; // FITBIT
    else if (string_starts_with(name, "Charge "))  //  Charge 2, 3, 4
        existing->category = CATEGORY_WEARABLE; // FITBIT
    else if (string_starts_with(name, "Inspire HR"))
        existing->category = CATEGORY_WEARABLE; // FITBIT
    else if (string_starts_with(name, "Approach S20"))
        existing->category = CATEGORY_WEARABLE; // Garmin Golf Watch
    else if (string_starts_with(name, "Mi Smart Band"))
        existing->category = CATEGORY_WEARABLE; // Fitness
    else if (string_starts_with(name, "TICKR X"))
        existing->category = CATEGORY_WEARABLE; // Heartrate
    else if (string_starts_with(name, "ID115Plus HR"))
        existing->category = CATEGORY_WEARABLE; // Heartrate
    else if (string_starts_with(name, "ID128Color HM"))
        existing->category = CATEGORY_WEARABLE; // ID128Color HM fitness band
    else if (string_starts_with(name, "HR-BT"))
        existing->category = CATEGORY_WEARABLE; // Heartrate
    else if (string_starts_with(name, "WHOOP"))
        existing->category = CATEGORY_WEARABLE; // Heartrate
    else if (string_starts_with(name, "Alta HR"))
        existing->category = CATEGORY_WEARABLE; // Fitbit Alta Heartrate

    else if (string_starts_with(name, "RS507 "))
        existing->category = CATEGORY_WEARABLE; // Ring barcode scanner

    // FIXED
    else if (strncmp(name, "Tacx Neo 2T", 4) == 0)
        existing->category = CATEGORY_FIXED; // Bike trainer
    else if (strncmp(name, "SCHWINN 170/270", 4) == 0)
        existing->category = CATEGORY_FIXED; // Bike trainer
        
    else if (strncmp(name, "MOLEKULE", 8) == 0)
        existing->category = CATEGORY_FIXED; // Air filter
    else if (strncmp(name, "Nest Cam", 8) == 0)
        existing->category = CATEGORY_FIXED; // Camera
    else if (strncmp(name, "Seos", 4) == 0)
        existing->category = CATEGORY_FIXED; // Credential technology
    else if (string_starts_with(name, "LEDBlue"))
        existing->category = CATEGORY_FIXED; // RGB LED controller
    else if (string_starts_with(name, "ELK-BLEDOM"))
        existing->category = CATEGORY_FIXED; // RGB LED controller
    else if (string_starts_with(name, "SP110E"))
        existing->category = CATEGORY_FIXED; // RGB LED controller
    else if (string_starts_with(name, "SCHLAGE"))
        existing->category = CATEGORY_FIXED; // Smart door lock?
    else if (string_starts_with(name, "WYZE"))
        existing->category = CATEGORY_FIXED; // Smart door lock?
    else if (string_starts_with(name, "bhyve"))
    {
        existing->category = CATEGORY_FIXED; // Sprinkler controller
    }
    else if (string_starts_with(name, "Evluma"))
    {
        existing->category = CATEGORY_FIXED; // Evluma AMAX lighting controller
    }
    else if (string_starts_with(name, "ihoment_H"))
    {
        existing->category = CATEGORY_FIXED; // LED light strip controller
    }
    else if (string_starts_with(name, "BEDJET"))
        existing->category = CATEGORY_FIXED; // Bed temperature controller!
    else if (string_starts_with(name, "[Refrigerator] Samsung"))
        existing->category = CATEGORY_FIXED; // Fridge!
    else if (string_starts_with(name, "eDynamo"))
        existing->category = CATEGORY_FIXED; // Credit card reader
    else if (string_starts_with(name, "Square Reader"))
        existing->category = CATEGORY_FIXED; // Credit card reader
    else if (string_starts_with(name, "Self Checkout"))
        existing->category = CATEGORY_FIXED; // Self-checkout terminal
    else if (string_starts_with(name, "TJQLJ"))
        existing->category = CATEGORY_FIXED; // Checkout? or beacon in TJMAX store

    // TVs
    else if (strcmp(name, "AppleTV") == 0)
        existing->category = CATEGORY_TV;
    else if (strcmp(name, "Apple TV") == 0)
        existing->category = CATEGORY_TV;
    else if (string_starts_with(name, "HiSmart"))       // HiSmart[TV] 2k ...
        existing->category = CATEGORY_TV;
    else if (string_starts_with(name, "BRAVIA"))        // Sony Bravia TVs, VU1
        existing->category = CATEGORY_TV;
    else if (string_starts_with(name, "AT&T TV"))       // AT&T TV
        existing->category = CATEGORY_TV;

    // Beacons
    else if (strncmp(name, "AprilBeacon", 11) == 0)
        existing->category = CATEGORY_BEACON;
    else if (strncmp(name, "abtemp", 6) == 0)
        existing->category = CATEGORY_BEACON;
    else if (strncmp(name, "abeacon", 7) == 0)
        existing->category = CATEGORY_BEACON;
    else if (strncmp(name, "estimote", 8) == 0)
        existing->category = CATEGORY_BEACON;
    else if (strncmp(name, "Tile", 4) == 0)
        existing->category = CATEGORY_BEACON;
    else if (strncmp(name, "LYWSD03MMC", 10) == 0)
        existing->category = CATEGORY_BEACON;

    // Headphones or speakers
    else if (strncmp(name, "Sesh Evo-LE", 11) == 0)
        existing->category = CATEGORY_HEADPHONES; // Skullcandy
    else if (strncmp(name, "F2", 2) == 0)
        existing->category = CATEGORY_HEADPHONES; // Soundpal F2 spakers
    else if (strncmp(name, "Jabra", 5) == 0)
        existing->category = CATEGORY_HEADPHONES;
    else if (string_starts_with(name, "LE-"))     // LE-Bose, LE-<name>Bose, LE-Reserved_N
        existing->category = CATEGORY_HEADPHONES;
    else if (strncmp(name, "LE-reserved_C", 13) == 0)
        existing->category = CATEGORY_HEADPHONES;
    else if (strncmp(name, "Blaze", 5) == 0)
        existing->category = CATEGORY_HEADPHONES;
    else if (strncmp(name, "HarpBT", 6) == 0)
        existing->category = CATEGORY_HEADPHONES;
    else if (string_starts_with(name, "LG HBS1120"))
        existing->category = CATEGORY_HEADPHONES;
    else if (string_starts_with(name, "Z-Link"))
        existing->category = CATEGORY_HEADPHONES;
    else if (string_starts_with(name, "LE_Stealth 700 Xbox"))
        existing->category = CATEGORY_HEADPHONES;
    else if (string_starts_with(name, "LE_WH-H900N"))
        existing->category = CATEGORY_HEADPHONES;  //LE_WH-H900N (h.ear)
    else if (string_starts_with(name, "Bose AE2 SoundLink"))
        existing->category = CATEGORY_HEADPHONES;
    else if (string_starts_with(name, "DSW229Dynamo 1100X"))
        existing->category = CATEGORY_HEADPHONES;   // Subwoofer
    else if (string_starts_with(name, "DSW227Dynamo 600X"))
        existing->category = CATEGORY_HEADPHONES;   // Subwoofer
    else if (string_contains_insensitive(name, "headphone"))
        existing->category = CATEGORY_HEADPHONES;   // Subwoofer
    else if (string_starts_with(name, "LHB-"))
        existing->category = CATEGORY_HEADPHONES;   // LG Soundbar?
    else if (string_starts_with(name, "HTC BS"))
        existing->category = CATEGORY_HEADPHONES;   // Conference speaker

    // BT Speakers
    else if (string_starts_with(name, "Venue-Tile"))
        existing->category = CATEGORY_HEADPHONES;   // Skullcandy headphones with Tile // pub
    else if (string_starts_with(name, "Echo Dot"))
        existing->category = CATEGORY_HEADPHONES;   // Alexa
    else if (string_starts_with(name, "SRS-XB12"))
        existing->category = CATEGORY_HEADPHONES;   // BT Speakers
    else if (string_starts_with(name, "ACTON II"))
        existing->category = CATEGORY_HEADPHONES;   // BT Speakers
    else if (string_starts_with(name, "VIZIO V51"))
        existing->category = CATEGORY_HEADPHONES;   // Soundbar
    else if (string_starts_with(name, "VQ"))
        existing->category = CATEGORY_HEADPHONES;   // Retroradio and speakers

    // TVs
    // e.g. "[TV] Samsung Q70 Series (65)" icon is audio_card
    else if (string_starts_with(name, "[TV] "))
        existing->category = CATEGORY_TV;
    // else if (string_starts_with(name, "[TV] Samsung"))
    //     existing->category = CATEGORY_TV;
    else if (string_starts_with(name, "[Signage] Samsung"))
        existing->category = CATEGORY_TV;
    else if (string_starts_with(name, "[LG] webOS TV"))
        existing->category = CATEGORY_TV;
    else if (string_starts_with(name, "SONY XBR"))
        existing->category = CATEGORY_TV;

    // Receivers
    else if (string_starts_with(name, "YamahaAV"))
        existing->category = CATEGORY_FIXED; // receiver

    // Printers
    else if (string_starts_with(name, "ENVY Photo"))
        existing->category = CATEGORY_FIXED; // printer
    else if (string_starts_with(name, "Sony UP-DX"))
        existing->category = CATEGORY_FIXED; // printer Sony UP-DX100

    // Accessories
    else if (string_starts_with(name, "Apple Pencil"))
        existing->category = CATEGORY_FIXED;  // Not really fixed
    else if (string_starts_with(name, "SPEN 02"))
        existing->category = CATEGORY_FIXED;  // Not really fixed WACOM

    // POS Terminals
    else if (string_starts_with(name, "Tap & Chip"))
        existing->category = CATEGORY_FIXED;  // Shopify tap & chip reader
    else if (string_starts_with(name, "*MOB85"))
        existing->category = CATEGORY_FIXED;  // POS
    else if (string_starts_with(name, "PayRange"))
        existing->category = CATEGORY_FIXED;  // POS
    else if (string_starts_with(name, "Bluesnap"))
        existing->category = CATEGORY_FIXED;  // POS
    else if (string_starts_with(name, "IBM"))
        existing->category = CATEGORY_FIXED;  // POS
    else if (string_starts_with(name, "NWTR040"))
        existing->category = CATEGORY_FIXED;  // POS
    else if (string_starts_with(name, "XXZKJ"))
        existing->category = CATEGORY_FIXED;  // Taiyo Yuden module Beacon? Other?

    // Cars
    else if (string_starts_with(name, "Audi"))
        existing->category = CATEGORY_CAR;
    else if (string_starts_with(name, "VW "))
        existing->category = CATEGORY_CAR;
    else if (strncmp(name, "BMW", 3) == 0)
        existing->category = CATEGORY_CAR;
    else if (string_starts_with(name, "GM_PEPS_"))
        existing->category = CATEGORY_CAR; // maybe the key fob
    else if (strncmp(name, "Subaru", 6) == 0)
        existing->category = CATEGORY_CAR;
    else if (string_starts_with(name, "nuvi"))
        existing->category = CATEGORY_CAR;
    else if (string_starts_with(name, "UberBeacon"))
        existing->category = CATEGORY_CAR; // Uber's dashboard display
    else if (string_starts_with(name, "Lyft Amp"))
        existing->category = CATEGORY_CAR; // Lyft's dashboard display
    else if (string_starts_with(name, "Scosche BTFM4"))
        existing->category = CATEGORY_CAR; // Handsfree car kit
    else if (strncmp(name, "Land Rover", 10) == 0)
    {
        existing->category = CATEGORY_CAR;
    }

// E0:55:3D

    // And some MAC addresses
    else if (string_starts_with(existing->mac, "e0:55:3d"))
    {
        // OEM to Sony? HP?
        optional_set(existing->name, "_Cisco Meraki", NAME_LENGTH); // Bluetooth and Wifi and beacon
        existing->category = CATEGORY_FIXED;
    }
    else if (string_starts_with(existing->mac, "0c:96:e6"))
    {
        // OEM to Sony? HP?
        optional_set(existing->name, "_Cloud Network Tech", NAME_LENGTH); // (Samoa) Inc
        existing->category = CATEGORY_FIXED;
    }
    else if (string_starts_with(existing->mac, "cc:04:b4"))
    {
        optional_set(existing->name, "_Select Comfort", NAME_LENGTH);
        existing->category = CATEGORY_FIXED;
    }
    else if (string_starts_with(existing->mac, "64:db:a0"))
    {
        optional_set(existing->name, "_Select Comfort", NAME_LENGTH);
        existing->category = CATEGORY_FIXED;
    }
    // TODO: Android device names
}