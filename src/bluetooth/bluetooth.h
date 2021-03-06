#ifndef BLUETOOTH_H
#define BLUETOOTH_H

#include <glib.h>
#include <gio/gio.h>
#include <stdbool.h>
#include <stdio.h>
#include "utility.h"

#define 	BLE_APPEARANCE_UNKNOWN   0
#define 	BLE_APPEARANCE_GENERIC_PHONE   64
#define 	BLE_APPEARANCE_GENERIC_COMPUTER   128
#define 	BLE_APPEARANCE_GENERIC_WATCH   192
#define 	BLE_APPEARANCE_WATCH_SPORTS_WATCH   193
#define 	BLE_APPEARANCE_GENERIC_CLOCK   256
#define 	BLE_APPEARANCE_GENERIC_DISPLAY   320
#define 	BLE_APPEARANCE_GENERIC_REMOTE_CONTROL   384
#define 	BLE_APPEARANCE_GENERIC_EYE_GLASSES   448
#define 	BLE_APPEARANCE_GENERIC_TAG   512
#define 	BLE_APPEARANCE_GENERIC_KEYRING   576
#define 	BLE_APPEARANCE_GENERIC_MEDIA_PLAYER   640
#define 	BLE_APPEARANCE_GENERIC_BARCODE_SCANNER   704
#define 	BLE_APPEARANCE_GENERIC_THERMOMETER   768
#define 	BLE_APPEARANCE_THERMOMETER_EAR   769
#define 	BLE_APPEARANCE_GENERIC_HEART_RATE_SENSOR   832
#define 	BLE_APPEARANCE_HEART_RATE_SENSOR_HEART_RATE_BELT   833
#define 	BLE_APPEARANCE_GENERIC_BLOOD_PRESSURE   896
#define 	BLE_APPEARANCE_BLOOD_PRESSURE_ARM   897
#define 	BLE_APPEARANCE_BLOOD_PRESSURE_WRIST   898
#define 	BLE_APPEARANCE_GENERIC_HID   960
#define 	BLE_APPEARANCE_HID_KEYBOARD   961
#define 	BLE_APPEARANCE_HID_MOUSE   962
#define 	BLE_APPEARANCE_HID_JOYSTICK   963
#define 	BLE_APPEARANCE_HID_GAMEPAD   964
#define 	BLE_APPEARANCE_HID_DIGITIZERSUBTYPE   965
#define 	BLE_APPEARANCE_HID_CARD_READER   966
#define 	BLE_APPEARANCE_HID_DIGITAL_PEN   967
#define 	BLE_APPEARANCE_HID_BARCODE   968
#define 	BLE_APPEARANCE_GENERIC_GLUCOSE_METER   1024
#define 	BLE_APPEARANCE_GENERIC_RUNNING_WALKING_SENSOR   1088
#define 	BLE_APPEARANCE_RUNNING_WALKING_SENSOR_IN_SHOE   1089
#define 	BLE_APPEARANCE_RUNNING_WALKING_SENSOR_ON_SHOE   1090
#define 	BLE_APPEARANCE_RUNNING_WALKING_SENSOR_ON_HIP   1091
#define 	BLE_APPEARANCE_GENERIC_CYCLING   1152
#define 	BLE_APPEARANCE_CYCLING_CYCLING_COMPUTER   1153
#define 	BLE_APPEARANCE_CYCLING_SPEED_SENSOR   1154
#define 	BLE_APPEARANCE_CYCLING_CADENCE_SENSOR   1155
#define 	BLE_APPEARANCE_CYCLING_POWER_SENSOR   1156
#define 	BLE_APPEARANCE_CYCLING_SPEED_CADENCE_SENSOR   1157
#define 	BLE_APPEARANCE_GENERIC_PULSE_OXIMETER   3136
#define 	BLE_APPEARANCE_PULSE_OXIMETER_FINGERTIP   3137
#define 	BLE_APPEARANCE_PULSE_OXIMETER_WRIST_WORN   3138
#define 	BLE_APPEARANCE_GENERIC_WEIGHT_SCALE   3200
#define 	BLE_APPEARANCE_GENERIC_OUTDOOR_SPORTS_ACT   5184
#define 	BLE_APPEARANCE_OUTDOOR_SPORTS_ACT_LOC_DISP   5185
#define 	BLE_APPEARANCE_OUTDOOR_SPORTS_ACT_LOC_AND_NAV_DISP   5186
#define 	BLE_APPEARANCE_OUTDOOR_SPORTS_ACT_LOC_POD   5187
#define 	BLE_APPEARANCE_OUTDOOR_SPORTS_ACT_LOC_AND_NAV_POD   5188

// All BLE Guids are of the form: 0000XXXX-0000-1000-8000-00805f9b34fb
// XXXX values:

#define         BLE_GUID_HEART_RATE_SERVICE 0x180d
#define         BLE_GUID_DEVICE_INFORMATION_SERVICE 0x180a
#define         BLE_GUID_MANUFACTURER_NAME_STRING 0x2a29

typedef void (*method_cb_t)(GObject *, GAsyncResult *, gpointer);

int bluez_set_discovery_filter(GDBusConnection *conn);

int bluez_adapter_connect_device(GDBusConnection *conn, char *address);

int bluez_adapter_disconnect_device(GDBusConnection *conn, char *address);

int bluez_device_call_method_address(GDBusConnection *conn, const char *method, char* address, GVariant *param, method_cb_t method_cb);

int bluez_adapter_call_method(GDBusConnection *conn, const char *method, GVariant *param, method_cb_t method_cb);

void bluez_get_discovery_filter_cb(GObject *conn, GAsyncResult *res, gpointer data);

int bluez_adapter_set_property(GDBusConnection *conn, const char *prop, GVariant *value);

/*
    bluez_remove_device
 */
int bluez_remove_device(GDBusConnection *conn, char address[18]);

//typedef struct name_prefix {
//    int category;
//    char** items;
//};

extern char* terminals[];
extern char* phones[];
extern char* tablets[];
extern char* computers[];
extern char* wearables[];
extern char* headphones[];
extern char* tvs[];
extern char* printers[];
extern char* beacons[];
extern char* cars[];


#endif