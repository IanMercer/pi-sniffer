/*
    Utility functions
*/

#include <glib.h>
#include <gio/gio.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <net/if.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>

#define _GNU_SOURCE     /* To get defns of NI_MAXSERV and NI_MAXHOST */
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netdb.h>
#include <ifaddrs.h>
#include <unistd.h>
#include <linux/if_link.h>

#include "utility.h"

/*
 * Measures the current (and peak) resident and virtual memories
 * usage of your linux C process, in kB
 */
void getMemory(
    int* currRealMem, int* peakRealMem,
    int* currVirtMem, int* peakVirtMem) {

    // stores each word in status file
    char buffer[1024] = "";

    // linux file contains this-process info
    FILE* file = fopen("/proc/self/status", "r");

    // read the entire file
    while (fscanf(file, " %1023s", buffer) == 1) {

        if (strcmp(buffer, "VmRSS:") == 0) {
            fscanf(file, " %d", currRealMem);
        }
        if (strcmp(buffer, "VmHWM:") == 0) {
            fscanf(file, " %d", peakRealMem);
        }
        if (strcmp(buffer, "VmSize:") == 0) {
            fscanf(file, " %d", currVirtMem);
        }
        if (strcmp(buffer, "VmPeak:") == 0) {
            fscanf(file, " %d", peakVirtMem);
        }
    }
    fclose(file);
}

// trim string (https://stackoverflow.com/a/122974/224370 but simplified)

char* trim(char *str)
{
    size_t len = 0;
    char *frontp = str;
    char *endp = NULL;

    if (str == NULL)
    {
        return NULL;
    }
    if (str[0] == '\0')
    {
        return str;
    }

    len = strlen(str);
    endp = str + len;

    /* Move the front and back pointers to address the first non-whitespace
     * characters from each end.
     */
    while (*frontp == ' ')
    {
        ++frontp;
    }
    while (*(--endp) == ' ' && endp != frontp)
    {
    }

    if (endp <= frontp)
    {
        // All whitespace
        *str = '\0';
        return str;
    }

    *(endp + 1) = '\0'; // may already be zero

    /* Shift the string so that it starts at str so that if it's dynamically
     * allocated, we can still free it on the returned pointer.
     * Also remove any control characters
     */
    char *startp = str;
    while (*frontp)
    {
        if (*frontp < ' ' || *frontp > 'z'){
          frontp++;
          // remove the character instead, some trackers have bad characters in the name *startp++ = '_';
        }
        else {
          *startp++ = *frontp++;
        }
    }
    *startp = '\0';

    return str;
}


void get_path_from_address(char* address, char* path, int pathLength) {
   snprintf(path, pathLength, "/org/bluez/hci0/dev_%s", address);
   path[22] = '_';
   path[25] = '_';
   path[28] = '_';
   path[31] = '_';
   path[34] = '_';
}

// Get the MAC address from a BLUEZ path

bool get_address_from_path(char *address, int length, const char *path)
{

    if (path == NULL)
    {
        address[0] = '\0';
        return FALSE;
    }

    char *found = g_strstr_len(path, -1, "dev_");
    if (found == NULL)
    {
        address[0] = '\0';
        return FALSE;
    }

    int i;
    char *tmp = found + 4;

    address[length - 1] = '\0'; // safety

    for (i = 0; *tmp != '\0'; i++, tmp++)
    {
        if (i >= length - 1)
        {
            break;
        }
        if (*tmp == '_')
        {
            address[i] = ':';
        }
        else
        {
            address[i] = *tmp;
        }
    }
    return TRUE;
}

/*
  pretty_print a GVariant with a label
*/
void pretty_print2(const char *field_name, GVariant *value, gboolean types)
{
    gchar *pretty = g_variant_print(value, types);
    g_debug("%s: %s\n", field_name, pretty);
    g_free(pretty);
}

void pretty_print(const char *field_name, GVariant *value)
{
    gchar *pretty = g_variant_print(value, FALSE);
    g_debug("%s: %s\n", field_name, pretty);
    g_free(pretty);
}


// The mac address of the wlan0 interface (every Pi has one so fairly safe to assume wlan0 exists)

static bool try_get_mac_address(const char* ifname, char* access_point_address)
{
    int s;
    struct ifreq buffer;

    s = socket(PF_INET, SOCK_DGRAM, 0);
    memset(&buffer, 0x00, sizeof(buffer));
    strcpy(buffer.ifr_name, ifname);
    ioctl(s, SIOCGIFHWADDR, &buffer);

    close(s);
    memcpy(access_point_address, &buffer.ifr_hwaddr.sa_data, 6);
    if (access_point_address[0] == 0) return FALSE;
    return TRUE;
}




/*
   Get mac address (6 bytes) and string version (18 bytes)
*/
void get_mac_address(char* access_point_address)
{
    // Take first common interface name that returns a valid mac address
    if (try_get_mac_address("wlan0", access_point_address)) return;
    if (try_get_mac_address("eth0", access_point_address)) return;
    if (try_get_mac_address("enp4s0", access_point_address)) return;
    g_print("ERROR: Could not get local mac address\n");
    exit(-23);
}

/*
   Mac address bytes to string
*/
void mac_address_to_string(char* output, int length, char* access_point_address)
{
    snprintf(output, length, "%.2x%.2x%.2x%.2x%.2x%.2x", access_point_address[0], access_point_address[1],
        access_point_address[2], access_point_address[3], access_point_address[4], access_point_address[5]);
    output[12] = '\0';
}

