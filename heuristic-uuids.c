// UUID heuristics

#include "utility.h"
#include "device.h"
#include "heuristics.h"

#include <glib.h>
//#include <gio/gio.h>
#include <stdbool.h>
#include <stdio.h>
//#include <sys/ioctl.h>
#include <sys/types.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/types.h>
#include <string.h>

void handle_uuids(struct Device *existing, char *uuidArray[2048], int actualLength, char* gatts, int gatts_length)
{
    // Print off the UUIDs here
    for (int i = 0; i < actualLength; i++)
    {
        char *strCopy = strdup(uuidArray[i]);

        // All common BLE UUIDs are of the form: 0000XXXX-0000-1000-8000-00805f9b34fb
        // so we only need two hex bytes. But Apple and others use complete GUIDs
        // for their own services, so let's take the first four hex bytes
        strCopy[8] = '\0';
        int64_t ble_uuid = strtol(strCopy, NULL, 16);

        // https://www.bluetooth.com/specifications/gatt/characteristics/

        if (ble_uuid == 0x00001000ul) append_text(gatts, gatts_length, "ServiceDiscoveryServer, ");
        else if (ble_uuid == 0x00001001ul) append_text(gatts, gatts_length, "BrowseGroupDescriptor, ");
        else if (ble_uuid == 0x00001002ul) append_text(gatts, gatts_length, "PublicBrowseGroup, ");
        else if (ble_uuid == 0x00001816ul) append_text(gatts, gatts_length, "CyclingCadence, ");
        else if (ble_uuid == 0x00001818ul) 
        {
            append_text(gatts, gatts_length, "CyclingPower, ");
            soft_set_category(&existing->category, CATEGORY_WEARABLE);
        }
        else if (ble_uuid == 0x00001101ul) append_text(gatts, gatts_length, "SerialPort, ");
        else if (ble_uuid == 0x00001102ul) append_text(gatts, gatts_length, "LANAccessUsingPPP, ");
        else if (ble_uuid == 0x00001103ul) append_text(gatts, gatts_length, "DialupNetworking, ");
        else if (ble_uuid == 0x00001104ul) append_text(gatts, gatts_length, "IrMCSync, ");
        else if (ble_uuid == 0x00001105ul) append_text(gatts, gatts_length, "OBEXObjectPush, ");
        else if (ble_uuid == 0x00001106ul) append_text(gatts, gatts_length, "OBEXFileTransfer, ");
        else if (ble_uuid == 0x00001107ul) append_text(gatts, gatts_length, "IrMCSyncCommand, ");
        else if (ble_uuid == 0x00001108ul) append_text(gatts, gatts_length, "Headset, ");
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
        else if (ble_uuid == 0x0000111Eul) append_text(gatts, gatts_length, "Handsfree, ");
        else if (ble_uuid == 0x0000111Ful) append_text(gatts, gatts_length, "HandsfreeAudioGateway, ");
        else if (ble_uuid == 0x00001120ul) append_text(gatts, gatts_length, "DirectPrintingReferenceObjects, ");
        else if (ble_uuid == 0x00001121ul) append_text(gatts, gatts_length, "ReflectedUI, ");
        else if (ble_uuid == 0x00001122ul) append_text(gatts, gatts_length, "BasicPringing, ");
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
        else if (ble_uuid == 0x00002A37ul) append_text(gatts, gatts_length, "Heart rate measurement ");
        else if (ble_uuid == 0x0000feaaul) append_text(gatts, gatts_length, "Eddystone ");
        else if (ble_uuid == 0x0000ffa0ul) append_text(gatts, gatts_length, "Accelerometer, ");
        else if (ble_uuid == 0x0000ffe0ul) append_text(gatts, gatts_length, "Temperature, ");
        else if (ble_uuid == 0x0000fff0ul) append_text(gatts, gatts_length, "Blood F0, ");
        else if (ble_uuid == 0x0000fff1ul) append_text(gatts, gatts_length, "Blood F1, ");
        else if (ble_uuid == 0x0000fff2ul) append_text(gatts, gatts_length, "Blood F2, ");
        else if (ble_uuid == 0x4e72b490ul) append_text(gatts, gatts_length, "Alexa, ");
        else if (ble_uuid == 0x7905f431ul) append_text(gatts, gatts_length, "Apple NCS, ");
        else if (ble_uuid == 0x89d3502bul) append_text(gatts, gatts_length, "Apple MS, ");
        else if (ble_uuid == 0x9fa480e0ul) append_text(gatts, gatts_length, "Apple XX, ");
        else if (ble_uuid == 0xb9401000ul) append_text(gatts, gatts_length, "Estimote 1, ");
        else if (ble_uuid == 0xb9402000ul) append_text(gatts, gatts_length, "Estimote 2,");
        else if (ble_uuid == 0xb9403000ul) append_text(gatts, gatts_length, "Estimote 3, ");
        else if (ble_uuid == 0xb9404000ul) append_text(gatts, gatts_length, "Estimote 4, ");
        else if (ble_uuid == 0xb9405000ul) append_text(gatts, gatts_length, "Estimote 5, ");
        else if (ble_uuid == 0xb9406000ul) append_text(gatts, gatts_length, "Estimote 6, ");
        else if (ble_uuid == 0xc7f94713ul) append_text(gatts, gatts_length, "CDP, ");
        else if (ble_uuid == 0xd0611e78ul) append_text(gatts, gatts_length, "Continuity, ");
        else if (ble_uuid == 0xcbbfe0e1ul) {
            append_text(gatts, gatts_length, "ATT STB?, ");
            soft_set_category(&existing->category, CATEGORY_TV);
        }
        else if (ble_uuid == 0x5918f000ul) {
            append_text(gatts, gatts_length, "Ezurio MTM, ");
            soft_set_category(&existing->category, CATEGORY_FIXED);
        }
        else
            append_text(gatts, gatts_length, "Unknown(%s), ", strCopy);

        g_free(strCopy);
    }
}
