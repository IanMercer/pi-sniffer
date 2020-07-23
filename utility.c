/*
    Utility functions
*/

#include <glib.h>
#include <gio/gio.h>
#include "mqtt.h"
#include "mqtt_pal.h"
#include "posix_sockets.h"
#include <stdbool.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <net/if.h>
#include <time.h>
#include <math.h>
#include <signal.h>


#include <stdio.h>
#include <stdlib.h>
#include <string.h>


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

char *trim(char *str)
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
static void pretty_print2(const char *field_name, GVariant *value, gboolean types)
{
    gchar *pretty = g_variant_print(value, types);
    g_print("%s %s\n", field_name, pretty);
    g_free(pretty);
}

static void pretty_print(const char *field_name, GVariant *value)
{
    gchar *pretty = g_variant_print(value, FALSE);
    g_print("%s %s\n", field_name, pretty);
    g_free(pretty);
}


// The mac address of the wlan0 interface (every Pi has one so fairly safe to assume wlan0 exists)

static char access_point_address[6];
static bool try_get_mac_address(const char* ifname);

static char controller_mac_address[13];
static char hostbuffer[256];

static void get_mac_address()
{
    gethostname(hostbuffer, sizeof(hostbuffer));

    g_print("Hostname %s\n", hostbuffer);
    client_id = hostbuffer;

    // Take first common interface name that returns a valid mac address
    if (try_get_mac_address("wlan0")) return;
    if (try_get_mac_address("eth0")) return;
    if (try_get_mac_address("enp4s0")) return;
    g_print("ERROR: Could not get local mac address\n");
    exit(-23);
}


static bool try_get_mac_address(const char* ifname)
{
    int s;
    struct ifreq buffer;

    s = socket(PF_INET, SOCK_DGRAM, 0);
    memset(&buffer, 0x00, sizeof(buffer));
    strcpy(buffer.ifr_name, ifname);
    ioctl(s, SIOCGIFHWADDR, &buffer);
    close(s);
    memcpy(&access_point_address, &buffer.ifr_hwaddr.sa_data, 6);
    if (access_point_address[0] == 0) return FALSE;

    snprintf(controller_mac_address, sizeof(controller_mac_address), "%.2x%.2x%.2x%.2x%.2x%.2x", access_point_address[0], access_point_address[1],
        access_point_address[2], access_point_address[3], access_point_address[4], access_point_address[5]);
    controller_mac_address[12] = '\0';

    // Use hostname instead ... client_id = controller_mac_address;

    g_print("Local MAC address for %s is: %s\n", ifname, controller_mac_address);
//    for (s = 0; s < 6; s++)
//    {
//        g_print("%.2X", (unsigned char)access_point_address[s]);
//    }
//    g_print("\n");
    return TRUE;
}