/*
   Mac address 64 bit to string
*/
void mac_64_to_string(char* output, int length, int64_t access_64)
{
    snprintf(output, length, "%.2x%.2x%.2x%.2x%.2x%.2x", 
        (int8_t)(access_64 >> 40) & 0xff,
        (int8_t)(access_64 >> 32) & 0xff,
        (int8_t)(access_64 >> 24) & 0xff,
        (int8_t)(access_64 >> 16) & 0xff,
        (int8_t)(access_64 >> 8) & 0xff,
        (int8_t)(access_64 >> 0) & 0xff);
    output[12] = '\0';
}

/*
   Mac address string to 64 bit integer (top two bytes zero)
*/
int64_t mac_string_to_int_64 (char* mac){
    int64_t r=0;
    int len = strlen(mac);  // Should be 5 + 6*2 = 17
    if (len != 17) g_warning("Incorrect length mac address %s", mac);
    // Very lazy, assumes format is right: 00:00:00:00:00:00
    for(int i = 0; i < len; i++) {
        char let = mac[i];
        if (let >= '0' && let <= '9') { 
            r = (r << 4) + (let - '0');
        } else if (let >= 'a' && let <= 'f') { 
            r = (r << 4) + (let - 'a' + 10);
        } else if (let >= 'A' && let <= 'F') { 
            r = (r << 4) + (let - 'A' + 10);
        }
    } 
    return r;
}

/*
    Append text to buffer (assumes buffer is valid string at start)
*/
void append_text(char* buffer, int length, char* format, ...)
{
    char* pos = buffer;
    int count = 0;
    while (*pos != '\0' && count++<length) pos++;
    if (count < length)
    {
        va_list argptr;
        va_start(argptr, format);
        vsnprintf(pos, length-count, format, argptr);
        va_end(argptr);
    }
}

/*
  string starts with
  TODO: Make this insensitive
*/
bool string_starts_with(const char *buffer, const char *match)
{
  return strncmp(buffer, match, strlen(match)) == 0;
}

/*
  string ends with
  TODO: Make this insensitive
*/
bool string_endswith(const char *buffer, const char *match)
{
    int offset = strlen(buffer) - strlen(match);
    if (offset < 0) return FALSE;
    return strncmp(buffer + offset, match, strlen(match)) == 0;
}

/*
  string contains insensitive
*/
bool string_contains_insensitive(const char *buffer, const char *match)
{
  do {
    const char* h = buffer;
    const char* n = match;

    while (g_ascii_tolower((unsigned char) *h) == g_ascii_tolower((unsigned char ) *n) && *n) {
      h++;
      n++;
    }
    if (*n == 0) {
      return TRUE;
    }
  } while (*buffer++);
  return FALSE;
}

/*
   Overwrite if it was empty or began with an underscore (temporary name)
*/
void optional_set(char* name, char* value, int max_length) {
  if (strlen(name) && name[0]!='_') return;
  g_strlcpy(name, value, max_length);
}

void soft_set_8(int8_t* field, int8_t field_new)
{
    // CATEGORY_UNKNOWN, UNKNOWN_ADDRESS_TYPE = 0
    if (*field == 0) *field = field_new;
}

void soft_set_category(int8_t* field, int8_t field_new)
{
    // CATEGORY_UNKNOWN, UNKNOWN_ADDRESS_TYPE = 0
    if (*field == 0) *field = field_new;
}

void soft_set_u16(uint16_t* field, uint16_t field_new)
{
    // CATEGORY_UNKNOWN, UNKNOWN_ADDRESS_TYPE = 0
    if (*field == 0) *field = field_new;
}


bool interface_state = FALSE;

#define MAXHOST 5;

bool is_any_interface_up()
{
    int count = 0;
    int count_connected = 0;

    struct ifaddrs *ifaddr;
    struct ifaddrs *ifa;
    int family;

    if (getifaddrs(&ifaddr) == -1) {
        perror("getifaddrs");
        return FALSE;
    }

    for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {

        if (ifa->ifa_addr == NULL)
            continue;

        family = ifa->ifa_addr->sa_family;

        if (family != AF_INET) continue; // For IPV6 add && family != AF_INET6) continue;
        if (strncmp(ifa->ifa_name, "lo", 2) == 0) continue;       // loopback

        count++;
       
        // use getnameinfo to get address (not needed)

        // get flags to see if interface is UP and RUNNING
        struct ifreq    ifr;
        int socketfd = socket(AF_INET, SOCK_DGRAM, 0);
        if (socketfd != -1)
        {
            strncpy(ifr.ifr_ifrn.ifrn_name, ifa->ifa_name, IF_NAMESIZE);

            ioctl(socketfd, SIOCGIFFLAGS, &ifr);

            bool isConnected = ifr.ifr_ifru.ifru_flags & IFF_RUNNING;
            //g_debug("%s is %i", ifa->ifa_name, isConnected);
            if (isConnected) count_connected++;

            close(socketfd);
        }
    }

    freeifaddrs(ifaddr);

    if ((count_connected > 0) != interface_state)
    {
        g_warning("Network connectivity %i out of %i interfaces connected", count_connected, count);
        interface_state = count_connected > 0;
    }
    return count_connected > 0;
}

