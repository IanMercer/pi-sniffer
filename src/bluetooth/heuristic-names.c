// name heuristics - may be able to determin category from name

#include <stdio.h>
#include "device.h"
#include "utility.h"
#include "heuristics.h"

void apply_name_heuristics (struct Device* existing, const char* name)
{
    if (string_contains_insensitive(name, "iPhone"))
    {
        existing->category = CATEGORY_PHONE;
    }
    else if (string_ends_with(name, " phone"))  // some people name them
    {
        // Privacy
        g_strlcpy(existing->name, "*****'s phone", NAME_LENGTH);
        existing->category = CATEGORY_PHONE;
    }
    else if (string_contains_insensitive(name, "Galaxy Note"))
        existing->category = CATEGORY_PHONE;
    else if (string_contains_insensitive(name, "Galaxy A10"))
        existing->category = CATEGORY_PHONE;
    else if (string_contains_insensitive(name, "Galaxy A20"))
        existing->category = CATEGORY_PHONE;
    else if (string_contains_insensitive(name, "iPad"))
    {
        existing->category = CATEGORY_TABLET;
    }
    // Privacy
    else if (string_contains_insensitive(name, "s Mac"))  // MacBook, Mac Pro, Mac Book, ..
    {
        // name could be longer than NAME_LENGTH at this point, maybe someone has a very long name
        char* remainder = g_strstr_len(name, strlen(name), "Mac");
        g_strlcpy(existing->name, "*****'s Mac", NAME_LENGTH);
        g_strlcpy(existing->name+8, remainder, NAME_LENGTH-8);
        existing->category = CATEGORY_COMPUTER;
    }
    else if (string_contains_insensitive(name, "MacBook"))
    {
        // Privacy
        g_strlcpy(existing->name, "*****'s MacBook", NAME_LENGTH);
        existing->category = CATEGORY_COMPUTER;
    }
    else if (string_starts_with(name, "BOOTCAMP"))
        existing->category = CATEGORY_COMPUTER;

    // Watches
    else if (string_starts_with(name, "iWatch"))
        existing->category = CATEGORY_WATCH;
    else if (string_starts_with(name, "Apple Watch"))
        existing->category = CATEGORY_WATCH;
    else if (string_starts_with(name, "Galaxy Watch"))
        existing->category = CATEGORY_WATCH;
    else if (string_starts_with(name, "Spartan Trainer"))
        existing->category = CATEGORY_WATCH; // Suunto
    else if (string_starts_with(name, "Gear S3"))
        existing->category = CATEGORY_WATCH;
    else if (string_starts_with(name, "Approach S20"))
        existing->category = CATEGORY_WATCH; // Garmin Golf Watch
    else if (string_starts_with(name, "fenix"))
        existing->category = CATEGORY_WATCH;
    //else if (string_equals(name, "Hum")) // toothbrush
    //    existing->category = CATEGORY_FIXED;

    else if (string_starts_with(name, "Galaxy Fit"))
        existing->category = CATEGORY_WEARABLE;
    else if (string_starts_with(name, "Ionic"))
        existing->category = CATEGORY_WEARABLE; // FITBIT
    else if (string_starts_with(name, "Zip"))
        existing->category = CATEGORY_WEARABLE; // FITBIT Zip
    else if (string_starts_with(name, "Versa"))
        existing->category = CATEGORY_WEARABLE; // FITBIT
    else if (string_starts_with(name, "Charge "))  //  Charge 2, 3, 4
        existing->category = CATEGORY_WEARABLE; // FITBIT
    else if (string_starts_with(name, "Inspire HR"))
        existing->category = CATEGORY_WEARABLE; // FITBIT
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

    // FIXED
    else if (string_starts_with(name, "Tacx Neo 2T"))
        existing->category = CATEGORY_FITNESS; // Bike trainer
    else if (string_starts_with(name, "SCHWINN 170/270"))
        existing->category = CATEGORY_FITNESS; // Bike trainer
    else if (string_starts_with(name, "HT SANA"))
        existing->category = CATEGORY_FITNESS; // Massage chair
    else if (string_starts_with(name, "MOLEKULE"))
        existing->category = CATEGORY_APPLIANCE; // Air filter
    else if (string_starts_with(name, "iFlex_"))
        existing->category = CATEGORY_APPLIANCE; // Fluke power meter?


    else if (string_starts_with(name, "Nest Cam"))
        existing->category = CATEGORY_SECURITY; // Camera
    else if (string_starts_with(name, "Seos"))
        existing->category = CATEGORY_SECURITY; // Credential technology
    else if (string_starts_with(name, "SCHLAGE"))
        existing->category = CATEGORY_SECURITY; // Smart door lock?
    else if (string_starts_with(name, "WYZE"))
        existing->category = CATEGORY_SECURITY; // Smart door lock?
    else if (string_starts_with(name, "Kuna"))
        existing->category = CATEGORY_SECURITY;
    else if (string_starts_with(name, "Dropcam"))
        existing->category = CATEGORY_SECURITY;

    // Category

    else if (string_starts_with(name, "D3400"))
        existing->category = CATEGORY_CAMERA; // Nikon


    // Spriklers

    else if (string_starts_with(name, "bhyve"))
        existing->category = CATEGORY_SPRINKLERS; // Sprinkler controller

    // LIGHTING

    else if (string_starts_with(name, "Hue Lamp"))
        existing->category = CATEGORY_LIGHTING;
    else if (string_starts_with(name, "Feit Bulb"))
        existing->category = CATEGORY_LIGHTING;
    else if (string_starts_with(name, "Triones"))
        existing->category = CATEGORY_LIGHTING;
    else if (string_starts_with(name, "Evluma"))
        existing->category = CATEGORY_LIGHTING; // Evluma AMAX lighting controller
    else if (string_starts_with(name, "ihoment_H"))
        existing->category = CATEGORY_LIGHTING; // LED light strip controller
    else if (string_starts_with(name, "LEDBlue"))
        existing->category = CATEGORY_LIGHTING; // RGB LED controller
    else if (string_starts_with(name, "ELK-BLEDOM"))
        existing->category = CATEGORY_LIGHTING; // RGB LED controller
    else if (string_starts_with(name, "SP110E"))
        existing->category = CATEGORY_LIGHTING; // RGB LED controller

    // APPLIANCE

    else if (string_starts_with(name, "EssenzaTwo"))
        existing->category = CATEGORY_APPLIANCE;    // Nespresso coffee machine, not a Lamborghini supercar
    else if (string_starts_with(name, "BEDJET"))
        existing->category = CATEGORY_APPLIANCE;    // Bed temperature controller!
    else if (string_starts_with(name, "[Refrigerator] Samsung"))
        existing->category = CATEGORY_APPLIANCE;    // Fridge!
    else if (string_starts_with(name, "Levolor"))
        existing->category = CATEGORY_APPLIANCE;    // Shades
    else if (string_starts_with(name, "RIDGID Battery"))
        existing->category = CATEGORY_APPLIANCE;    // Batteries!


    // TVs
    else if (strcmp(name, "AppleTV") == 0)
        existing->category = CATEGORY_TV;
    else if (strcmp(name, "Apple TV") == 0)
        existing->category = CATEGORY_TV;
    else if (string_starts_with(name, "HiSmart"))       // HiSmart[TV] 2k ...
        existing->category = CATEGORY_TV;
    else if (string_starts_with(name, "XBR-"))          // Sony TV 
        existing->category = CATEGORY_TV;
    else if (string_starts_with(name, "BRAVIA"))        // Sony Bravia TVs, VU1
        existing->category = CATEGORY_TV;
    else if (string_starts_with(name, "AT&T TV"))       // AT&T TV
        existing->category = CATEGORY_TV;
    else if (string_ends_with(name, "TV"))
        existing->category = CATEGORY_TV;

    // Beacons
    else if (string_starts_with(name, "AprilBeacon"))
        existing->category = CATEGORY_BEACON;
    else if (string_starts_with(name, "abtemp"))
        existing->category = CATEGORY_BEACON;
    else if (string_starts_with(name, "abeacon"))
        existing->category = CATEGORY_BEACON;
    else if (string_starts_with(name, "estimote"))
        existing->category = CATEGORY_BEACON;
    else if (string_starts_with(name, "Tile"))
        existing->category = CATEGORY_BEACON;
    else if (string_starts_with(name, "iTAG"))
        existing->category = CATEGORY_BEACON;
    else if (string_starts_with(name, "LYWSD03MMC"))
        existing->category = CATEGORY_BEACON;
    else if (string_ends_with(name, "beacon"))
        existing->category = CATEGORY_BEACON;
    else if (string_ends_with(name, "Beacon"))
        existing->category = CATEGORY_BEACON;

    // Headphones

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
    else if (string_starts_with(name, "Sparkle Motion"))
        existing->category = CATEGORY_HEADPHONES;
    else if (string_contains_insensitive(name, "headphone"))
        existing->category = CATEGORY_HEADPHONES;

    // Hearing aids - todo: separate category

    else if (string_contains_insensitive(name, "Thomas Hearing Aids"))
        existing->category = CATEGORY_HEADPHONES;


    // SPEAKERS

    else if (string_starts_with(name, "mini lifejacket jolt"))
        existing->category = CATEGORY_SPEAKERS;
    else if (string_starts_with(name, "DSW229Dynamo 1100X"))
        existing->category = CATEGORY_SPEAKERS;   // Subwoofer
    else if (string_starts_with(name, "DSW227Dynamo 600X"))
        existing->category = CATEGORY_SPEAKERS;   // Subwoofer
    else if (string_starts_with(name, "LHB-"))
        existing->category = CATEGORY_SPEAKERS;   // LG Soundbar?
    else if (string_starts_with(name, "HTC BS"))
        existing->category = CATEGORY_SPEAKERS;   // Conference speaker
    else if (string_ends_with(name, "speaker"))
        existing->category = CATEGORY_SPEAKERS;   // e.g. Master Bedroom speaker
    else if (string_ends_with(name, "speak"))
        existing->category = CATEGORY_SPEAKERS;   // e.g. Master Bedroom speaker

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
    else if (string_starts_with(name, "iHome iBT751"))
        existing->category = CATEGORY_HEADPHONES; // iHome iBT751 speakers and lighting

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
    else if (string_starts_with(name, "Chromecast"))
        existing->category = CATEGORY_TV;

    // Receivers
    else if (string_starts_with(name, "YamahaAV"))
        existing->category = CATEGORY_FIXED; // receiver

    // Printers
    else if (string_starts_with(name, "ENVY Photo"))
        existing->category = CATEGORY_PRINTER; // printer
    else if (string_starts_with(name, "Sony UP-DX"))
        existing->category = CATEGORY_PRINTER; // printer Sony UP-DX100
    else if (string_starts_with(name, "TS9500"))
        existing->category = CATEGORY_PRINTER;
    else if (string_starts_with(name, "TR8500"))
        existing->category = CATEGORY_PRINTER;

    // Accessories
    else if (string_starts_with(name, "Apple Pencil"))
        existing->category = CATEGORY_WEARABLE;  // Not really wearable
    else if (string_starts_with(name, "SPEN 02"))
        existing->category = CATEGORY_FIXED;  // Not really fixed WACOM
    else if (string_starts_with(name, "Oculus"))
        existing->category = CATEGORY_FIXED; //Oculus Quest etc.

    // POS

    else if (string_starts_with(name, "eDynamo"))
        existing->category = CATEGORY_POS; // Credit card reader
    else if (string_starts_with(name, "Square Reader"))
        existing->category = CATEGORY_POS; // Credit card reader
    else if (string_starts_with(name, "Self Checkout"))
        existing->category = CATEGORY_POS; // Self-checkout terminal
    else if (string_starts_with(name, "TJQLJ"))
        existing->category = CATEGORY_POS; // Checkout? or beacon in TJMAX store
    else if (string_starts_with(name, "RS507 "))
        existing->category = CATEGORY_POS; // Ring barcode scanner
    else if (string_starts_with(name, "Tap & Chip"))
        existing->category = CATEGORY_POS;  // Shopify tap & chip reader
    else if (string_starts_with(name, "*MOB85"))
        existing->category = CATEGORY_POS;  // POS
    else if (string_starts_with(name, "PayRange"))
        existing->category = CATEGORY_POS;  // POS
    else if (string_starts_with(name, "Bluesnap"))
        existing->category = CATEGORY_POS;  // POS
    else if (string_starts_with(name, "IBM"))
        existing->category = CATEGORY_POS;  // POS
    else if (string_starts_with(name, "NWTR040"))
        existing->category = CATEGORY_POS;  // POS
    else if (string_starts_with(name, "XXZKJ"))
        existing->category = CATEGORY_POS;  // Taiyo Yuden module Beacon? Other?
    else if (string_starts_with(name, "IDTECH-VP3300"))
        existing->category = CATEGORY_POS;

    // Cars

    else if (string_starts_with(name, "Audi"))
        existing->category = CATEGORY_CAR;
    else if (string_starts_with(name, "CalAmp BT"))     // lojack etc.
        existing->category = CATEGORY_CAR;
    else if (string_starts_with(name, "DEI-"))     // remote starter
        existing->category = CATEGORY_CAR;
    else if (string_starts_with(name, "Nextbase622GW"))     // dash cam
        existing->category = CATEGORY_CAR;
    else if (string_starts_with(name, "Aura Pro"))     // LED lights for cars
        existing->category = CATEGORY_CAR;
    else if (string_starts_with(name, "VW "))
        existing->category = CATEGORY_CAR;
    else if (strncmp(name, "BMW", 3) == 0)
        existing->category = CATEGORY_CAR;
    else if (string_starts_with(name, "GM_PEPS_"))
        existing->category = CATEGORY_CAR; // maybe the key fob
    else if (strncmp(name, "Subaru", 6) == 0)
        existing->category = CATEGORY_CAR;
    else if (string_starts_with(name, "DV7100"))
        existing->category = CATEGORY_CAR;      // aftermarket headunit
    else if (string_starts_with(name, "nuvi"))
        existing->category = CATEGORY_CAR;
    else if (string_starts_with(name, "UberBeacon"))
        existing->category = CATEGORY_CAR; // Uber's dashboard display
    else if (string_starts_with(name, "Lyft Amp"))
        existing->category = CATEGORY_CAR; // Lyft's dashboard display
    else if (string_starts_with(name, "VHMLite"))
        existing->category = CATEGORY_CAR; // OBD-II monitor
    else if (string_starts_with(name, "VEEPEAK"))
        existing->category = CATEGORY_CAR; // OBD-II monitor
    else if (string_starts_with(name, "LMU3030_BT"))
        existing->category = CATEGORY_CAR; // OBD-II monitor
    else if (string_starts_with(name, "Scosche BTFM4"))
        existing->category = CATEGORY_CAR; // Handsfree car kit
    else if (string_starts_with(name, "BLE_Garmin Driv"))
        existing->category = CATEGORY_CAR; // Garmin Nav
    else if (string_starts_with(name, "MAX 360c"))
        existing->category = CATEGORY_CAR; // radar detector
    else if (strncmp(name, "Land Rover", 10) == 0)
        existing->category = CATEGORY_CAR;
    // TODO: More Android device names
}