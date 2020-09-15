#ifndef UTILITY_H
#define UTILITY_H
/*
    Utility functions
*/

#define G_LOG_USE_STRUCTURED 1
#include <glib.h>
#include <stdbool.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/types.h>

/*
 * Measures the current (and peak) resident and virtual memories
 * usage of your linux C process, in kB
 */
void getMemory(
    int* currRealMem, int* peakRealMem,
    int* currVirtMem, int* peakVirtMem);

char* trim(char *str);

void get_path_from_address(char* address, char* path, int pathLength);

bool get_address_from_path(char *address, int length, const char *path);

/*
  pretty_print a GVariant with a label including types
*/
void pretty_print2(const char *field_name, GVariant *value, gboolean types);

/*
  pretty_print a GVariant with a label
*/
void pretty_print(const char *field_name, GVariant *value);

/*
  Get a mac address from one of the interfaces
*/
void get_mac_address(char* access_point_address);

/*
   Is a given interface UP
*/
bool is_interface_up(const char* ifname);

/*
    Is any network interface up (from a list of common names)
*/
bool is_any_interface_up();

void mac_address_to_string(char* output, int length, char* access_point_address);

/*
   Mac address 64 bit to string
*/
void mac_64_to_string(char* output, int length, int64_t access_64);

/*
   Mac address string to 64 bit integer (top two bytes zero)
*/
int64_t mac_string_to_int_64 (char* mac);

/*
    Append text to buffer
*/
void append_text(char* buffer, int length, char* format, ...);

/*
  string starts with
*/
bool string_starts_with(char *buffer, char *match);

/*
  string contains
*/
bool string_contains(char *buffer, char *match);

/*
   Overwrite if it was empty or began with an underscore (temporary name)
*/
void optional_set(char* name, char* value, int max_length);

void soft_set_8(int8_t* field, int8_t field_new);

void soft_set_u16(uint16_t* field, uint16_t field_new);

#endif