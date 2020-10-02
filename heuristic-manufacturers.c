// manufacturer heuristics

#include "device.h"
#include "utility.h"
#include "heuristics.h"
#include <stdio.h>

typedef struct {
    const uint16_t manufacturer;
    const char* name;
} manufacturer_entry;

manufacturer_entry Disney = { .manufacturer = 0x0183, .name = "Disney" };


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
    else if (manufacturer == 0x0000)
    {
        optional_set(existing->name, "_Invalid 0x0000", NAME_LENGTH);
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
    else if (manufacturer == 0x0201)
    {
        optional_set(existing->name, "AR Timing", NAME_LENGTH);
        existing->category = CATEGORY_CAR;   // ???
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

        // Galaxy Watch
        // <[byte 0x01, 0x00, 0x02, 0x00, 0x01, 0x03, 0x02]>

        // <[byte 0x42, 0x04, 0x01, 0x01, 0x6e, 0x28, 0x39, 0x5e, 0x57, 0x84, 0xb3, 0x2a, 0x39, 0x5e, 0x57, 0x84, 0xb2, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00]>
        // <[byte 0x42, 0x04, 0x01, 0x01, 0x6e, 0x28, 0x39, 0x5e, 0x57, 0x84, 0xb3, 0x2a, 0x39, 0x5e, 0x57, 0x84, 0xb2, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00]>
        // <[byte 0x42, 0x04, 0x01, 0x01, 0x60, 0x00, 0xc3, 0xf4, 0x2d, 0xd5, 0x6e, 0x02, 0xc3, 0xf4, 0x2d, 0xd5, 0x6d, 0x01, 0x93, 0x00, 0x00, 0x00, 0x00, 0x00]>
        // <[byte 0x42, 0x04, 0x01, 0x20, 0x66, 0x19, 0x05, 0x00, 0x02, 0x01, 0x41, 0x01, 0x01, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00]>
        // <[byte 0x42, 0x04, 0x01, 0x01, 0x66, 0xb8, 0xbc, 0x5b, 0xfe, 0x21, 0xf7, 0xba, 0xbc, 0x5b, 0xfe, 0x21, 0xf6, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00]>

        // [TV] Samsung 8 Series (65)
        // <[byte 0x42, 0x04, 0x01, 0x20, 0x66, 0x19, 0x05, 0x00, 0x02, 0x01, 0x41, 0x01, 0x01, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00]>


    }

    else if (manufacturer == 0x9479)
    {
        optional_set(existing->name, "_Unlisted(0x9479)", NAME_LENGTH);
    }
    else if (manufacturer == 0x37bb)
    {
        optional_set(existing->name, "_Unlisted(0x37bb)", NAME_LENGTH);
    }
    else if (manufacturer == 0x0b01)
    {
        optional_set(existing->name, "_Unlisted(0x0b01)", NAME_LENGTH);
    }
    else if (manufacturer == 0x0a01)
    {
        optional_set(existing->name, "_Unlisted(0x0a01)", NAME_LENGTH);
    }
    else if (manufacturer == 0x484d)
    {
        optional_set(existing->name, "_Unlisted(0x484d)", NAME_LENGTH);
    }
    else if (manufacturer == 0x1010)
    {
        optional_set(existing->name, "_Unlisted(0x1010)", NAME_LENGTH);
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
        // Manufacturer of car clusters
        optional_set(existing->name, "_Nippon Seiki", NAME_LENGTH);
        existing->category = CATEGORY_CAR;
    }
    else if (manufacturer == 0x0399)
    {
        optional_set(existing->name, "_Nikon", NAME_LENGTH);
        existing->category = CATEGORY_FIXED;
    }
    else if (manufacturer == 0xc688)
    {
        optional_set(existing->name, "_Logitech", NAME_LENGTH);     // not listed on BT website
        existing->category = CATEGORY_FIXED;
    }
    else if (manufacturer == 0x0003)
    {
        optional_set(existing->name, "_IBM", NAME_LENGTH);
        existing->category = CATEGORY_FIXED;
    }
    else if (manufacturer == 0x0059)
    {
        optional_set(existing->name, "_Nordic Semiconductor", NAME_LENGTH);
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
        // Locks
        optional_set(existing->name, "_Unikey Technologies", NAME_LENGTH);
        existing->category = CATEGORY_FIXED;
    }
    else if (manufacturer == 0x0183)
    {
        optional_set(existing->name, "_Disney", NAME_LENGTH);
        existing->category = CATEGORY_WEARABLE;  // no idea!
    }
    else if (manufacturer == 0x01d1)
    {
        // Locks and home automation
        optional_set(existing->name, "_August Home", NAME_LENGTH);
        existing->category = CATEGORY_FIXED;
    }
    else if (manufacturer == 0x01a5)
    {
        optional_set(existing->name, "_Icon Health and Fitness", NAME_LENGTH);
        existing->category = CATEGORY_FIXED;
    }
    else if (manufacturer == 0x02ab)
    {
        optional_set(existing->name, "_BBPOS Limited", NAME_LENGTH);
        existing->category = CATEGORY_FIXED;
    }
    else if (manufacturer == 0x0401)
    {
        optional_set(existing->name, "_Relations Inc", NAME_LENGTH);
        existing->category = CATEGORY_FIXED;
    }
    else if (manufacturer == 0x0601)
    {
        // Make TPMS systems
        optional_set(existing->name, "_Shrader Electronics", NAME_LENGTH);
        existing->category = CATEGORY_CAR;
    }
    else if (manufacturer == 0x00d2)
    {
        optional_set(existing->name, "_AbTemp", NAME_LENGTH);
        g_debug("  Ignoring manufdata");
    }
    else if (manufacturer == 0xb1bc)
    {
        // This code appears to be some special mesh message
        optional_set(existing->name, "Mesh message", NAME_LENGTH);
    }
    else
    {
            // https://www.bluetooth.com/specifications/assigned-numbers/16-bit-uuids-for-members/
        const char* manuf = company_id_to_string(manufacturer, &existing->category);
        char underscored[NAME_LENGTH];
        if (manuf != NULL)
        {
            // Prefix with _ to indicate that this isn't a for-sure name
            snprintf(underscored, sizeof(underscored), "_%s", manuf);
            optional_set(existing->name,underscored, NAME_LENGTH);
        }
        else 
        {
            char manuf[32];
            snprintf(manuf, sizeof(manuf), "_Manufacturer 0x%04x", manufacturer);
            g_info("  Did not recognize %s", manuf);
            optional_set(existing->name, manuf, NAME_LENGTH);
        }
    }
}


