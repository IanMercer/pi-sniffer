// heuristics

#include "device.h"
#include "utility.h"

void apply_mac_address_heuristics (struct Device* device);

void apply_name_heuristics (struct Device* device, const char* name);

void handle_manufacturer(struct Device *device, uint16_t manufacturer, unsigned char *allocdata, uint32_t hash_for_one);

void handle_apple(struct Device *device, unsigned char *allocdata);

void handle_uuids(struct Device *device, char *uuidArray[2048], int actualLength, char* gatts, int gatts_length);

void handle_class(struct Device *device, uint32_t class);

void handle_icon(struct Device *existing, const char* icon);

void handle_appearance(struct Device *existing, const uint16_t appearance);

/*
    Database of Bluetooth company Ids (see Bluetooth website)
*/
const char *company_id_to_string(int company_id, int8_t* category);
