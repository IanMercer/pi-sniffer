// name heuristics - may be able to determin category from name

#include <stdio.h>
#include "device.h"
#include "utility.h"
#include "heuristics.h"


/*
*  Sanitize name no matter what source it came from - another Pi or Bluetooth or a file
*/
void sanitize_and_assign_name(const char* name, struct Device* device, char* needle)
{
    if (device->name_type < nt_alias)  // beacon names don't get hashed
    {
        if (string_contains_insensitive(name, needle))
        {
            char* remainder = g_strstr_len(name, strlen(name), needle);
            if (remainder != name)  // If whole string matches don't hash it
            {
                // name could be longer than NAME_LENGTH at this point, maybe someone has a very long name
                uint32_t hash = hash_string(name, NAME_LENGTH);
                snprintf(device->name, NAME_LENGTH, "%c%c%4x %s", name[0], name[1], hash & 0xffff, remainder);
            }
        }
    }
}


void apply_name_heuristics (struct Device* device)
{
    const char* name = device -> name;
    if (string_contains_insensitive(name, "iPhone"))
    {
        sanitize_and_assign_name(name, device, "iPhone");
        device->category = CATEGORY_PHONE;
    }
    else if (string_ends_with(name, "phone"))  // some people name them
    {
        sanitize_and_assign_name(name, device, "phone");
    }
    else if (string_contains_insensitive(name, "Galaxy Note"))
        device->category = CATEGORY_PHONE;
    else if (string_contains_insensitive(name, "Galaxy Tab"))
        device->category = CATEGORY_TABLET;
    else if (string_contains_insensitive(name, "Galaxy A"))  //10 20 51
        device->category = CATEGORY_PHONE;
    else if (string_contains_insensitive(name, "Galaxy S"))  //9+
        device->category = CATEGORY_PHONE;
    else if (string_contains_insensitive(name, "Galaxy"))
        device->category = CATEGORY_PHONE;
    else if (string_contains_insensitive(name, "iPad"))
    {
        device->category = CATEGORY_TABLET;
    }
    // Privacy
    else if (string_contains_insensitive(name, "s Mac"))  // MacBook, Mac Pro, Mac Book, ..
    {
        sanitize_and_assign_name(name, device, "Mac");
        device->category = CATEGORY_COMPUTER;
    }
    else if (string_contains_insensitive(name, "MacBook"))
    {
        sanitize_and_assign_name(name, device, "Mac");
        device->category = CATEGORY_COMPUTER;
    }
    else if (string_starts_with(name, "BOOTCAMP"))
        device->category = CATEGORY_COMPUTER;

    // Watches
    else if (string_starts_with(name, "iWatch"))
        device->category = CATEGORY_WATCH;
    else if (string_starts_with(name, "Apple Watch"))
        device->category = CATEGORY_WATCH;
    else if (string_starts_with(name, "Galaxy Watch"))
        device->category = CATEGORY_WATCH;
    else if (string_starts_with(name, "Spartan Trainer"))
        device->category = CATEGORY_WATCH; // Suunto
    else if (string_starts_with(name, "Gear S3"))
        device->category = CATEGORY_WATCH;
    else if (string_starts_with(name, "Approach S20"))
        device->category = CATEGORY_WATCH; // Garmin Golf Watch
    else if (string_starts_with(name, "fenix"))
        device->category = CATEGORY_WATCH;
    //else if (string_equals(name, "Hum")) // toothbrush
    //    existing->category = CATEGORY_FIXED;

    else if (string_starts_with(name, "Galaxy Fit"))
        device->category = CATEGORY_FITNESS;
    else if (string_starts_with(name, "Ionic"))
        device->category = CATEGORY_FITNESS; // FITBIT
    else if (string_starts_with(name, "Zip"))
        device->category = CATEGORY_FITNESS; // FITBIT Zip
    else if (string_starts_with(name, "Versa"))
        device->category = CATEGORY_FITNESS; // FITBIT
    else if (string_starts_with(name, "Charge "))  //  Charge 2, 3, 4
        device->category = CATEGORY_FITNESS; // FITBIT
    else if (string_starts_with(name, "Inspire HR"))
        device->category = CATEGORY_FITNESS; // FITBIT
    else if (string_starts_with(name, "Mi Smart Band"))
        device->category = CATEGORY_FITNESS; // Fitness
    else if (string_starts_with(name, "TICKR X"))
        device->category = CATEGORY_FITNESS; // Heartrate
    else if (string_starts_with(name, "ID115Plus HR"))
        device->category = CATEGORY_FITNESS; // Heartrate
    else if (string_starts_with(name, "ID128Color HM"))
        device->category = CATEGORY_FITNESS; // ID128Color HM fitness band
    else if (string_starts_with(name, "HR-BT"))
        device->category = CATEGORY_FITNESS; // Heartrate
    else if (string_starts_with(name, "WHOOP"))
        device->category = CATEGORY_FITNESS; // Heartrate
    else if (string_starts_with(name, "Alta HR"))
        device->category = CATEGORY_FITNESS; // Fitbit Alta Heartrate
    else if (string_starts_with(name, "Dexcom6B"))
        device->category = CATEGORY_HEALTH; // Diabetic monitor

    else if (strncmp(name, "LumosHelmet", 11) == 0)  // Bike Helmet
        device->category = CATEGORY_WEARABLE;

    // FIXED
    else if (string_starts_with(name, "Tacx Neo 2T"))
        device->category = CATEGORY_FITNESS; // Bike trainer
    else if (string_starts_with(name, "SCHWINN 170/270"))
        device->category = CATEGORY_FITNESS; // Bike trainer
    else if (string_starts_with(name, "HT SANA"))
        device->category = CATEGORY_FITNESS; // Massage chair
    else if (string_starts_with(name, "MOLEKULE"))
        device->category = CATEGORY_APPLIANCE; // Air filter
    else if (string_starts_with(name, "iFlex_"))
        device->category = CATEGORY_APPLIANCE; // Fluke power meter?


    else if (string_starts_with(name, "Nest Cam"))
        device->category = CATEGORY_SECURITY; // Camera
    else if (string_starts_with(name, "Seos"))
        device->category = CATEGORY_SECURITY; // Credential technology
    else if (string_starts_with(name, "SCHLAGE"))
        device->category = CATEGORY_SECURITY; // Smart door lock?
    else if (string_starts_with(name, "WYZE"))
        device->category = CATEGORY_SECURITY; // Smart door lock?
    else if (string_starts_with(name, "Kuna"))
        device->category = CATEGORY_SECURITY;
    else if (string_starts_with(name, "Dropcam"))
        device->category = CATEGORY_SECURITY;

    // Category

    else if (string_starts_with(name, "D3400"))
        device->category = CATEGORY_CAMERA; // Nikon


    // Spriklers

    else if (string_starts_with(name, "bhyve"))
        device->category = CATEGORY_SPRINKLERS; // Sprinkler controller

    // LIGHTING

    else if (string_starts_with(name, "Hue Lamp"))
        device->category = CATEGORY_LIGHTING;
    else if (string_starts_with(name, "Feit Bulb"))
        device->category = CATEGORY_LIGHTING;
    else if (string_starts_with(name, "Triones"))
        device->category = CATEGORY_LIGHTING;
    else if (string_starts_with(name, "Evluma"))
        device->category = CATEGORY_LIGHTING; // Evluma AMAX lighting controller
    else if (string_starts_with(name, "ihoment_H"))
        device->category = CATEGORY_LIGHTING; // LED light strip controller
    else if (string_starts_with(name, "LEDBlue"))
        device->category = CATEGORY_LIGHTING; // RGB LED controller
    else if (string_starts_with(name, "ELK-BLEDOM"))
        device->category = CATEGORY_LIGHTING; // RGB LED controller
    else if (string_starts_with(name, "SP110E"))
        device->category = CATEGORY_LIGHTING; // RGB LED controller

    // APPLIANCE

    else if (string_starts_with(name, "EssenzaTwo"))
        device->category = CATEGORY_APPLIANCE;    // Nespresso coffee machine, not a Lamborghini supercar
    else if (string_starts_with(name, "BEDJET"))
        device->category = CATEGORY_APPLIANCE;    // Bed temperature controller!
    else if (string_starts_with(name, "[Refrigerator] Samsung"))
        device->category = CATEGORY_APPLIANCE;    // Fridge!
    else if (string_starts_with(name, "Levolor"))
        device->category = CATEGORY_APPLIANCE;    // Shades
    else if (string_starts_with(name, "RIDGID Battery"))
        device->category = CATEGORY_APPLIANCE;    // Batteries!


    // TVs
    else if (strcmp(name, "AppleTV") == 0)
        device->category = CATEGORY_TV;
    else if (strcmp(name, "Apple TV") == 0)
        device->category = CATEGORY_TV;
    else if (string_starts_with(name, "HiSmart"))       // HiSmart[TV] 2k ...
        device->category = CATEGORY_TV;
    else if (string_starts_with(name, "XBR-"))          // Sony TV 
        device->category = CATEGORY_TV;
    else if (string_starts_with(name, "BRAVIA"))        // Sony Bravia TVs, VU1
        device->category = CATEGORY_TV;
    else if (string_starts_with(name, "AT&T TV"))       // AT&T TV
        device->category = CATEGORY_TV;
    else if (string_ends_with(name, "TV"))
        device->category = CATEGORY_TV;

    // Beacons
    else if (string_starts_with(name, "AprilBeacon"))
        device->category = CATEGORY_BEACON;
    else if (string_starts_with(name, "abtemp"))
        device->category = CATEGORY_BEACON;
    else if (string_starts_with(name, "abeacon"))
        device->category = CATEGORY_BEACON;
    else if (string_starts_with(name, "estimote"))
        device->category = CATEGORY_BEACON;
    else if (string_starts_with(name, "Tile"))
        device->category = CATEGORY_BEACON;
    else if (string_starts_with(name, "iTAG"))
        device->category = CATEGORY_BEACON;
    else if (string_starts_with(name, "LYWSD03MMC"))
        device->category = CATEGORY_BEACON;
    else if (string_ends_with(name, "beacon"))
        device->category = CATEGORY_BEACON;
    else if (string_ends_with(name, "Beacon"))
        device->category = CATEGORY_BEACON;

    // Headphones

    else if (strncmp(name, "Sesh Evo-LE", 11) == 0)
        device->category = CATEGORY_HEADPHONES; // Skullcandy
    else if (strncmp(name, "F2", 2) == 0)
        device->category = CATEGORY_HEADPHONES; // Soundpal F2 spakers
    else if (strncmp(name, "Jabra", 5) == 0)
        device->category = CATEGORY_HEADPHONES;
    else if (string_starts_with(name, "LE-"))     // LE-Bose, LE-<name>Bose, LE-Reserved_N
        device->category = CATEGORY_HEADPHONES;
    else if (string_starts_with(name, "LE-reserved_C"))
        device->category = CATEGORY_HEADPHONES;
    else if (strncmp(name, "Blaze", 5) == 0)
        device->category = CATEGORY_HEADPHONES;
    else if (string_starts_with(name, "HarpBT"))
        device->category = CATEGORY_HEADPHONES;
    else if (string_starts_with(name, "JBL LIVE500BT"))
        device->category = CATEGORY_HEADPHONES;
    else if (string_starts_with(name, "LG HBS1120"))
        device->category = CATEGORY_HEADPHONES;
    else if (string_starts_with(name, "Z-Link"))
        device->category = CATEGORY_HEADPHONES;
    else if (string_starts_with(name, "LE_Stealth 700 Xbox"))
        device->category = CATEGORY_HEADPHONES;
    else if (string_starts_with(name, "LE_WH-H900N"))
        device->category = CATEGORY_HEADPHONES;  //LE_WH-H900N (h.ear)
    else if (string_starts_with(name, "Bose AE2 SoundLink"))
        device->category = CATEGORY_HEADPHONES;
    else if (string_starts_with(name, "Sparkle Motion"))
        device->category = CATEGORY_HEADPHONES;
    else if (string_contains_insensitive(name, "headphone"))
        device->category = CATEGORY_HEADPHONES;
    else if (string_contains_insensitive(name, "FreeBuds")) // HUAWEI FreeBuds 3i
        device->category = CATEGORY_HEADPHONES;

    // Hearing aids - todo: separate category
    else if (string_contains_insensitive(name, "Thomas Hearing Aids"))
        device->category = CATEGORY_HEADPHONES;


    // SPEAKERS

    else if (string_starts_with(name, "mini lifejacket jolt"))
        device->category = CATEGORY_SPEAKERS;
    else if (string_starts_with(name, "DSW229Dynamo 1100X"))
        device->category = CATEGORY_SPEAKERS;   // Subwoofer
    else if (string_starts_with(name, "DSW227Dynamo 600X"))
        device->category = CATEGORY_SPEAKERS;   // Subwoofer
    else if (string_starts_with(name, "LHB-"))
        device->category = CATEGORY_SPEAKERS;   // LG Soundbar?
    else if (string_starts_with(name, "HTC BS"))
        device->category = CATEGORY_SPEAKERS;   // Conference speaker
    else if (string_ends_with(name, "speaker"))
        device->category = CATEGORY_SPEAKERS;   // e.g. Master Bedroom speaker
    else if (string_ends_with(name, "speak"))
        device->category = CATEGORY_SPEAKERS;   // e.g. Master Bedroom speaker

    // BT Speakers
    else if (string_starts_with(name, "Venue-Tile"))
        device->category = CATEGORY_HEADPHONES;   // Skullcandy headphones with Tile // pub
    else if (string_starts_with(name, "Echo Dot"))
        device->category = CATEGORY_HEADPHONES;   // Alexa
    else if (string_starts_with(name, "SRS-XB12"))
        device->category = CATEGORY_HEADPHONES;   // BT Speakers
    else if (string_starts_with(name, "ACTON II"))
        device->category = CATEGORY_HEADPHONES;   // BT Speakers
    else if (string_starts_with(name, "VIZIO V51"))
        device->category = CATEGORY_HEADPHONES;   // Soundbar
    else if (string_starts_with(name, "VQ"))
        device->category = CATEGORY_HEADPHONES;   // Retroradio and speakers
    else if (string_starts_with(name, "iHome iBT751"))
        device->category = CATEGORY_HEADPHONES; // iHome iBT751 speakers and lighting

    // TVs
    // e.g. "[TV] Samsung Q70 Series (65)" icon is audio_card
    else if (string_starts_with(name, "[TV] "))
        device->category = CATEGORY_TV;
    // else if (string_starts_with(name, "[TV] Samsung"))
    //     existing->category = CATEGORY_TV;
    else if (string_starts_with(name, "[Signage] Samsung"))
        device->category = CATEGORY_TV;
    else if (string_starts_with(name, "[LG] webOS TV"))
        device->category = CATEGORY_TV;
    else if (string_starts_with(name, "SONY XBR"))
        device->category = CATEGORY_TV;
    else if (string_starts_with(name, "Chromecast"))
        device->category = CATEGORY_TV;

    // Receivers
    else if (string_starts_with(name, "YamahaAV"))
        device->category = CATEGORY_FIXED; // receiver

    // Printers
    else if (string_starts_with(name, "ENVY Photo"))
        device->category = CATEGORY_PRINTER; // printer
    else if (string_starts_with(name, "Sony UP-DX"))
        device->category = CATEGORY_PRINTER; // printer Sony UP-DX100
    else if (string_starts_with(name, "TS9500"))
        device->category = CATEGORY_PRINTER;
    else if (string_starts_with(name, "TR8500"))
        device->category = CATEGORY_PRINTER;

    // Accessories
    else if (string_starts_with(name, "Apple Pencil"))
        device->category = CATEGORY_PENCIL;
    else if (string_starts_with(name, "SPEN 02"))
        device->category = CATEGORY_ACCESSORY;  // Not really fixed WACOM
    else if (string_starts_with(name, "Oculus"))
        device->category = CATEGORY_ACCESSORY; //Oculus Quest etc.

    // POS

    else if (string_starts_with(name, "eDynamo"))
        device->category = CATEGORY_POS; // Credit card reader
    else if (string_starts_with(name, "Square Reader"))
        device->category = CATEGORY_POS; // Credit card reader
    else if (string_starts_with(name, "Self Checkout"))
        device->category = CATEGORY_POS; // Self-checkout terminal
    else if (string_starts_with(name, "TJQLJ"))
        device->category = CATEGORY_POS; // Checkout? or beacon in TJMAX store
    else if (string_starts_with(name, "RS507 "))
        device->category = CATEGORY_POS; // Ring barcode scanner
    else if (string_starts_with(name, "Tap & Chip"))
        device->category = CATEGORY_POS;  // Shopify tap & chip reader
    else if (string_starts_with(name, "*MOB85"))
        device->category = CATEGORY_POS;  // POS
    else if (string_starts_with(name, "PayRange"))
        device->category = CATEGORY_POS;  // POS
    else if (string_starts_with(name, "Bluesnap"))
        device->category = CATEGORY_POS;  // POS
    else if (string_starts_with(name, "IBM"))
        device->category = CATEGORY_POS;  // POS
    else if (string_starts_with(name, "NWTR040"))
        device->category = CATEGORY_POS;  // POS
    else if (string_starts_with(name, "XXZKJ"))
        device->category = CATEGORY_POS;  // Taiyo Yuden module Beacon? Other?
    else if (string_starts_with(name, "IDTECH-VP3300"))
        device->category = CATEGORY_POS;

    // Cars

    else if (string_starts_with(name, "Audi"))
        device->category = CATEGORY_CAR;
    else if (string_starts_with(name, "CalAmp BT"))     // lojack etc.
        device->category = CATEGORY_CAR;
    else if (string_starts_with(name, "DEI-"))     // remote starter
        device->category = CATEGORY_CAR;
    else if (string_starts_with(name, "Nextbase622GW"))     // dash cam
        device->category = CATEGORY_CAR;
    else if (string_starts_with(name, "Aura Pro"))     // LED lights for cars
        device->category = CATEGORY_CAR;
    else if (string_starts_with(name, "VW "))
        device->category = CATEGORY_CAR;
    else if (strncmp(name, "BMW", 3) == 0)
        device->category = CATEGORY_CAR;
    else if (string_starts_with(name, "GM_PEPS_"))
        device->category = CATEGORY_CAR; // maybe the key fob
    else if (strncmp(name, "Subaru", 6) == 0)
        device->category = CATEGORY_CAR;
    else if (string_starts_with(name, "DV7100"))
        device->category = CATEGORY_CAR;      // aftermarket headunit
    else if (string_starts_with(name, "nuvi"))
        device->category = CATEGORY_CAR;
    else if (string_starts_with(name, "UberBeacon"))
        device->category = CATEGORY_CAR; // Uber's dashboard display
    else if (string_starts_with(name, "Lyft Amp"))
        device->category = CATEGORY_CAR; // Lyft's dashboard display
    else if (string_starts_with(name, "VHMLite"))
        device->category = CATEGORY_CAR; // OBD-II monitor
    else if (string_starts_with(name, "VEEPEAK"))
        device->category = CATEGORY_CAR; // OBD-II monitor
    else if (string_starts_with(name, "LMU3030_BT"))
        device->category = CATEGORY_CAR; // OBD-II monitor
    else if (string_starts_with(name, "Scosche BTFM4"))
        device->category = CATEGORY_CAR; // Handsfree car kit
    else if (string_starts_with(name, "BLE_Garmin Driv"))
        device->category = CATEGORY_CAR; // Garmin Nav
    else if (string_starts_with(name, "MAX 360c"))
        device->category = CATEGORY_CAR; // radar detector
    else if (string_starts_with(name, "Interphone TOUR BLE"))
        device->category = CATEGORY_CAR; // motorbike intercom
    else if (string_starts_with(name, "FenSens"))
        device->category = CATEGORY_CAR; // backup camera
    else if (strncmp(name, "Land Rover", 10) == 0)
        device->category = CATEGORY_CAR;

    // TODO: More Android device names
}