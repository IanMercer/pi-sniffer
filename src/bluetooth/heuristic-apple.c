// Apple heuristics

#include "device.h"
#include "utility.h"
#include "mqtt_send.h"

void handle_apple(struct Device *existing, unsigned char *allocdata)
{
    // Apple
    uint8_t apple_device_type = allocdata[00];
    if (apple_device_type == 0x01)
    {
        // An iMac causes this
        g_info("  %s '%s' Apple Device type 0x01 - what is this?", existing->mac, existing->name);

    }
    else if (apple_device_type == 0x02)
    {
        optional_set(existing->name, "_Beacon", NAME_LENGTH);
        if (existing->category != CATEGORY_BEACON)
        {
            g_info("  %s '%s' Beacon", existing->mac, existing->name);
            existing->category = CATEGORY_BEACON;
        }

        // Aprilbeacon temperature sensor sends temperature in manufacturer data
        if (strncmp(existing->name, "abtemp", 6) == 0)
        {
            //uint8_t temperature = allocdata[21];
            //send_to_mqtt_single_value(existing->mac, "temperature", temperature);
        }
    }
    else if (apple_device_type == 0x03)     // On user action
    {
        optional_set(existing->name, "_Airprint", NAME_LENGTH);
        g_info("  %s '%s' Airprint", existing->mac, existing->name);
    }
    else if (apple_device_type == 0x05)     // On user action
    {
        optional_set(existing->name, "_Airdrop", NAME_LENGTH);
        g_info("  %s '%s' Airdrop", existing->mac, existing->name);
    }
    else if (apple_device_type == 0x06)     // Constantly
    {
        optional_set(existing->name, "_Homekit", NAME_LENGTH);
        g_info("  %s '%s' Homekit", existing->mac, existing->name);
        // 1 byte adv internal length
        // 1 byte status flags
        // 6 bytes device id
        // 2 bytes category
        // 2 bytes global state number
    }
    else if (apple_device_type == 0x07)     // Proximity Pairing - Constantly (rare)
    {
        optional_set(existing->name, "_Airpods", NAME_LENGTH);
        g_info("  %s '%s' Proximity Pairing", existing->mac, existing->name);
        existing->category = CATEGORY_HEADPHONES;
        // 1 byte length
        // 1 byte status flags
        // 0x01
        // 2 bytes device model
        // 1 byte UTP
        // 2 bytes battery level and charging state
    }
    else if (apple_device_type == 0x08)     // On user action (rare)
    {
        optional_set(existing->name, "_Siri", NAME_LENGTH);
        g_info("  %s '%s' Siri", existing->mac, existing->name);
        // 1 byte length
        // 2 bytes perceptual hash
        // 1 byte SNR
        // 1 byte confidence
        // 2 bytes device class
        // 1 byte random
    }
    else if (apple_device_type == 0x09)     // On user action (some)
    {
        optional_set(existing->name, "_Airplay", NAME_LENGTH);
        g_info("  %s '%s' Airplay", existing->mac, existing->name);
        existing->category = CATEGORY_FIXED;  // probably an Apple TV?
        // 1 byte length
        // 1 byte flags
        // 1 byte config seed
        // 4 bytes IPv4 address
    }
    else if (apple_device_type == 0x0a)     // ?? (rare)
    {
        optional_set(existing->name, "_Apple 0a", NAME_LENGTH);
        g_info("  %s '%s' Apple 0a??", existing->mac, existing->name);
    }
    else if (apple_device_type == 0x0b)     // On physical action
    {
        // Confirmed seen from iWatch
        // Sent when watch has lost pairing to phone?
        optional_set(existing->name, "iWatch", NAME_LENGTH);
        g_info("  %s '%s' Magic Switch", existing->mac, existing->name);
        soft_set_category(&existing->category, CATEGORY_WATCH);
    }
    else if (apple_device_type == 0x0c)     // Handoff
    {
        optional_set(existing->name, "_Apple Phone", NAME_LENGTH);
        g_info("  %s '%s' Handoff", existing->mac, existing->name);
        soft_set_category(&existing->category, CATEGORY_PHONE);  // might be an iPad? but assume phone
        // 1 byte length
        // 1 byte version
        // 2 bytes IV
        // 1 byte AES-GCM Auth tag
        // 16 bytes encrypted payload
    }
    else if (apple_device_type == 0x0d)     // Instant hotspot - On user action
    {
        optional_set(existing->name, "_Apple WifiSet", NAME_LENGTH);
        g_info("  %s '%s' WifiSet", existing->mac, existing->name);
        soft_set_category(&existing->category, CATEGORY_PHONE);  // might be an iPad? but assume phone
    }
    else if (apple_device_type == 0x0e)     // Instant hotspot - Reaction to target presence
    {
        optional_set(existing->name, "_Apple Hotspot", NAME_LENGTH);
        g_info("  %s '%s' Hotspot", existing->mac, existing->name);
        soft_set_category(&existing->category, CATEGORY_PHONE);  // might be an iPad? but assume phone
    } 
    else if (apple_device_type == 0x0f)     // Nearby action - On user action (rare)
    {
        optional_set(existing->name, "_Apple Nearby Action", NAME_LENGTH);
        g_info("  %s '%s' Nearby Action", existing->mac, existing->name);
        soft_set_category(&existing->category, CATEGORY_PHONE);  // might be an iPad? but assume phone
        // Used for WiFi-password messages
        // 1 byte length
        // 1 byte action flags
        // 1 byte action type
        // 3 bytes auth tag
        // 4 x 3 bytes action parameters
    }
    else if (apple_device_type == 0x10)     // Nearby Info - Constantly
    {
        optional_set(existing->name, "_Apple", NAME_LENGTH);
        //g_debug("  Nearby Info ");
        // 0x10
        // 1 byte length
        // 1 byte activity level
        // 1 byte information
        // 3 bytes auth tag

        // e.g. phone: <[byte 0x10, 0x06, 0x51, 0x1e, 0xc1, 0x36, 0x99, 0xe1]>}

        // too soon ... name comes later ... optional(existing->name, "Apple Device");
        // Not right, MacBook Pro seems to send this too

        uint8_t lower_bits = allocdata[02] & 0x0f;
        uint8_t upper_bits = allocdata[02] >> 4;
        uint8_t information_byte = allocdata[03];

        char* wifi = 
            information_byte == 0x10 ? "iPhone6?" : 
            information_byte == 0x18 ? "Wifi ON ()" : 
            information_byte == 0x1c ? "Wifi ON ()" : 
            information_byte == 0x1e ? "Wifi ON ()" : 
            information_byte == 0x1a ? "Wifi OFF()" : " ";

        if (lower_bits == 0x00){
            // iPad sends this, unused
            g_info("  %s '%s' Nearby Info 0x00: unknown u=%.2x info=%.2x %s", existing->mac, existing->name, upper_bits, information_byte, wifi);
        }
        else if (lower_bits == 0x01){
            g_info("  %s '%s' Nearby Info 0x01: disabled u=%.2x info=%.2x %s", existing->mac, existing->name, upper_bits, information_byte, wifi);
            // Apple TV sends this too, so cannot assume phone here '_Apple' Nearby Info 0x01: disabled u=00 info=00
            //soft_set_category(&existing->category, CATEGORY_PHONE);  // most likely category
        }
        else if (lower_bits == 0x02){
            g_info("  %s '%s' Nearby Info 0x02: unknown? u=%.2x info=%.2x %s", existing->mac, existing->name, upper_bits, information_byte, wifi);
        }
        else if (lower_bits == 0x03){
            // locked screen
            g_info("  %s '%s' Nearby Info 0x03: locked u=%.2x info=%.2x %s", existing->mac, existing->name, upper_bits, information_byte, wifi);
        }
        else if (lower_bits == 0x04){
            g_info("  %s '%s' Nearby Info 0x04: unknown? u=%.2x info=%.2x %s", existing->mac, existing->name, upper_bits, information_byte, wifi);
        }
        else if (lower_bits == 0x05){
            g_info("  %s '%s' Nearby Info 0x05: audio playing, screen off u=%.2x info=%.2x %s", existing->mac, existing->name, upper_bits, information_byte, wifi);
        }
        else if (lower_bits == 0x06){
            // iWatch sends this
            g_info("  %s '%s' Nearby Info 0x06: unknown? watch? u=%.2x info=%.2x %s", existing->mac, existing->name, upper_bits, information_byte, wifi);
        }
        else if (lower_bits == 0x07){
            // transition phase
            g_info("  %s '%s' Nearby Info 0x07: on lock screen? u=%.2x info=%.2x %s", existing->mac, existing->name, upper_bits, information_byte, wifi);
        }
        else if (lower_bits == 0x08){
            // iPhoneX 
            g_info("  %s '%s' Nearby Info 0x08: screen is on u=%.2x info=%.2x %s", existing->mac, existing->name, upper_bits, information_byte, wifi);
        }
        else if (lower_bits == 0x09){
            // Nope, iPad is locked
            // Nope, iPhone X, video was not playing, u=07
            // Nope, iPhone 8, video not playing, u=03, info=1a, wifi was on
            g_info("  %s '%s' Nearby Info 0x09: screen is on u=%.2x info=%.2x %s", existing->mac, existing->name, upper_bits, information_byte, wifi);
        }
        else if (lower_bits == 0x0A){
            // Elsewhere it says this is a message from phone to watch?
            // This message is sent by phones not watches, seems to have nothing to do with them
            // iPhone X
            // See https://arxiv.org/pdf/1904.10600.pdf
            soft_set_category(&existing->category, CATEGORY_PHONE);  // might be an iPad?
            g_info("  %s '%s' Nearby Info 0x0a: iPhone u=%.2x info=%.2x %s", existing->mac, existing->name, upper_bits, information_byte, wifi);
        }
        else if (lower_bits == 0x0B){
            // active user
            soft_set_category(&existing->category, CATEGORY_PHONE);  // might be an iPad?
            g_info("  %s '%s' Nearby Info 0x0b: Recent user interaction u=%.2x info=%.2x %s", existing->mac, existing->name, upper_bits, information_byte, wifi);
        }
        else if (lower_bits == 0x0C){
            g_info("  %s '%s' Nearby Info 0x0c: ???? %.2x info=%.2x %s", existing->mac, existing->name, upper_bits, information_byte, wifi);
        }
        else if (lower_bits == 0x0D){
            g_info("  %s '%s' Nearby Info 0x0d: ???? %.2x info=%.2x %s", existing->mac, existing->name, upper_bits, information_byte, wifi);
        }
        else if (lower_bits == 0x0E){
            g_info("  %s '%s' Nearby Info 0x0e: Phone call or Facetime %.2x info=%.2x %s", existing->mac, existing->name, upper_bits, information_byte, wifi);
        }
        else if (lower_bits == 0x0F){
            g_info("  %s '%s' Nearby Info 0x0f: unknown? watch? u=%.2x info=%.2x %s", existing->mac, existing->name, upper_bits, information_byte, wifi);
        }
        else
            g_info("  %s '%s' Nearby Info 0x%2x: Unknown device status upper=%2x info=%.2x %s", existing->mac, existing->name, lower_bits, upper_bits, information_byte, wifi);
    }
    else
    {
        g_info("  %s '%s' Did not recognize apple device type %.2x", existing->mac, existing->name, apple_device_type);
    }
}