/*
    Database of Bluetooth company Ids (see Bluetooth website)
*/
const char *company_id_to_string(int company_id, int8_t* category)
{
	switch (company_id) {
	case 0x0000:
		return "Ericsson Technology Licensing";
	case 0x0001:
		return "Nokia Mobile Phones";
	case 0x0002:
		return "Intel Corp.";
	case 0x0003:
		return "IBM Corp.";
	case 0x0004:
		return "Toshiba Corp.";
	case 0x0005:
		return "3Com";
	case 0x0006:
		return "Microsoft";
	case 0x0007:
		return "Lucent";
	case 0x0008:
		return "Motorola";
	case 0x0009:
		return "Infineon Technologies AG";
	case 0x000a:
		return "Cambridge Silicon Radio";
	case 0x000b:
		return "Silicon Wave";
	case 0x000c:
		return "Digianswer A/S";
	case 0x000d:
		return "Texas Instruments Inc.";
	case 0x000e:
		return "Ceva, Inc. (formerly Parthus Technologies, Inc.)";
	case 0x000f:
		return "Broadcom Corporation";
	case 0x0010:
		return "Mitel Semiconductor";
	case 0x0011:
		return "Widcomm, Inc";
	case 0x0012:
		return "Zeevo, Inc.";
	case 0x0013:
		return "Atmel Corporation";
	case 0x0014:
		return "Mitsubishi Electric Corporation";
	case 0x0015:
		return "RTX Telecom A/S";
	case 0x0016:
		return "KC Technology Inc.";
	case 0x0017:
		return "NewLogic";
	case 0x0018:
		return "Transilica, Inc.";
	case 0x0019:
		return "Rohde & Schwarz GmbH & Co. KG";
	case 0x001a:
		return "TTPCom Limited";
	case 0x001b:
		return "Signia Technologies, Inc.";
	case 0x001c:
		return "Conexant Systems Inc.";
	case 0x001d:
		return "Qualcomm";
	case 0x001e:
		return "Inventel";
	case 0x001f:
		return "AVM Berlin";
	case 0x0020:
		return "BandSpeed, Inc.";
	case 33:
		return "Mansella Ltd";
	case 34:
		return "NEC Corporation";
	case 35:
		return "WavePlus Technology Co., Ltd.";
	case 36:
		return "Alcatel";
	case 37:
		return "NXP Semiconductors (formerly Philips Semiconductors)";
	case 38:
		return "C Technologies";
	case 39:
		return "Open Interface";
	case 40:
		return "R F Micro Devices";
	case 41:
		return "Hitachi Ltd";
	case 42:
		return "Symbol Technologies, Inc.";
	case 43:
		return "Tenovis";
	case 44:
		return "Macronix International Co. Ltd.";
	case 45:
		return "GCT Semiconductor";
	case 46:
		return "Norwood Systems";
	case 47:
		return "MewTel Technology Inc.";
	case 48:
		return "ST Microelectronics";
	case 49:
		return "Synopsis";
	case 50:
		return "Red-M (Communications) Ltd";
	case 51:
		return "Commil Ltd";
	case 52:
		return "Computer Access Technology Corporation (CATC)";
	case 53:
		return "Eclipse (HQ Espana) S.L.";
	case 54:
		return "Renesas Electronics Corporation";
	case 55:
		return "Mobilian Corporation";
	case 56:
		return "Terax";
	case 57:
		return "Integrated System Solution Corp.";
	case 58:
		return "Matsushita Electric Industrial Co., Ltd.";
	case 59:
		return "Gennum Corporation";
	case 60:
		return "BlackBerry Limited (formerly Research In Motion)";
	case 61:
		return "IPextreme, Inc.";
	case 62:
		return "Systems and Chips, Inc.";
	case 63:
		return "Bluetooth SIG, Inc.";
	case 64:
		return "Seiko Epson Corporation";
	case 65:
		return "Integrated Silicon Solution Taiwan, Inc.";
	case 66:
		return "CONWISE Technology Corporation Ltd";
	case 67:
		return "PARROT SA";
	case 68:
		return "Socket Mobile";
	case 69:
		return "Atheros Communications, Inc.";
	case 70:
		return "MediaTek, Inc.";
	case 71:
		return "Bluegiga";
	case 72:
		return "Marvell Technology Group Ltd.";
	case 73:
		return "3DSP Corporation";
	case 74:
		return "Accel Semiconductor Ltd.";
	case 75:
		return "Continental Automotive Systems";
	case 76:
		return "Apple, Inc.";
	case 77:
		return "Staccato Communications, Inc.";
	case 78:
		return "Avago Technologies";
	case 79:
		return "APT Licensing Ltd.";
	case 80:
		return "SiRF Technology";
	case 81:
		return "Tzero Technologies, Inc.";
	case 82:
		return "J&M Corporation";
	case 83:
		return "Free2move AB";
	case 84:
		return "3DiJoy Corporation";
	case 85:
		return "Plantronics, Inc.";
	case 86:
		return "Sony Ericsson Mobile Communications";
	case 87:
		return "Harman International Industries, Inc.";
	case 88:
		return "Vizio, Inc.";
	case 89:
		return "Nordic Semiconductor ASA";
	case 90:
		return "EM Microelectronic-Marin SA";
	case 91:
		return "Ralink Technology Corporation";
	case 92:
		return "Belkin International, Inc.";
	case 93:
		return "Realtek Semiconductor Corporation";
	case 94:
		return "Stonestreet One, LLC";
	case 95:
		return "Wicentric, Inc.";
	case 96:
		return "RivieraWaves S.A.S";
	case 97:
		return "RDA Microelectronics";
	case 98:
		return "Gibson Guitars";
	case 99:
		return "MiCommand Inc.";
	case 100:
		return "Band XI International, LLC";
	case 0x0065:
        *category = CATEGORY_FIXED;
		return "Hewlett-Packard Company";
	case 102:
		return "9Solutions Oy";
	case 103:
		return "GN Netcom A/S";
	case 104:
		return "General Motors";
	case 105:
		return "A&D Engineering, Inc.";
	case 106:
		return "MindTree Ltd.";
	case 107:
		return "Polar Electro OY";
	case 108:
		return "Beautiful Enterprise Co., Ltd.";
	case 109:
		return "BriarTek, Inc.";
	case 110:
		return "Summit Data Communications, Inc.";
	case 111:
		return "Sound ID";
	case 112:
		return "Monster, LLC";
	case 113:
		return "connectBlue AB";
	case 114:
		return "ShangHai Super Smart Electronics Co. Ltd.";
	case 115:
		return "Group Sense Ltd.";
	case 116:
		return "Zomm, LLC";
	case 117:
		return "Samsung Electronics Co. Ltd.";
	case 118:
		return "Creative Technology Ltd.";
	case 119:
		return "Laird Technologies";
	case 120:
		return "Nike, Inc.";
	case 121:
		return "lesswire AG";
	case 122:
		return "MStar Semiconductor, Inc.";
	case 123:
		return "Hanlynn Technologies";
	case 124:
		return "A & R Cambridge";
	case 125:
		return "Seers Technology Co. Ltd";
	case 126:
		return "Sports Tracking Technologies Ltd.";
	case 127:
		return "Autonet Mobile";
	case 128:
		return "DeLorme Publishing Company, Inc.";
	case 129:
		return "WuXi Vimicro";
	case 130:
		return "Sennheiser Communications A/S";
	case 131:
		return "TimeKeeping Systems, Inc.";
	case 132:
		return "Ludus Helsinki Ltd.";
	case 133:
		return "BlueRadios, Inc.";
	case 134:
		return "equinox AG";
	case 135:
		return "Garmin International, Inc.";
	case 136:
		return "Ecotest";
	case 137:
		return "GN ReSound A/S";
	case 138:
		return "Jawbone";
	case 139:
		return "Topcorn Positioning Systems, LLC";
	case 140:
		return "Gimbal Inc. (formerly Qualcomm Labs, Inc. and Qualcomm Retail Solutions, Inc.)";
	case 141:
		return "Zscan Software";
	case 142:
		return "Quintic Corp.";
	case 143:
		return "Stollman E+V GmbH";
	case 144:
		return "Funai Electric Co., Ltd.";
	case 145:
		return "Advanced PANMOBIL Systems GmbH & Co. KG";
	case 146:
		return "ThinkOptics, Inc.";
	case 147:
		return "Universal Electronics, Inc.";
	case 148:
		return "Airoha Technology Corp.";
	case 149:
		return "NEC Lighting, Ltd.";
	case 150:
		return "ODM Technology, Inc.";
	case 151:
		return "ConnecteDevice Ltd.";
	case 152:
		return "zer01.tv GmbH";
	case 153:
		return "i.Tech Dynamic Global Distribution Ltd.";
	case 154:
		return "Alpwise";
	case 155:
		return "Jiangsu Toppower Automotive Electronics Co., Ltd.";
	case 156:
		return "Colorfy, Inc.";
	case 157:
		return "Geoforce Inc.";
	case 158:
		return "Bose Corporation";
	case 159:
		return "Suunto Oy";
	case 160:
		return "Kensington Computer Products Group";
	case 161:
		return "SR-Medizinelektronik";
	case 162:
		return "Vertu Corporation Limited";
	case 163:
		return "Meta Watch Ltd.";
	case 164:
		return "LINAK A/S";
	case 165:
		return "OTL Dynamics LLC";
	case 166:
		return "Panda Ocean Inc.";
	case 167:
		return "Visteon Corporation";
	case 168:
		return "ARP Devices Limited";
	case 169:
		return "Magneti Marelli S.p.A";
	case 170:
		return "CAEN RFID srl";
	case 171:
		return "Ingenieur-Systemgruppe Zahn GmbH";
	case 172:
		return "Green Throttle Games";
	case 173:
		return "Peter Systemtechnik GmbH";
	case 174:
		return "Omegawave Oy";
	case 175:
		return "Cinetix";
	case 176:
		return "Passif Semiconductor Corp";
	case 177:
		return "Saris Cycling Group, Inc";
	case 178:
		return "Bekey A/S";
	case 179:
		return "Clarinox Technologies Pty. Ltd.";
	case 180:
		return "BDE Technology Co., Ltd.";
	case 181:
		return "Swirl Networks";
	case 182:
		return "Meso international";
	case 183:
		return "TreLab Ltd";
	case 184:
		return "Qualcomm Innovation Center, Inc. (QuIC)";
	case 185:
		return "Johnson Controls, Inc.";
	case 186:
		return "Starkey Laboratories Inc.";
	case 187:
		return "S-Power Electronics Limited";
	case 188:
		return "Ace Sensor Inc";
	case 189:
		return "Aplix Corporation";
	case 190:
		return "AAMP of America";
	case 191:
		return "Stalmart Technology Limited";
	case 192:
		return "AMICCOM Electronics Corporation";
	case 193:
		return "Shenzhen Excelsecu Data Technology Co.,Ltd";
	case 194:
		return "Geneq Inc.";
	case 195:
		return "adidas AG";
	case 196:
		return "LG Electronics";
	case 197:
		return "Onset Computer Corporation";
	case 198:
		return "Selfly BV";
	case 199:
		return "Quuppa Oy.";
	case 200:
		return "GeLo Inc";
	case 201:
		return "Evluma";
	case 202:
		return "MC10";
	case 203:
		return "Binauric SE";
	case 204:
		return "Beats Electronics";
	case 205:
		return "Microchip Technology Inc.";
	case 206:
		return "Elgato Systems GmbH";
	case 207:
		return "ARCHOS SA";
	case 208:
		return "Dexcom, Inc.";
	case 209:
		return "Polar Electro Europe B.V.";
	case 210:
		return "Dialog Semiconductor B.V.";
	case 211:
		return "Taixingbang Technology (HK) Co,. LTD.";
	case 212:
		return "Kawantech";
	case 213:
		return "Austco Communication Systems";
	case 214:
		return "Timex Group USA, Inc.";
	case 215:
		return "Qualcomm Technologies, Inc.";
	case 216:
		return "Qualcomm Connected Experiences, Inc.";
	case 217:
		return "Voyetra Turtle Beach";
	case 218:
		return "txtr GmbH";
	case 219:
		return "Biosentronics";
	case 220:
		return "Procter & Gamble";
	case 221:
		return "Hosiden Corporation";
	case 222:
		return "Muzik LLC";
	case 223:
		return "Misfit Wearables Corp";
	case 224:
		return "Google";
	case 225:
		return "Danlers Ltd";
	case 226:
		return "Semilink Inc";
	case 227:
		return "inMusic Brands, Inc";
	case 228:
		return "L.S. Research Inc.";
	case 229:
		return "Eden Software Consultants Ltd.";
	case 230:
		return "Freshtemp";
	case 231:
		return "KS Technologies";
	case 232:
		return "ACTS Technologies";
	case 233:
		return "Vtrack Systems";
	case 234:
		return "Nielsen-Kellerman Company";
	case 235:
		return "Server Technology, Inc.";
	case 236:
		return "BioResearch Associates";
	case 237:
		return "Jolly Logic, LLC";
	case 238:
		return "Above Average Outcomes, Inc.";
	case 239:
		return "Bitsplitters GmbH";
	case 240:
		return "PayPal, Inc.";
	case 241:
		return "Witron Technology Limited";
	case 242:
		return "Aether Things Inc. (formerly Morse Project Inc.)";
	case 243:
		return "Kent Displays Inc.";
	case 244:
		return "Nautilus Inc.";
	case 245:
		return "Smartifier Oy";
	case 246:
		return "Elcometer Limited";
	case 247:
		return "VSN Technologies Inc.";
	case 248:
		return "AceUni Corp., Ltd.";
	case 249:
		return "StickNFind";
	case 250:
		return "Crystal Code AB";
	case 251:
		return "KOUKAAM a.s.";
	case 252:
		return "Delphi Corporation";
	case 253:
		return "ValenceTech Limited";
	case 254:
		return "Reserved";
	case 255:
		return "Typo Products, LLC";
	case 256:
		return "TomTom International BV";
	case 257:
		return "Fugoo, Inc";
	case 258:
		return "Keiser Corporation";
	case 259:
		return "Bang & Olufsen A/S";
	case 260:
		return "PLUS Locations Systems Pty Ltd";
	case 261:
		return "Ubiquitous Computing Technology Corporation";
	case 262:
		return "Innovative Yachtter Solutions";
	case 263:
		return "William Demant Holding A/S";
	case 264:
		return "Chicony Electronics Co., Ltd.";
	case 265:
		return "Atus BV";
	case 266:
		return "Codegate Ltd.";
	case 267:
		return "ERi, Inc.";
	case 268:
		return "Transducers Direct, LLC";
	case 269:
		return "Fujitsu Ten Limited";
	case 270:
		return "Audi AG";
	case 271:
		return "HiSilicon Technologies Co., Ltd.";
	case 272:
		return "Nippon Seiki Co., Ltd.";
	case 273:
		return "Steelseries ApS";
	case 274:
		return "vyzybl Inc.";
	case 275:
		return "Openbrain Technologies, Co., Ltd.";
	case 276:
		return "Xensr";
	case 277:
		return "e.solutions";
	case 278:
		return "1OAK Technologies";
	case 279:
		return "Wimoto Technologies Inc";
	case 280:
		return "Radius Networks, Inc.";
	case 281:
		return "Wize Technology Co., Ltd.";
	case 282:
		return "Qualcomm Labs, Inc.";
	case 283:
		return "Aruba Networks";
	case 284:
		return "Baidu";
	case 285:
		return "Arendi AG";
	case 286:
		return "Skoda Auto a.s.";
	case 287:
		return "Volkswagon AG";
	case 288:
		return "Porsche AG";
	case 289:
		return "Sino Wealth Electronic Ltd.";
	case 290:
		return "AirTurn, Inc.";
	case 291:
		return "Kinsa, Inc.";
	case 292:
		return "HID Global";
	case 293:
		return "SEAT es";
	case 294:
		return "Promethean Ltd.";
	case 295:
		return "Salutica Allied Solutions";
	case 296:
		return "GPSI Group Pty Ltd";
	case 297:
		return "Nimble Devices Oy";
	case 298:
		return "Changzhou Yongse Infotech Co., Ltd";
	case 299:
		return "SportIQ";
	case 300:
		return "TEMEC Instruments B.V.";
	case 301:
		return "Sony Corporation";
	case 302:
		return "ASSA ABLOY";
	case 303:
		return "Clarion Co., Ltd.";
	case 304:
		return "Warehouse Innovations";
	case 305:
		return "Cypress Semiconductor Corporation";
	case 306:
		return "MADS Inc";
	case 307:
		return "Blue Maestro Limited";
	case 308:
		return "Resolution Products, Inc.";
	case 309:
		return "Airewear LLC";
	case 310:
		return "Seed Labs, Inc. (formerly ETC sp. z.o.o.)";
	case 311:
		return "Prestigio Plaza Ltd.";
	case 312:
		return "NTEO Inc.";
	case 313:
		return "Focus Systems Corporation";
	case 314:
		return "Tencent Holdings Limited";
	case 315:
		return "Allegion";
	case 316:
		return "Murata Manufacuring Co., Ltd.";
	case 317:
		return "WirelessWERX";
	case 318:
		return "Nod, Inc.";
	case 319:
		return "B&B Manufacturing Company";
	case 320:
		return "Alpine Electronics (China) Co., Ltd";
	case 321:
		return "FedEx Services";
	case 322:
		return "Grape Systems Inc.";
	case 323:
		return "Bkon Connect";
	case 324:
		return "Lintech GmbH";
	case 325:
		return "Novatel Wireless";
	case 326:
		return "Ciright";
	case 327:
		return "Mighty Cast, Inc.";
	case 328:
		return "Ambimat Electronics";
	case 329:
		return "Perytons Ltd.";
	case 330:
		return "Tivoli Audio, LLC";
	case 331:
		return "Master Lock";
	case 332:
		return "Mesh-Net Ltd";
	case 333:
		return "Huizhou Desay SV Automotive CO., LTD.";
	case 334:
		return "Tangerine, Inc.";
	case 335:
		return "B&W Group Ltd.";
	case 336:
		return "Pioneer Corporation";
	case 337:
		return "OnBeep";
	case 338:
		return "Vernier Software & Technology";
	case 339:
		return "ROL Ergo";
	case 340:
		return "Pebble Technology";
	case 341:
		return "NETATMO";
	case 342:
		return "Accumulate AB";
	case 343:
		return "Anhui Huami Information Technology Co., Ltd.";
	case 344:
		return "Inmite s.r.o.";
	case 345:
		return "ChefSteps, Inc.";
	case 346:
		return "micas AG";
	case 347:
		return "Biomedical Research Ltd.";
	case 348:
		return "Pitius Tec S.L.";
	case 349:
		return "Estimote, Inc.";
	case 350:
		return "Unikey Technologies, Inc.";
	case 351:
		return "Timer Cap Co.";
	case 352:
		return "AwoX";
	case 353:
		return "yikes";
	case 354:
		return "MADSGlobal NZ Ltd.";
	case 355:
		return "PCH International";
	case 356:
		return "Qingdao Yeelink Information Technology Co., Ltd.";
	case 357:
		return "Milwaukee Tool (formerly Milwaukee Electric Tools)";
	case 358:
		return "MISHIK Pte Ltd";
	case 359:
		return "Bayer HealthCare";
	case 360:
		return "Spicebox LLC";
	case 361:
		return "emberlight";
	case 362:
		return "Cooper-Atkins Corporation";
	case 363:
		return "Qblinks";
	case 364:
		return "MYSPHERA";
	case 365:
		return "LifeScan Inc";
	case 366:
		return "Volantic AB";
	case 367:
		return "Podo Labs, Inc";
	case 368:
		return "Roche Diabetes Care AG";
	case 369:
		return "Amazon Fulfillment Service";
	case 370:
		return "Connovate Technology Private Limited";
	case 371:
		return "Kocomojo, LLC";
	case 372:
		return "Everykey LLC";
	case 373:
		return "Dynamic Controls";
	case 374:
		return "SentriLock";
	case 375:
		return "I-SYST inc.";
	case 376:
		return "CASIO COMPUTER CO., LTD.";
	case 377:
		return "LAPIS Semiconductor Co., Ltd.";
	case 378:
		return "Telemonitor, Inc.";
	case 379:
		return "taskit GmbH";
	case 380:
		return "Daimler AG";
	case 381:
		return "BatAndCat";
	case 382:
		return "BluDotz Ltd";
	case 383:
		return "XTel ApS";
	case 384:
		return "Gigaset Communications GmbH";
	case 385:
		return "Gecko Health Innovations, Inc.";
	case 386:
		return "HOP Ubiquitous";
	case 387:
		return "To Be Assigned";
	case 388:
		return "Nectar";
	case 389:
		return "bel'apps LLC";
	case 390:
		return "CORE Lighting Ltd";
	case 391:
		return "Seraphim Sense Ltd";
	case 392:
		return "Unico RBC";
	case 393:
		return "Physical Enterprises Inc.";
	case 394:
		return "Able Trend Technology Limited";
	case 395:
		return "Konica Minolta, Inc.";
	case 396:
		return "Wilo SE";
	case 397:
		return "Extron Design Services";
	case 398:
		return "Fitbit, Inc.";
	case 399:
		return "Fireflies Systems";
	case 400:
		return "Intelletto Technologies Inc.";
	case 401:
		return "FDK CORPORATION";
	case 402:
		return "Cloudleaf, Inc";
	case 403:
		return "Maveric Automation LLC";
	case 404:
		return "Acoustic Stream Corporation";
	case 405:
		return "Zuli";
	case 406:
		return "Paxton Access Ltd";
	case 407:
		return "WiSilica Inc";
	case 408:
		return "Vengit Limited";
	case 409:
		return "SALTO SYSTEMS S.L.";
	case 410:
		return "TRON Forum (formerly T-Engine Forum)";
	case 411:
		return "CUBETECH s.r.o.";
	case 412:
		return "Cokiya Incorporated";
	case 413:
		return "CVS Health";
	case 414:
		return "Ceruus";
	case 415:
		return "Strainstall Ltd";
	case 416:
		return "Channel Enterprises (HK) Ltd.";
	case 417:
		return "FIAMM";
	case 418:
		return "GIGALANE.CO.,LTD";
	case 419:
		return "EROAD";
	case 420:
		return "Mine Safety Appliances";
	case 421:
		return "Icon Health and Fitness";
	case 422:
		return "Asandoo GmbH";
	case 423:
		return "ENERGOUS CORPORATION";
	case 424:
		return "Taobao";
	case 425:
		return "Canon Inc.";
	case 426:
		return "Geophysical Technology Inc.";
	case 427:
		return "Facebook, Inc.";
	case 428:
		return "Nipro Diagnostics, Inc.";
	case 429:
		return "FlightSafety International";
	case 430:
		return "Earlens Corporation";
	case 431:
		return "Sunrise Micro Devices, Inc.";
	case 432:
		return "Star Micronics Co., Ltd.";
	case 433:
		return "Netizens Sp. z o.o.";
	case 434:
		return "Nymi Inc.";
	case 435:
		return "Nytec, Inc.";
	case 436:
		return "Trineo Sp. z o.o.";
	case 437:
		return "Nest Labs Inc.";
	case 438:
		return "LM Technologies Ltd";
	case 439:
		return "General Electric Company";
	case 440:
		return "i+D3 S.L.";
	case 441:
		return "HANA Micron";
	case 442:
		return "Stages Cycling LLC";
	case 443:
		return "Cochlear Bone Anchored Solutions AB";
	case 444:
		return "SenionLab AB";
	case 445:
		return "Syszone Co., Ltd";
	case 446:
		return "Pulsate Mobile Ltd.";
	case 447:
		return "Hong Kong HunterSun Electronic Limited";
	case 448:
		return "pironex GmbH";
	case 449:
		return "BRADATECH Corp.";
	case 450:
		return "Transenergooil AG";
	case 451:
		return "Bunch";
	case 452:
		return "DME Microelectronics";
	case 453:
		return "Bitcraze AB";
	case 454:
		return "HASWARE Inc.";
	case 455:
		return "Abiogenix Inc.";
	case 456:
		return "Poly-Control ApS";
	case 457:
		return "Avi-on";
	case 458:
		return "Laerdal Medical AS";
	case 459:
		return "Fetch My Pet";
	case 460:
		return "Sam Labs Ltd.";
	case 461:
		return "Chengdu Synwing Technology Ltd";
	case 462:
		return "HOUWA SYSTEM DESIGN, k.k.";
	case 463:
		return "BSH";
	case 464:
		return "Primus Inter Pares Ltd";
	case 465:
		return "August";
	case 466:
		return "Gill Electronics";
	case 467:
		return "Sky Wave Design";
	case 468:
		return "Newlab S.r.l.";
	case 469:
		return "ELAD srl";
	case 470:
		return "G-wearables inc.";
	case 471:
		return "Squadrone Systems Inc.";
	case 472:
		return "Code Corporation";
	case 473:
		return "Savant Systems LLC";
	case 0x01da:
        *category = CATEGORY_FIXED;
		return "Logitech" /* International SA"*/;
	case 475:
		return "Innblue Consulting";
	case 476:
		return "iParking Ltd.";
	case 477:
		return /*"Koninklijke */ "Philips Electronics N.V.";
	case 478:
		return "Minelab Electronics Pty Limited";
	case 479:
		return "Bison Group Ltd.";
	case 480:
		return "Widex A/S";
	case 481:
		return "Jolla Ltd";
	case 482:
		return "Lectronix, Inc.";
	case 483:
		return "Caterpillar Inc";
	case 484:
		return "Freedom Innovations";
	case 485:
		return "Dynamic Devices Ltd";
	case 486:
		return "Technology Solutions (UK) Ltd";
	case 487:
		return "IPS Group Inc.";
	case 488:
		return "STIR";
	case 489:
		return "Sano, Inc";
	case 490:
		return "Advanced Application Design, Inc.";
	case 491:
		return "AutoMap LLC";
	case 492:
		return "Spreadtrum Communications Shanghai Ltd";
	case 493:
		return "CuteCircuit LTD";
	case 494:
		return "Valeo Service";
	case 495:
		return "Fullpower Technologies, Inc.";
	case 496:
		return "KloudNation";
	case 497:
		return "Zebra Technologies Corporation";
	case 498:
		return "Itron, Inc.";
	case 499:
		return "The University of Tokyo";
	case 500:
		return "UTC Fire and Security";
	case 501:
		return "Cool Webthings Limited";
	case 502:
		return "DJO Global";
	case 503:
		return "Gelliner Limited";
	case 504:
		return "Anyka (Guangzhou) Microelectronics Technology Co, LTD";
	case 505:
		return "Medtronic, Inc.";
	case 506:
		return "Gozio, Inc.";
	case 507:
		return "Form Lifting, LLC";
	case 508:
		return "Wahoo Fitness, LLC";
	case 509:
		return "Kontakt Micro-Location Sp. z o.o.";
	case 510:
		return "Radio System Corporation";
	case 511:
		return "Freescale Semiconductor, Inc.";
	case 512:
		return "Verifone Systems PTe Ltd. Taiwan Branch";
	case 513:
		return "AR Timing";
	case 514:
		return "Rigado LLC";
	case 515:
		return "Kemppi Oy";
	case 516:
		return "Tapcentive Inc.";
	case 517:
		return "Smartbotics Inc.";
	case 518:
		return "Otter Products, LLC";
	case 519:
		return "STEMP Inc.";
	case 520:
		return "LumiGeek LLC";
	case 521:
		return "InvisionHeart Inc.";
	case 522:
		return "Macnica Inc.";
	case 523:
		return "Jaguar Land Rover Limited";
	case 524:
		return "CoroWare Technologies, Inc";
	case 525:
		return "Simplo Technology Co., LTD";
	case 526:
		return "Omron Healthcare Co., LTD";
	case 527:
		return "Comodule GMBH";
	case 528:
		return "ikeGPS";
	case 529:
		return "Telink Semiconductor Co. Ltd";
	case 530:
		return "Interplan Co., Ltd";
	case 531:
		return "Wyler AG";
	case 532:
		return "IK Multimedia Production srl";
	case 533:
		return "Lukoton Experience Oy";
	case 534:
		return "MTI Ltd";
	case 535:
		return "Tech4home, Lda";
	case 536:
		return "Hiotech AB";
	case 537:
		return "DOTT Limited";
	case 538:
		return "Blue Speck Labs, LLC";
	case 539:
		return "Cisco Systems Inc";
	case 540:
		return "Mobicomm Inc";
	case 541:
		return "Edamic";
	case 542:
		return "Goodnet Ltd";
	case 543:
		return "Luster Leaf Products Inc";
	case 544:
		return "Manus Machina BV";
	case 545:
		return "Mobiquity Networks Inc";
	case 546:
		return "Praxis Dynamics";
	case 547:
		return "Philip Morris Products S.A.";
	case 548:
		return "Comarch SA";
	case 549:
		return "Nestl Nespresso S.A.";
	case 550:
		return "Merlinia A/S";
	case 551:
		return "LifeBEAM Technologies";
	case 552:
		return "Twocanoes Labs, LLC";
	case 553:
		return "Muoverti Limited";
	case 554:
		return "Stamer Musikanlagen GMBH";
	case 555:
		return "Tesla Motors";
	case 556:
		return "Pharynks Corporation";
	case 557:
		return "Lupine";
	case 558:
		return "Siemens AG";
	case 559:
		return "Huami (Shanghai) Culture Communication CO., LTD";
	case 560:
		return "Foster Electric Company, Ltd";
	case 561:
		return "ETA SA";
	case 562:
		return "x-Senso Solutions Kft";
	case 563:
		return "Shenzhen SuLong Communication Ltd";
	case 564:
		return "FengFan (BeiJing) Technology Co, Ltd";
	case 565:
		return "Qrio Inc";
	case 566:
		return "Pitpatpet Ltd";
	case 567:
		return "MSHeli s.r.l.";
	case 568:
		return "Trakm8 Ltd";
	case 569:
		return "JIN CO, Ltd";
	case 570:
		return "Alatech Technology";
	case 571:
		return "Beijing CarePulse Electronic Technology Co, Ltd";
	case 572:
		return "Awarepoint";
	case 573:
		return "ViCentra B.V.";
	case 574:
		return "Raven Industries";
	case 575:
		return "WaveWare Technologies";
	case 576:
		return "Argenox Technologies";
	case 577:
		return "Bragi GmbH";
	case 578:
		return "16Lab Inc";
	case 579:
		return "Masimo Corp";
	case 580:
		return "Iotera Inc.";
	case 581:
		return "Endress+Hauser";
	case 582:
		return "ACKme Networks, Inc.";
	case 583:
		return "FiftyThree Inc.";
	case 584:
		return "Parker Hannifin Corp";
	case 585:
		return "Transcranial Ltd";
	case 586:
		return "Uwatec AG";
	case 587:
		return "Orlan LLC";
	case 588:
		return "Blue Clover Devices";
	default:
		return NULL;
	}
}