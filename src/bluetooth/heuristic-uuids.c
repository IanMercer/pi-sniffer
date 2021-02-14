// UUID heuristics

#include "utility.h"
#include "device.h"
#include "heuristics.h"

#include <glib.h>
#include <stdbool.h>
#include <stdio.h>
#include <sys/types.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/types.h>
#include <string.h>

void handle_uuids(struct Device *device, char *uuidArray[2048], int actualLength, char* gatts, int gatts_length)
{
    // Print off the UUIDs here
    for (int i = 0; i < actualLength; i++)
    {
        char *strCopy = strdup(uuidArray[i]);

        // All common BLE UUIDs are of the form: 0000XXXX-0000-1000-8000-00805f9b34fb
        // so we only need two hex bytes. But Apple and others use complete GUIDs
        // for their own services, so let's take the first four hex bytes
        strCopy[8] = '\0';
        uint64_t ble_uuid = strtoul(strCopy, NULL, 16);

        // https://www.bluetooth.com/specifications/gatt/characteristics/
        // See also https://github.com/ghostyguo/BleUuidExplorer/blob/master/app/src/main/java/ghostysoft/bleuuidexplorer/GattAttributes.java

        if (ble_uuid == 0x00001000ul) append_text(gatts, gatts_length, "ServiceDiscoveryServer, ");
        else if (ble_uuid == 0x00001001ul) append_text(gatts, gatts_length, "BrowseGroupDescriptor, ");
        else if (ble_uuid == 0x00001002ul) append_text(gatts, gatts_length, "PublicBrowseGroup, ");
        else if (ble_uuid == 0x00001816ul) append_text(gatts, gatts_length, "CyclingCadence, ");
        else if (ble_uuid == 0x00001818ul) 
        {
            append_text(gatts, gatts_length, "CyclingPower, ");
            soft_set_category(&device->category, CATEGORY_WEARABLE);
        }
        else if (ble_uuid == 0x00001101ul) append_text(gatts, gatts_length, "SerialPort, ");
        else if (ble_uuid == 0x00001102ul) append_text(gatts, gatts_length, "LANAccessUsingPPP, ");
        else if (ble_uuid == 0x00001103ul) append_text(gatts, gatts_length, "DialupNetworking, ");
        else if (ble_uuid == 0x00001104ul) append_text(gatts, gatts_length, "IrMCSync, ");
        else if (ble_uuid == 0x00001105ul) append_text(gatts, gatts_length, "OBEXObjectPush, ");
        else if (ble_uuid == 0x00001106ul) append_text(gatts, gatts_length, "OBEXFileTransfer, ");
        else if (ble_uuid == 0x00001107ul) append_text(gatts, gatts_length, "IrMCSyncCommand, ");
        else if (ble_uuid == 0x00001108ul) 
        {
            append_text(gatts, gatts_length, "Headset, ");
            soft_set_category(&device->category, CATEGORY_HEADPHONES);
        }
        else if (ble_uuid == 0x00001109ul) append_text(gatts, gatts_length, "CordlessTelephony, ");
        else if (ble_uuid == 0x0000110Aul) append_text(gatts, gatts_length, "AudioSource, ");
        else if (ble_uuid == 0x0000110Bul) append_text(gatts, gatts_length, "AudioSink, ");
        else if (ble_uuid == 0x0000110Cul) append_text(gatts, gatts_length, "AVRemoteControlTarget, ");
        else if (ble_uuid == 0x0000110Dul) append_text(gatts, gatts_length, "AdvancedAudioDistribution, ");
        else if (ble_uuid == 0x0000110Eul) append_text(gatts, gatts_length, "AVRemoteControl, ");
        else if (ble_uuid == 0x0000110Ful) append_text(gatts, gatts_length, "VideoConferencing, ");
        else if (ble_uuid == 0x00001110ul) append_text(gatts, gatts_length, "Intercom, ");
        else if (ble_uuid == 0x00001111ul) append_text(gatts, gatts_length, "Fax, ");
        else if (ble_uuid == 0x00001112ul) append_text(gatts, gatts_length, "HeadsetAudioGateway, ");
        else if (ble_uuid == 0x00001113ul) append_text(gatts, gatts_length, "WAP, ");
        else if (ble_uuid == 0x00001114ul) append_text(gatts, gatts_length, "WAPClient, ");
        else if (ble_uuid == 0x00001115ul) append_text(gatts, gatts_length, "PANU, ");
        else if (ble_uuid == 0x00001116ul) append_text(gatts, gatts_length, "NAP, ");
        else if (ble_uuid == 0x00001117ul) append_text(gatts, gatts_length, "GN, ");
        else if (ble_uuid == 0x00001118ul) append_text(gatts, gatts_length, "DirectPrinting, ");
        else if (ble_uuid == 0x00001119ul) append_text(gatts, gatts_length, "ReferencePrinting, ");
        else if (ble_uuid == 0x0000111Aul) append_text(gatts, gatts_length, "Imaging, ");
        else if (ble_uuid == 0x0000111Bul) append_text(gatts, gatts_length, "ImagingResponder, ");
        else if (ble_uuid == 0x0000111Cul) append_text(gatts, gatts_length, "ImagingAutomaticArchive, ");
        else if (ble_uuid == 0x0000111Dul) append_text(gatts, gatts_length, "ImagingReferenceObjects, ");
        else if (ble_uuid == 0x0000111Eul)
        {
            append_text(gatts, gatts_length, "Handsfree, ");
            soft_set_category(&device->category, CATEGORY_HEADPHONES);
        }
        else if (ble_uuid == 0x0000111Ful) append_text(gatts, gatts_length, "HandsfreeAudioGateway, ");
        else if (ble_uuid == 0x00001120ul) append_text(gatts, gatts_length, "DirectPrintingReferenceObjects, ");
        else if (ble_uuid == 0x00001121ul) append_text(gatts, gatts_length, "ReflectedUI, ");
        else if (ble_uuid == 0x00001122ul) 
        {
            append_text(gatts, gatts_length, "BasicPrinting, ");
            soft_set_category(&device->category, CATEGORY_PRINTER);
        }
        else if (ble_uuid == 0x00001123ul) append_text(gatts, gatts_length, "PrintingStatus, ");
        else if (ble_uuid == 0x00001124ul) append_text(gatts, gatts_length, "HumanInterfaceDevice, ");
        else if (ble_uuid == 0x00001125ul) append_text(gatts, gatts_length, "HardcopyCableReplacement, ");
        else if (ble_uuid == 0x00001126ul) append_text(gatts, gatts_length, "HCRPrint, ");
        else if (ble_uuid == 0x00001127ul) append_text(gatts, gatts_length, "HCRScan, ");
        else if (ble_uuid == 0x00001128ul) append_text(gatts, gatts_length, "CommonISDNAccess, ");
        else if (ble_uuid == 0x00001129ul) append_text(gatts, gatts_length, "VideoConferencingGW, ");
        else if (ble_uuid == 0x0000112Aul) append_text(gatts, gatts_length, "UDIMT, ");
        else if (ble_uuid == 0x0000112Bul) append_text(gatts, gatts_length, "UDITA, ");
        else if (ble_uuid == 0x0000112Cul) append_text(gatts, gatts_length, "AudioVideo, ");
        else if (ble_uuid == 0x0000112Dul) append_text(gatts, gatts_length, "SIMAccess, ");
        else if (ble_uuid == 0x00001200ul) append_text(gatts, gatts_length, "PnPInformation, ");
        else if (ble_uuid == 0x00001201ul) append_text(gatts, gatts_length, "GenericNetworking, ");
        else if (ble_uuid == 0x00001202ul) append_text(gatts, gatts_length, "GenericFileTransfer, ");
        else if (ble_uuid == 0x00001203ul) append_text(gatts, gatts_length, "GenericAudio, ");
        else if (ble_uuid == 0x00001204ul) append_text(gatts, gatts_length, "GenericTelephony, ");
        else if (ble_uuid == 0x00002a29ul) append_text(gatts, gatts_length, "Manufacturer, ");
        else if (ble_uuid == 0x00001800ul) append_text(gatts, gatts_length, "Generic access, ");
        else if (ble_uuid == 0x00001801ul) append_text(gatts, gatts_length, "Generic attribute, ");
        else if (ble_uuid == 0x00001802ul) append_text(gatts, gatts_length, "Immediate Alert, ");
        else if (ble_uuid == 0x00001803ul) append_text(gatts, gatts_length, "Link loss, ");
        else if (ble_uuid == 0x00001804ul) append_text(gatts, gatts_length, "Tx Power level, ");
        else if (ble_uuid == 0x00001805ul) append_text(gatts, gatts_length, "Current time, ");
        else if (ble_uuid == 0x0000180ful) append_text(gatts, gatts_length, "Battery, ");
        else if (ble_uuid == 0x0000111eul) append_text(gatts, gatts_length, "HandsFree, ");
        else if (ble_uuid == 0x0000180aul) append_text(gatts, gatts_length, "Device information, ");
        else if (ble_uuid == 0x0000180dul) append_text(gatts, gatts_length, "Heart rate service, ");
        else if (ble_uuid == 0x00001812ul) append_text(gatts, gatts_length, "Light?, ");  // not sure
        else if (ble_uuid == 0x00001821ul) {
            // This one is used for beacons used to train the system
            device->is_training_beacon = TRUE;
            append_text(gatts, gatts_length, "Indoor Positioning, ");
        }
        else if (ble_uuid == 0x00002A37ul) {
            append_text(gatts, gatts_length, "Heart rate measurement ");
            soft_set_category(&device->category, CATEGORY_FITNESS);
        }
        else if (ble_uuid == 0x70954782ul) {
            append_text(gatts, gatts_length, "FujiFilm ");
            soft_set_category(&device->category, CATEGORY_CAMERA);
        }
        else if (ble_uuid == 0x04000000ul) {
            append_text(gatts, gatts_length, "Toothbrush ");  // looks like an unofficial UUID
            soft_set_category(&device->category, CATEGORY_TOOTHBRUSH);
        }
        else if (ble_uuid == 0x00006666ul) append_text(gatts, gatts_length, "Bad 0x6666, ");
        else if (ble_uuid == 0x6ada028cul) 
        {
            append_text(gatts, gatts_length, "Notfound(6ada028c), ");
        }
        else if (ble_uuid == 0xa3c87500) 
        {
            append_text(gatts, gatts_length, "Eddystone Configuration, ");
            soft_set_category(&device->category, CATEGORY_BEACON);
        }
        else if (ble_uuid == 0x0000feaaul) 
        {
            append_text(gatts, gatts_length, "Eddystone, ");
            soft_set_category(&device->category, CATEGORY_BEACON);
        }
        else if (ble_uuid == 0x0000de00ul){
            append_text(gatts, gatts_length, "Nikon, ");
            soft_set_category(&device->category, CATEGORY_CAMERA);
        }
        else if (ble_uuid == 0x0000ffa0ul) append_text(gatts, gatts_length, "Accelerometer, ");
        else if (ble_uuid == 0x0000ffe0ul) append_text(gatts, gatts_length, "Temperature, ");

        else if (ble_uuid == 0x0000feecul) {
            append_text(gatts, gatts_length, "Tile, ");
            soft_set_category(&device->category, CATEGORY_BEACON);
        }
        else if (ble_uuid == 0x0000feedul) {
            set_name(device, "Tile", nt_known);       // better than device, they all have same name
            append_text(gatts, gatts_length, "Tile, ");
            soft_set_category(&device->category, CATEGORY_BEACON);
        }
        else if (ble_uuid == 0x0000feaful) {
            append_text(gatts, gatts_length, "Nest, ");
            soft_set_category(&device->category, CATEGORY_SECURITY);
        }
        else if (ble_uuid == 0xadabfb00ul) {
            append_text(gatts, gatts_length, "FitbitHR?, ");
            soft_set_category(&device->category, CATEGORY_FITNESS);
        }
        else if (ble_uuid == 0x0f9652d2ul) {
            append_text(gatts, gatts_length, "Truck, ");    // air suspension etc.
            soft_set_category(&device->category, CATEGORY_CAR);
        }
        else if (ble_uuid == 0x18ea0000ul) {
            append_text(gatts, gatts_length, "DeWalt, ");
            soft_set_category(&device->category, CATEGORY_BEACON);
        }
        else if (ble_uuid == 0xc374034ful) {
            append_text(gatts, gatts_length, "DeWalt, ");
            soft_set_category(&device->category, CATEGORY_BEACON);
        }
        else if (ble_uuid == 0x0000fff0ul) append_text(gatts, gatts_length, "ISSC Transparent Service, ");
        else if (ble_uuid == 0x0000fff1ul) append_text(gatts, gatts_length, "ISSC Transparent RX, ");
        else if (ble_uuid == 0x0000fff2ul) append_text(gatts, gatts_length, "ISSC Transparent TX, ");
        else if (ble_uuid == 0x0000fff6ul) append_text(gatts, gatts_length, "RX, ");
        else if (ble_uuid == 0x0000fff7ul) append_text(gatts, gatts_length, "TX, ");
        else if (ble_uuid == 0x4e72b490ul) append_text(gatts, gatts_length, "Alexa, ");

        else if (ble_uuid == 0x0000fe1cul) append_text(gatts, gatts_length, "NetMedia, Inc.");
        else if (ble_uuid == 0x0000fe1dul) append_text(gatts, gatts_length, "Illuminati Instrument Corporation");
        else if (ble_uuid == 0x0000fe1eul) append_text(gatts, gatts_length, "Smart Innovations Co., Ltd");
        else if (ble_uuid == 0x0000fe1ful) append_text(gatts, gatts_length, "Garmin International, Inc.");
        else if (ble_uuid == 0x0000fe20ul) append_text(gatts, gatts_length, "Emerson");
        else if (ble_uuid == 0x0000fe21ul) {
            append_text(gatts, gatts_length, "Bose Corporation");
            soft_set_category(&device->category, CATEGORY_HEADPHONES);
        }
        else if (ble_uuid == 0x0000fe22ul) append_text(gatts, gatts_length, "Zoll Medical Corporation");
        else if (ble_uuid == 0x0000fe23ul) append_text(gatts, gatts_length, "Zoll Medical Corporation");
        else if (ble_uuid == 0x0000fe24ul) 
        {
            append_text(gatts, gatts_length, "August Home Inc");    // smart locks
            soft_set_category(&device->category, CATEGORY_SECURITY);
        }
        else if (ble_uuid == 0x0000fe25ul) append_text(gatts, gatts_length, "Apple, Inc.");
        else if (ble_uuid == 0x0000fe26ul) append_text(gatts, gatts_length, "Google Inc.");
        else if (ble_uuid == 0x0000fe27ul) append_text(gatts, gatts_length, "Google Inc.");
        else if (ble_uuid == 0x0000fe28ul) append_text(gatts, gatts_length, "Ayla Networks");
        else if (ble_uuid == 0x0000fe29ul) append_text(gatts, gatts_length, "Gibson Innovations");
        else if (ble_uuid == 0x0000fe2aul) append_text(gatts, gatts_length, "DaisyWorks, Inc.");
        else if (ble_uuid == 0x0000fe2bul) append_text(gatts, gatts_length, "ITT Industries");
        else if (ble_uuid == 0x0000fe2cul) append_text(gatts, gatts_length, "Google Inc.");
        else if (ble_uuid == 0x0000fe2dul) append_text(gatts, gatts_length, "SMART INNOVATION Co.,Ltd");
        else if (ble_uuid == 0x0000fe2eul) append_text(gatts, gatts_length, "ERi,Inc.");
        else if (ble_uuid == 0x0000fe2ful) append_text(gatts, gatts_length, "CRESCO Wireless, Inc");
        else if (ble_uuid == 0x0000fe30ul) append_text(gatts, gatts_length, "Volkswagen AG");
        else if (ble_uuid == 0x0000fe31ul) append_text(gatts, gatts_length, "Volkswagen AG");
        else if (ble_uuid == 0x0000fe32ul) append_text(gatts, gatts_length, "Pro-Mark, Inc.");
        else if (ble_uuid == 0x0000fe33ul) append_text(gatts, gatts_length, "CHIPOLO d.o.o.");
        else if (ble_uuid == 0x0000fe34ul) append_text(gatts, gatts_length, "SmallLoop LLC");
        else if (ble_uuid == 0x0000fe35ul) append_text(gatts, gatts_length, "HUAWEI Technologies Co., Ltd");
        else if (ble_uuid == 0x0000fe36ul) append_text(gatts, gatts_length, "HUAWEI Technologies Co., Ltd");
        else if (ble_uuid == 0x0000fe37ul) append_text(gatts, gatts_length, "Spaceek LTD");
        else if (ble_uuid == 0x0000fe38ul) append_text(gatts, gatts_length, "Spaceek LTD");
        else if (ble_uuid == 0x0000fe39ul) append_text(gatts, gatts_length, "TTS Tooltechnic Systems AG & Co. KG");
        else if (ble_uuid == 0x0000fe3aul) append_text(gatts, gatts_length, "TTS Tooltechnic Systems AG & Co. KG");
        else if (ble_uuid == 0x0000fe3bul) append_text(gatts, gatts_length, "Dolby Laboratories");
        else if (ble_uuid == 0x0000fe3cul) append_text(gatts, gatts_length, "Alibaba");
        else if (ble_uuid == 0x0000fe3dul) append_text(gatts, gatts_length, "BD Medical");
        else if (ble_uuid == 0x0000fe3eul) append_text(gatts, gatts_length, "BD Medical");
        else if (ble_uuid == 0x0000fe3ful) append_text(gatts, gatts_length, "Friday Labs Limited");
        else if (ble_uuid == 0x0000fe40ul) append_text(gatts, gatts_length, "Inugo Systems Limited");
        else if (ble_uuid == 0x0000fe41ul) append_text(gatts, gatts_length, "Inugo Systems Limited");
        else if (ble_uuid == 0x0000fe42ul) append_text(gatts, gatts_length, "Nets A/S");
        else if (ble_uuid == 0x0000fe43ul) append_text(gatts, gatts_length, "Andreas Stihl AG & Co. KG");
        else if (ble_uuid == 0x0000fe44ul) append_text(gatts, gatts_length, "SK Telecom");
        else if (ble_uuid == 0x0000fe45ul) append_text(gatts, gatts_length, "Snapchat Inc");
        else if (ble_uuid == 0x0000fe46ul) append_text(gatts, gatts_length, "B&O Play A/S");
        else if (ble_uuid == 0x0000fe47ul) append_text(gatts, gatts_length, "General Motors");
        else if (ble_uuid == 0x0000fe48ul) append_text(gatts, gatts_length, "General Motors");
        else if (ble_uuid == 0x0000fe49ul) append_text(gatts, gatts_length, "SenionLab AB");
        else if (ble_uuid == 0x0000fe4aul) append_text(gatts, gatts_length, "OMRON HEALTHCARE Co., Ltd.");
        else if (ble_uuid == 0x0000fe4bul) append_text(gatts, gatts_length, "Koninklijke Philips N.V.");
        else if (ble_uuid == 0x0000fe4cul) append_text(gatts, gatts_length, "Volkswagen AG");
        else if (ble_uuid == 0x0000fe4dul) append_text(gatts, gatts_length, "Casambi Technologies Oy");
        else if (ble_uuid == 0x0000fe4eul) append_text(gatts, gatts_length, "NTT docomo");
        else if (ble_uuid == 0x0000fe4ful) append_text(gatts, gatts_length, "Molekule, Inc.");
        else if (ble_uuid == 0x0000fe50ul) append_text(gatts, gatts_length, "Google Inc.");
        else if (ble_uuid == 0x0000fe51ul) append_text(gatts, gatts_length, "SRAM");
        else if (ble_uuid == 0x0000fe52ul) append_text(gatts, gatts_length, "SetPoint Medical");
        else if (ble_uuid == 0x0000fe53ul) append_text(gatts, gatts_length, "3M");
        else if (ble_uuid == 0x0000fe54ul) append_text(gatts, gatts_length, "Motiv, Inc.");
        else if (ble_uuid == 0x0000fe55ul) append_text(gatts, gatts_length, "Google Inc.");
        else if (ble_uuid == 0x0000fe56ul) append_text(gatts, gatts_length, "Google Inc.");
        else if (ble_uuid == 0x0000fe57ul) append_text(gatts, gatts_length, "Dotted Labs");
        else if (ble_uuid == 0x0000fe58ul) append_text(gatts, gatts_length, "Nordic Semiconductor ASA");
        else if (ble_uuid == 0x0000fe59ul) append_text(gatts, gatts_length, "Nordic Semiconductor ASA");
        else if (ble_uuid == 0x0000fe5aul) append_text(gatts, gatts_length, "Chronologics Corporation");
        else if (ble_uuid == 0x0000fe5bul) append_text(gatts, gatts_length, "GT-tronics HK Ltd");
        else if (ble_uuid == 0x0000fe5cul) append_text(gatts, gatts_length, "million hunters GmbH");
        else if (ble_uuid == 0x0000fe5dul) append_text(gatts, gatts_length, "Grundfos A/S");
        else if (ble_uuid == 0x0000fe5eul) append_text(gatts, gatts_length, "Plastc Corporation");
        else if (ble_uuid == 0x0000fe5ful) append_text(gatts, gatts_length, "Eyefi, Inc.");
        else if (ble_uuid == 0x0000fe60ul) append_text(gatts, gatts_length, "Lierda Science & Technology Group Co., Ltd.");
        else if (ble_uuid == 0x0000fe61ul) append_text(gatts, gatts_length, "Logitech International SA");
        else if (ble_uuid == 0x0000fe62ul) append_text(gatts, gatts_length, "Indagem Tech LLC");
        else if (ble_uuid == 0x0000fe63ul) append_text(gatts, gatts_length, "Connected Yard, Inc.");
        else if (ble_uuid == 0x0000fe64ul) append_text(gatts, gatts_length, "Siemens AG");
        else if (ble_uuid == 0x0000fe65ul) append_text(gatts, gatts_length, "CHIPOLO d.o.o.");
        else if (ble_uuid == 0x0000fe66ul) append_text(gatts, gatts_length, "Intel Corporation");
        else if (ble_uuid == 0x0000fe67ul) append_text(gatts, gatts_length, "Lab Sensor Solutions");
        else if (ble_uuid == 0x0000fe68ul) append_text(gatts, gatts_length, "Qualcomm Life Inc");
        else if (ble_uuid == 0x0000fe69ul) append_text(gatts, gatts_length, "Qualcomm Life Inc");
        else if (ble_uuid == 0x0000fe6aul) append_text(gatts, gatts_length, "Kontakt Micro-Location Sp. z o.o.");
        else if (ble_uuid == 0x0000fe6bul) append_text(gatts, gatts_length, "TASER International, Inc.");
        else if (ble_uuid == 0x0000fe6cul) append_text(gatts, gatts_length, "TASER International, Inc.");
        else if (ble_uuid == 0x0000fe6dul) append_text(gatts, gatts_length, "The University of Tokyo");
        else if (ble_uuid == 0x0000fe6eul) append_text(gatts, gatts_length, "The University of Tokyo");
        else if (ble_uuid == 0x0000fe6ful) append_text(gatts, gatts_length, "LINE Corporation");
        else if (ble_uuid == 0x0000fe70ul) append_text(gatts, gatts_length, "Beijing Jingdong Century Trading Co., Ltd.");
        else if (ble_uuid == 0x0000fe71ul) append_text(gatts, gatts_length, "Plume Design Inc");
        else if (ble_uuid == 0x0000fe72ul) append_text(gatts, gatts_length, "St. Jude Medical, Inc.");
        else if (ble_uuid == 0x0000fe73ul) append_text(gatts, gatts_length, "St. Jude Medical, Inc.");
        else if (ble_uuid == 0x0000fe74ul) append_text(gatts, gatts_length, "unwire");
        else if (ble_uuid == 0x0000fe75ul) append_text(gatts, gatts_length, "TangoMe");
        else if (ble_uuid == 0x0000fe76ul) append_text(gatts, gatts_length, "TangoMe");
        else if (ble_uuid == 0x0000fe77ul) append_text(gatts, gatts_length, "Hewlett-Packard Company");
        else if (ble_uuid == 0x0000fe78ul) append_text(gatts, gatts_length, "Hewlett-Packard Company");
        else if (ble_uuid == 0x0000fe79ul) append_text(gatts, gatts_length, "Zebra Technologies");
        else if (ble_uuid == 0x0000fe7aul) append_text(gatts, gatts_length, "Bragi GmbH");
        else if (ble_uuid == 0x0000fe7bul) append_text(gatts, gatts_length, "Orion Labs, Inc.");
        else if (ble_uuid == 0x0000fe7cul) append_text(gatts, gatts_length, "Telit Wireless Solutions (Formerly Stollmann E+V GmbH)");
        else if (ble_uuid == 0x0000fe7dul) append_text(gatts, gatts_length, "Aterica Health Inc.");
        else if (ble_uuid == 0x0000fe7eul) append_text(gatts, gatts_length, "Awear Solutions Ltd");
        else if (ble_uuid == 0x0000fe7ful) append_text(gatts, gatts_length, "Doppler Lab");
        else if (ble_uuid == 0x0000fe80ul) append_text(gatts, gatts_length, "Doppler Lab");
        else if (ble_uuid == 0x0000fe81ul) append_text(gatts, gatts_length, "Medtronic Inc.");
        else if (ble_uuid == 0x0000fe82ul) append_text(gatts, gatts_length, "Medtronic Inc.");
        else if (ble_uuid == 0x0000fe83ul) append_text(gatts, gatts_length, "Blue Bite");
        else if (ble_uuid == 0x0000fe84ul) append_text(gatts, gatts_length, "RF Digital Corp");
        else if (ble_uuid == 0x0000fe85ul) append_text(gatts, gatts_length, "RF Digital Corp");
        else if (ble_uuid == 0x0000fe86ul) append_text(gatts, gatts_length, "HUAWEI Technologies Co., Ltd. ( )");
        else if (ble_uuid == 0x0000fe87ul) append_text(gatts, gatts_length, "Qingdao Yeelink Information Technology Co., Ltd. ( )");
        else if (ble_uuid == 0x0000fe88ul) append_text(gatts, gatts_length, "SALTO SYSTEMS S.L.");
        else if (ble_uuid == 0x0000fe89ul) append_text(gatts, gatts_length, "B&O Play A/S");
        else if (ble_uuid == 0x0000fe8aul) append_text(gatts, gatts_length, "Apple, Inc.");
        else if (ble_uuid == 0x0000fe8bul) append_text(gatts, gatts_length, "Apple, Inc.");
        else if (ble_uuid == 0x0000fe8cul) append_text(gatts, gatts_length, "TRON Forum");
        else if (ble_uuid == 0x0000fe8dul) append_text(gatts, gatts_length, "Interaxon Inc.");
        else if (ble_uuid == 0x0000fe8eul) append_text(gatts, gatts_length, "ARM Ltd");
        else if (ble_uuid == 0x0000fe8ful) append_text(gatts, gatts_length, "CSR");
        else if (ble_uuid == 0x0000fe90ul) append_text(gatts, gatts_length, "JUMA");
        else if (ble_uuid == 0x0000fe91ul) append_text(gatts, gatts_length, "Shanghai Imilab Technology Co.,Ltd");
        else if (ble_uuid == 0x0000fe92ul) append_text(gatts, gatts_length, "Jarden Safety & Security");
        else if (ble_uuid == 0x0000fe93ul) append_text(gatts, gatts_length, "OttoQ Inc.");
        else if (ble_uuid == 0x0000fe94ul) append_text(gatts, gatts_length, "OttoQ Inc.");
        else if (ble_uuid == 0x0000fe95ul) append_text(gatts, gatts_length, "Xiaomi Inc.");
        else if (ble_uuid == 0x0000fe96ul) append_text(gatts, gatts_length, "Tesla Motor Inc.");
        else if (ble_uuid == 0x0000fe97ul) append_text(gatts, gatts_length, "Tesla Motor Inc.");
        else if (ble_uuid == 0x0000fe98ul) append_text(gatts, gatts_length, "Currant, Inc.");
        else if (ble_uuid == 0x0000fe99ul) append_text(gatts, gatts_length, "Currant, Inc.");
        else if (ble_uuid == 0x0000fe9aul) append_text(gatts, gatts_length, "Estimote");
        else if (ble_uuid == 0x0000fe9bul) append_text(gatts, gatts_length, "Samsara Networks, Inc");
        else if (ble_uuid == 0x0000fe9cul) append_text(gatts, gatts_length, "GSI Laboratories, Inc.");
        else if (ble_uuid == 0x0000fe9dul) append_text(gatts, gatts_length, "Mobiquity Networks Inc");
        else if (ble_uuid == 0x0000fe9eul) append_text(gatts, gatts_length, "Dialog Semiconductor B.V.");
        else if (ble_uuid == 0x0000fe9ful) append_text(gatts, gatts_length, "Google Inc.");
        else if (ble_uuid == 0x0000fea0ul) append_text(gatts, gatts_length, "Google Inc.");
        else if (ble_uuid == 0x0000fea1ul) append_text(gatts, gatts_length, "Intrepid Control Systems, Inc.");
        else if (ble_uuid == 0x0000fea2ul) append_text(gatts, gatts_length, "Intrepid Control Systems, Inc.");
        else if (ble_uuid == 0x0000fea3ul) append_text(gatts, gatts_length, "ITT Industries");
        else if (ble_uuid == 0x0000fea4ul) append_text(gatts, gatts_length, "Paxton Access Ltd");
        else if (ble_uuid == 0x0000fea5ul) append_text(gatts, gatts_length, "GoPro, Inc.");
        else if (ble_uuid == 0x0000fea6ul) append_text(gatts, gatts_length, "GoPro, Inc.");
        else if (ble_uuid == 0x0000fea7ul) append_text(gatts, gatts_length, "UTC Fire and Security");
        else if (ble_uuid == 0x0000fea8ul) append_text(gatts, gatts_length, "Savant Systems LLC");
        else if (ble_uuid == 0x0000fea9ul) append_text(gatts, gatts_length, "Savant Systems LLC");
        else if (ble_uuid == 0x0000feaaul) append_text(gatts, gatts_length, "Google Inc.");
        else if (ble_uuid == 0x0000feabul) append_text(gatts, gatts_length, "Nokia Corporation");
        else if (ble_uuid == 0x0000feacul) append_text(gatts, gatts_length, "Nokia Corporation");
        else if (ble_uuid == 0x0000feadul) append_text(gatts, gatts_length, "Nokia Corporation");
        else if (ble_uuid == 0x0000feaeul) append_text(gatts, gatts_length, "Nokia Corporation");
        else if (ble_uuid == 0x0000feaful) append_text(gatts, gatts_length, "Nest Labs Inc.");
        else if (ble_uuid == 0x0000feb0ul) append_text(gatts, gatts_length, "Nest Labs Inc.");
        else if (ble_uuid == 0x0000feb1ul) append_text(gatts, gatts_length, "Electronics Tomorrow Limited");
        else if (ble_uuid == 0x0000feb2ul) append_text(gatts, gatts_length, "Microsoft Corporation");
        else if (ble_uuid == 0x0000feb3ul) append_text(gatts, gatts_length, "Taobao");
        else if (ble_uuid == 0x0000feb4ul) append_text(gatts, gatts_length, "WiSilica Inc.");
        else if (ble_uuid == 0x0000feb5ul) append_text(gatts, gatts_length, "WiSilica Inc.");
        else if (ble_uuid == 0x0000feb6ul) append_text(gatts, gatts_length, "Vencer Co, Ltd");
        else if (ble_uuid == 0x0000feb7ul) append_text(gatts, gatts_length, "Facebook, Inc.");
        else if (ble_uuid == 0x0000feb8ul) append_text(gatts, gatts_length, "Facebook, Inc.");
        else if (ble_uuid == 0x0000feb9ul) append_text(gatts, gatts_length, "LG Electronics");
        else if (ble_uuid == 0x0000febaul) append_text(gatts, gatts_length, "Tencent Holdings Limited");
        else if (ble_uuid == 0x0000febbul) append_text(gatts, gatts_length, "adafruit industries");
        else if (ble_uuid == 0x0000febcul) append_text(gatts, gatts_length, "Dexcom, Inc.");
        else if (ble_uuid == 0x0000febdul) append_text(gatts, gatts_length, "Clover Network, Inc.");
        else if (ble_uuid == 0x0000febeul) {
            append_text(gatts, gatts_length, "Bose Corporation");
            soft_set_category(&device->category, CATEGORY_HEADPHONES);
        }
        else if (ble_uuid == 0x0000febful) append_text(gatts, gatts_length, "Nod, Inc.");
        else if (ble_uuid == 0x0000fec0ul) append_text(gatts, gatts_length, "KDDI Corporation");
        else if (ble_uuid == 0x0000fec1ul) append_text(gatts, gatts_length, "KDDI Corporation");
        else if (ble_uuid == 0x0000fec2ul) append_text(gatts, gatts_length, "Blue Spark Technologies, Inc.");
        else if (ble_uuid == 0x0000fec3ul) append_text(gatts, gatts_length, "360fly, Inc.");
        else if (ble_uuid == 0x0000fec4ul) append_text(gatts, gatts_length, "PLUS Location Systems");
        else if (ble_uuid == 0x0000fec5ul) append_text(gatts, gatts_length, "Realtek Semiconductor Corp.");
        else if (ble_uuid == 0x0000fec6ul) append_text(gatts, gatts_length, "Kocomojo, LLC");
        else if (ble_uuid == 0x0000fec7ul) append_text(gatts, gatts_length, "Apple, Inc.");
        else if (ble_uuid == 0x0000fec8ul) append_text(gatts, gatts_length, "Apple, Inc.");
        else if (ble_uuid == 0x0000fec9ul) append_text(gatts, gatts_length, "Apple, Inc.");
        else if (ble_uuid == 0x0000fecaul) append_text(gatts, gatts_length, "Apple, Inc.");
        else if (ble_uuid == 0x0000fecbul) append_text(gatts, gatts_length, "Apple, Inc.");
        else if (ble_uuid == 0x0000feccul) append_text(gatts, gatts_length, "Apple, Inc.");
        else if (ble_uuid == 0x0000fecdul) append_text(gatts, gatts_length, "Apple, Inc.");
        else if (ble_uuid == 0x0000feceul) append_text(gatts, gatts_length, "Apple, Inc.");
        else if (ble_uuid == 0x0000fecful) append_text(gatts, gatts_length, "Apple, Inc.");
        else if (ble_uuid == 0x0000fed0ul) append_text(gatts, gatts_length, "Apple, Inc.");
        else if (ble_uuid == 0x0000fed1ul) append_text(gatts, gatts_length, "Apple, Inc.");
        else if (ble_uuid == 0x0000fed2ul) append_text(gatts, gatts_length, "Apple, Inc.");
        else if (ble_uuid == 0x0000fed3ul) append_text(gatts, gatts_length, "Apple, Inc.");
        else if (ble_uuid == 0x0000fed4ul) append_text(gatts, gatts_length, "Apple, Inc.");
        else if (ble_uuid == 0x0000fed5ul) append_text(gatts, gatts_length, "Plantronics Inc.");
        else if (ble_uuid == 0x0000fed6ul) append_text(gatts, gatts_length, "Broadcom Corporation");
        else if (ble_uuid == 0x0000fed7ul) append_text(gatts, gatts_length, "Broadcom Corporation");
        else if (ble_uuid == 0x0000fed8ul) append_text(gatts, gatts_length, "Google Inc.");
        else if (ble_uuid == 0x0000fed9ul) append_text(gatts, gatts_length, "Pebble Technology Corporation");
        else if (ble_uuid == 0x0000fedaul) append_text(gatts, gatts_length, "ISSC Technologies Corporation");
        else if (ble_uuid == 0x0000fedbul) append_text(gatts, gatts_length, "Perka, Inc.");
        else if (ble_uuid == 0x0000fedcul) append_text(gatts, gatts_length, "Jawbone");
        else if (ble_uuid == 0x0000feddul) append_text(gatts, gatts_length, "Jawbone");
        else if (ble_uuid == 0x0000fedeul) append_text(gatts, gatts_length, "Coin, Inc.");
        else if (ble_uuid == 0x0000fedful) append_text(gatts, gatts_length, "Design SHIFT");
        else if (ble_uuid == 0x0000fee0ul) append_text(gatts, gatts_length, "Anhui Huami Information Technology Co.");
        else if (ble_uuid == 0x0000fee1ul) append_text(gatts, gatts_length, "Anhui Huami Information Technology Co.");
        else if (ble_uuid == 0x0000fee2ul) append_text(gatts, gatts_length, "Anki, Inc.");
        else if (ble_uuid == 0x0000fee3ul) append_text(gatts, gatts_length, "Anki, Inc.");
        else if (ble_uuid == 0x0000fee4ul) append_text(gatts, gatts_length, "Nordic Semiconductor ASA");
        else if (ble_uuid == 0x0000fee5ul) append_text(gatts, gatts_length, "Nordic Semiconductor ASA");
        else if (ble_uuid == 0x0000fee6ul) append_text(gatts, gatts_length, "Silvair, Inc.");
        else if (ble_uuid == 0x0000fee7ul) append_text(gatts, gatts_length, "Tencent Holdings Limited");
        else if (ble_uuid == 0x0000fee8ul) append_text(gatts, gatts_length, "Quintic Corp.");
        else if (ble_uuid == 0x0000fee9ul) append_text(gatts, gatts_length, "Quintic Corp.");
        else if (ble_uuid == 0x0000feeaul) append_text(gatts, gatts_length, "Swirl Networks, Inc.");
        else if (ble_uuid == 0x0000feebul) append_text(gatts, gatts_length, "Swirl Networks, Inc.");
        else if (ble_uuid == 0x0000feecul) append_text(gatts, gatts_length, "Tile, Inc.");
        else if (ble_uuid == 0x0000feedul) append_text(gatts, gatts_length, "Tile, Inc.");
        else if (ble_uuid == 0x0000feeeul) append_text(gatts, gatts_length, "Polar Electro Oy");
        else if (ble_uuid == 0x0000feeful) append_text(gatts, gatts_length, "Polar Electro Oy");
        else if (ble_uuid == 0x0000fef0ul) append_text(gatts, gatts_length, "Intel");
        else if (ble_uuid == 0x0000fef1ul) append_text(gatts, gatts_length, "CSR");
        else if (ble_uuid == 0x0000fef2ul) append_text(gatts, gatts_length, "CSR");
        else if (ble_uuid == 0x0000fef3ul) append_text(gatts, gatts_length, "Google Inc.");
        else if (ble_uuid == 0x0000fef4ul) append_text(gatts, gatts_length, "Google Inc.");
        else if (ble_uuid == 0x0000fef5ul) append_text(gatts, gatts_length, "Dialog Semiconductor GmbH");
        else if (ble_uuid == 0x0000fef6ul) append_text(gatts, gatts_length, "Wicentric, Inc.");
        else if (ble_uuid == 0x0000fef7ul) append_text(gatts, gatts_length, "Aplix Corporation");
        else if (ble_uuid == 0x0000fef8ul) append_text(gatts, gatts_length, "Aplix Corporation");
        else if (ble_uuid == 0x0000fef9ul) append_text(gatts, gatts_length, "PayPal, Inc.");
        else if (ble_uuid == 0x0000fefaul) append_text(gatts, gatts_length, "PayPal, Inc.");
        else if (ble_uuid == 0x0000fefbul) append_text(gatts, gatts_length, "Telit Wireless Solutions (Formerly Stollmann E+V GmbH)");
        else if (ble_uuid == 0x0000fefcul) append_text(gatts, gatts_length, "Gimbal, Inc.");
        else if (ble_uuid == 0x0000fefdul) append_text(gatts, gatts_length, "Gimbal, Inc.");
        else if (ble_uuid == 0x0000fefeul) append_text(gatts, gatts_length, "GN ReSound A/S");
        else if (ble_uuid == 0x0000fefful) append_text(gatts, gatts_length, "GN Netcom");

        // Apple Notification Center Service
        else if (ble_uuid == 0x7905f431ul) append_text(gatts, gatts_length, "Apple NCS, ");
        // Apple Media Service
        else if (ble_uuid == 0x89d3502bul) append_text(gatts, gatts_length, "Apple MS, ");
        else if (ble_uuid == 0x9fa480e0ul) append_text(gatts, gatts_length, "Apple XX, ");
        else if (ble_uuid == 0xabbaff00ul) {
            append_text(gatts, gatts_length, "Fitbit, ");
            soft_set_category(&device->category, CATEGORY_FITNESS);
        }
        else if (ble_uuid == 0xb9401000ul) {
            append_text(gatts, gatts_length, "Estimote 1, ");
            soft_set_category(&device->category, CATEGORY_BEACON);
        }
        else if (ble_uuid == 0xb9402000ul) {
            append_text(gatts, gatts_length, "Estimote 2,");
            soft_set_category(&device->category, CATEGORY_BEACON);
        }
        else if (ble_uuid == 0xb9403000ul) {
            append_text(gatts, gatts_length, "Estimote 3, ");
            soft_set_category(&device->category, CATEGORY_BEACON);
        }
        else if (ble_uuid == 0xb9404000ul) {
            append_text(gatts, gatts_length, "Estimote 4, ");
            soft_set_category(&device->category, CATEGORY_BEACON);
        }
        else if (ble_uuid == 0xb9405000ul) {
            append_text(gatts, gatts_length, "Estimote 5, ");
            soft_set_category(&device->category, CATEGORY_BEACON);
        }
        else if (ble_uuid == 0xb9406000ul) {
            append_text(gatts, gatts_length, "Estimote 6, ");
            soft_set_category(&device->category, CATEGORY_BEACON);
        }
        else if (ble_uuid == 0xa3e68a83ul) {
            append_text(gatts, gatts_length, "Milwaukee, ");
            soft_set_category(&device->category, CATEGORY_BEACON);
        }
        else if (ble_uuid == 0xc7f94713ul) append_text(gatts, gatts_length, "CDP, ");
        else if (ble_uuid == 0xd0611e78ul) 
        {
            append_text(gatts, gatts_length, "Continuity, ");
        }
        else if (ble_uuid == 0x0000fd6ful) 
        {
            append_text(gatts, gatts_length, "CovidTrace, ");
            set_name(device, "Covid Trace", nt_generic);
            soft_set_category(&device->category, CATEGORY_COVID);
        }
        else if (ble_uuid == 0x0000fe79ul) 
        {
            append_text(gatts, gatts_length, "Zebra, ");
        }
        else if (ble_uuid == 0xcbbfe0e1ul) {
            append_text(gatts, gatts_length, "ATT STB?, ");
            soft_set_category(&device->category, CATEGORY_TV);
        }
        else if (ble_uuid == 0x5918f000ul) {
            append_text(gatts, gatts_length, "Ezurio MTM, ");
            soft_set_category(&device->category, CATEGORY_FIXED);
        }
        else if (ble_uuid == 0x0000fef5ul) {
            append_text(gatts, gatts_length, "Notfound(0xfef5), ");
        }
        else if (ble_uuid == 0x00000100ul) {
            append_text(gatts, gatts_length, "Notfound(0x100), ");
        }
        else if (ble_uuid == 0x0000fe95ul) {
            append_text(gatts, gatts_length, "Notfound(0xfe95), ");
        }
        else if (ble_uuid == 0x00010203ul) {
            append_text(gatts, gatts_length, "Notfound(0x00010203), ");
        }
        else if (ble_uuid == 0xebe0ccb0ul) {
            append_text(gatts, gatts_length, "Notfound(0xebe0ccb0), ");
        }
        else
            append_text(gatts, gatts_length, "Unknown(%s), ", strCopy);

        g_free(strCopy);
    }
}
