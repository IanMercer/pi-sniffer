// heuristics

#include "device.h"
#include "utility.h"

void apply_name_heuristics (struct Device* existing, const char* name);

void handle_manufacturer(struct Device *existing, uint16_t manufacturer, unsigned char *allocdata);

void handle_apple(struct Device *existing, unsigned char *allocdata);

void handle_uuids(struct Device *existing, char *uuidArray[2048], int actualLength, char* gatts, int gatts_length);
