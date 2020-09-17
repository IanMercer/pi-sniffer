// manufacturer heuristics

#include "device.h"
#include "utility.h"
#include "heuristics.h"
#include <stdio.h>

/*
     handle the manufacturer data
*/
void handle_manufacturer(struct Device *existing, uint16_t manufacturer, unsigned char *allocdata)
{
    if (manufacturer == 0x004c)
    { 
        handle_apple(existing, allocdata);
    }
    else if (manufacturer == 0x022b)
    {
        optional_set(existing->name, "_Tesla", NAME_LENGTH);
        existing->category = CATEGORY_CAR;
        //    ManufacturerData: {uint16 555: <[byte 0x04, 0x18, 0x77, 0x9d, 0x16, 0xee, 0x04, 0x6c, 0xf9, 0x49, 0x01, 0xf3, 0
    }
    else if (manufacturer == 0x0006)
    {
        optional_set(existing->name, "_Microsoft", NAME_LENGTH);
        existing->category = CATEGORY_COMPUTER; // maybe?
    }
    else if (manufacturer == 0x0087)
    {
        optional_set(existing->name, "_Garmin", NAME_LENGTH);
        existing->category = CATEGORY_WEARABLE; // could be fitness tracker
    }
    else if (manufacturer == 0x0141)
    {
        optional_set(existing->name, "Fedex", NAME_LENGTH);
        existing->category = CATEGORY_CAR;   // Fedex delivery? 
    }
    else if (manufacturer == 0x05A7)
    {
        optional_set(existing->name, "_Sonos", NAME_LENGTH);
        existing->category = CATEGORY_FIXED;
    }
    else if (manufacturer == 0xb4c1)
    {
        optional_set(existing->name, "_Dycoo", NAME_LENGTH); // not on official Bluetooth website??
    }
    else if (manufacturer == 0x0101)
    {
        optional_set(existing->name, "_Fugoo, Inc.", NAME_LENGTH);
        existing->category = CATEGORY_HEADPHONES;
    }
    else if (manufacturer == 0x0310)
    {
        optional_set(existing->name, "_SGL Italia", NAME_LENGTH);
        existing->category = CATEGORY_HEADPHONES;
    }
    else if (manufacturer == 0x3042)
    { // 12354 = someone didn't register
        optional_set(existing->name, "_Manuf 0x3042", NAME_LENGTH);
        existing->category = CATEGORY_HEADPHONES;
    }
    else if (manufacturer == 0x0075)
    {
        optional_set(existing->name, "_Samsung 0x75", NAME_LENGTH);
    }
    else if (manufacturer == 0xff19)
    {
        optional_set(existing->name, "_Samsung 0xff19", NAME_LENGTH);
    }
    else if (manufacturer == 0x0131)
    {
        optional_set(existing->name, "_Cypress Semi", NAME_LENGTH);
    }
    else if (manufacturer == 0x0110)
    {
        optional_set(existing->name, "_Nippon Seiki", NAME_LENGTH);
    }
    else if (manufacturer == 0x0399)
    {
        optional_set(existing->name, "_Nikon", NAME_LENGTH);
        existing->category = CATEGORY_FIXED;
    }
    else if (manufacturer == 0x0003)
    {
        optional_set(existing->name, "_IBM", NAME_LENGTH);
        existing->category = CATEGORY_FIXED;
    }
    else if (manufacturer == 0x0501)
    {
        optional_set(existing->name, "_Polaris ND", NAME_LENGTH);
        existing->category = CATEGORY_FIXED;
    }
    else if (manufacturer == 0x0649)
    {
        optional_set(existing->name, "_Ryeex", NAME_LENGTH);
        // Makes fitness bands
        existing->category = CATEGORY_WEARABLE;
    }
    else if (manufacturer == 0x014f)
    {
        optional_set(existing->name, "_B&W Group Ltd.", NAME_LENGTH);
        existing->category = CATEGORY_FIXED;
    }
    else if (manufacturer == 0x00c4)
    {
        optional_set(existing->name, "_LG Electronics", NAME_LENGTH);
        existing->category = CATEGORY_TV;  // maybe, they did make phones for a while
    }
    else if (manufacturer == 0x03ee)
    {
        optional_set(existing->name, "_CUBE Technolgies", NAME_LENGTH);
        existing->category = CATEGORY_FIXED;
    }
    else if (manufacturer == 0x00e0)
    {
        optional_set(existing->name, "_Google", NAME_LENGTH);
    }
    else if (manufacturer == 0x0085)
    {
        optional_set(existing->name, "_BlueRadios ODM", NAME_LENGTH);
    }
    else if (manufacturer == 0x0434)
    {
        optional_set(existing->name, "_Hatch Baby, Inc.", NAME_LENGTH);
        existing->category = CATEGORY_FIXED;
    }
    else if (manufacturer == 0x0157)
    {
        optional_set(existing->name, "_Anhui Huami", NAME_LENGTH);
        existing->category = CATEGORY_FIXED;
    }
    else if (manufacturer == 0x001d)
    {
        optional_set(existing->name, "_Qualcomm", NAME_LENGTH);
    }
    else if (manufacturer == 0x015e)
    {
        optional_set(existing->name, "_Unikey Technologies", NAME_LENGTH);
        existing->category = CATEGORY_FIXED;
    }
    else if (manufacturer == 0x01a5)
    {
        optional_set(existing->name, "_Icon Health and Fitness", NAME_LENGTH);
        existing->category = CATEGORY_FIXED;
    }
    else if (manufacturer == 0x0401)
    {
        optional_set(existing->name, "_Relations Inc", NAME_LENGTH);
        existing->category = CATEGORY_FIXED;
    }
    else if (manufacturer == 0x0065)
    {
        optional_set(existing->name, "_HP", NAME_LENGTH);
        existing->category = CATEGORY_FIXED;
    }
    else if (manufacturer == 0x00d2)
    {
        optional_set(existing->name, "_AbTemp", NAME_LENGTH);
        g_debug("  Ignoring manufdata");
    }
    else
    {
        // https://www.bluetooth.com/specifications/assigned-numbers/16-bit-uuids-for-members/
        char manuf[32];
        snprintf(manuf, sizeof(manuf), "_Manufacturer 0x%04x", manufacturer);
        g_info("  Did not recognize %s", manuf);
        optional_set(existing->name, manuf, NAME_LENGTH);
    }
}