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
    int len = 0;
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
    endp = str + len - 1;

    /* Move the front and back pointers to address the first non-whitespace
     * characters from each end.
     */
    while (*frontp == ' ')
    {
        ++frontp;
    }

    while (*endp == ' ' && endp > frontp)
    {
        endp--;
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
        if (*frontp < ' '){
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


/*
  url_slug removes spaces and other bad characters
*/
char* url_slug(char *str)
{
    char *src = str;
    char *dest = str;

    if (str == NULL)
    {
        return NULL;
    }

    while (*src != '\0')
    {
        if ((*src >= '0' && *src <='9') || (*src >= 'A' && *src <='Z') || (*src >= 'a' && *src <='z') || (*src == '_'))
        {
            *dest = *src;
            ++dest;
        }
        ++src;
    }
    *dest = '\0';

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
    if (length != 18) g_error("Length must be 18");
    snprintf(output, length, "%.2X:%.2X:%.2X:%.2X:%.2X:%.2X", access_point_address[0], access_point_address[1],
        access_point_address[2], access_point_address[3], access_point_address[4], access_point_address[5]);
    output[18] = '\0';
}

/*
   Mac address 64 bit to string
*/
void mac_64_to_string(char* output, int length, int64_t access_64)
{
    if (length != 18) g_error("Length must be 18");
    snprintf(output, length, "%.2X:%.2X:%.2X:%.2X:%.2X:%.2X",
        (int8_t)(access_64 >> 40) & 0xff,
        (int8_t)(access_64 >> 32) & 0xff,
        (int8_t)(access_64 >> 24) & 0xff,
        (int8_t)(access_64 >> 16) & 0xff,
        (int8_t)(access_64 >> 8) & 0xff,
        (int8_t)(access_64 >> 0) & 0xff);
    output[17] = '\0';
}

/*
* What's the value of this hex digit
*/
int hex_char_to_value(char let)
{
    if (let >= '0' && let <= '9') return (let - '0');
    if (let >= 'a' && let <= 'f') return (let - 'a' + 10);
    if (let >= 'A' && let <= 'F') return (let - 'A' + 10);
    return -1;
}

/*
* Is this a mac address
*/
bool is_mac(char* mac)
{
    int len = strlen(mac);  // Should be 6 * 3 - 1  = 17
    if (len != 17) return false;
    for (int i = 0; i < 6; i++)
    {
        if (hex_char_to_value(mac[i*3]) < 0) return false;
        if (hex_char_to_value(mac[i*3+1]) < 0) return false;
        // ignore separators
    }
    return true;
}

/*
   Mac address string to 64 bit integer (top two bytes zero)
*/
int64_t mac_string_to_int_64 (char* mac){
    int64_t r=0;
    int len = strlen(mac);  // Should be 5 + 6*2 = 17
    if (strcmp(mac,"<random>")==0) return 0;
    if (strcmp(mac,"<any>")==0) return 0;
    if (strcmp(mac,"<pub>")==0) return 0;
    if (strcmp(mac,"any")==0) return 0;
    if (len==0) return 0;
    if (len != 17) g_warning("Incorrect length mac address '%s'", mac);
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
    while (*pos != '\0' && ++count<length) pos++;
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
*/
bool string_starts_with(const char *buffer, const char *match)
{
  return strncasecmp(buffer, match, strlen(match)) == 0;
}

/*
  string ends with
*/
bool string_ends_with(const char *buffer, const char *match)
{
    int offset = strlen(buffer) - strlen(match);
    if (offset < 0) return FALSE;
    return strncasecmp(buffer + offset, match, strlen(match)) == 0;
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
void optional_set_alias(char* name, char* value, int max_length) {
  if (value == NULL) return;
  if (strlen(name) && name[0]!='_') return;  // already set to something not tempporary
  // TODO: user device->is_temp_name
  if (name[0] == '_' && strcmp(value, "_Beacon") == 0) return;  // don't overwrite with a less specific name
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
    if (!interface_state) g_debug("Start scanning interfaces");
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
        if (strncmp(ifa->ifa_name, "vir", 3) == 0) continue;      // virtual
        if (strncmp(ifa->ifa_name, "veth", 4) == 0) continue;     // virtual
        if (strncmp(ifa->ifa_name, "docker", 6) == 0) continue;   // virtual

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
            if (!interface_state)
            {
                // Log interfaces state if we are currently down (or starting)
                g_debug("%s is %i", ifa->ifa_name, isConnected);
            }
            if (isConnected) count_connected++;
            close(socketfd);
        }
    }

    freeifaddrs(ifaddr);

    if ((count_connected > 0) != interface_state)
    {
        g_info("Network connectivity %i out of %i interfaces connected", count_connected, count);
        interface_state = count_connected > 0;
    }
    return count_connected > 0;
}


/*
   free summary linked list
*/
void free_summary(struct summary** head)
{
   struct summary* tmp;

   while (*head != NULL)
    {
       tmp = *head;
       *head = (*head)->next;
       free(tmp);
    }
}

/*
   update_summary linked list
*/
void update_summary(struct summary** summary, const char* category, const char* extra, double phone_value, double tablet_value, double computer_value, double watch_value, double wearable_value, 
    double beacon_value, double covid_value, double other_value)
{
    for (struct summary* s = *summary; s != NULL; s = s->next)
    {
        if (strcmp(s->category, category) == 0)
        {
            s->phone_total += phone_value;
            s->tablet_total += tablet_value;
            s->computer_total += computer_value;
            s->watch_total += watch_value;
            s->wearable_total += wearable_value;
            s->beacon_total += beacon_value;
            s->covid_total += covid_value;
            s->other_total += other_value;
            return;
        }
    }

    struct summary* s = malloc(sizeof(struct summary));
    s->category = category;
    s->extra = extra;
    s->phone_total = phone_value;
    s->tablet_total = tablet_value;
    s->computer_total = computer_value;
    s->watch_total = watch_value;
    s->wearable_total = wearable_value;
    s->beacon_total = beacon_value;
    s->covid_total = covid_value;
    s->other_total = other_value;
    s->next = *summary;
    *summary = s;
}

/*
    Add a summary count of phones, watches, ... to a cJSON object
*/
void cJSON_AddSummary(cJSON * item, struct summary* s)
{
    if (s->phone_total > 0) cJSON_AddRounded(item, "phones", s->phone_total);
    if (s->watch_total > 0) cJSON_AddRounded(item, "watches", s->watch_total);
    if (s->wearable_total > 0) cJSON_AddRounded(item, "wearables", s->wearable_total);
    if (s->computer_total > 0) cJSON_AddRounded(item, "computers", s->computer_total);
    if (s->tablet_total > 0) cJSON_AddRounded(item, "tablets", s->tablet_total);
    if (s->beacon_total > 0) cJSON_AddRounded(item, "beacons", s->beacon_total);
    if (s->covid_total > 0) cJSON_AddRounded(item, "covid", s->covid_total);
    if (s->other_total > 0) cJSON_AddRounded(item, "other", s->other_total);
}

/*
*  Are there any values in this summary
*/
bool any_present(struct summary* s)
{
    return s->phone_total > 0 || s->watch_total > 0 || s->wearable_total > 0 || s->computer_total > 0 ||
           s->tablet_total > 0 || s->beacon_total > 0 || s->covid_total > 0 || s->other_total > 0;
}

/*
    Add a one decimal value to a JSON object
*/
void cJSON_AddRounded(cJSON * item, const char* label, double value)
{
    char print_num[18];
    snprintf(print_num, 18, "%.1f", value);
    cJSON_AddRawToObject(item, label, print_num);
}

/*
    Add a two decimals value to a JSON object
*/
void cJSON_AddRounded2(cJSON * item, const char* label, double value)
{
    char print_num[19];
    snprintf(print_num, 18, "%.2f", value);
    cJSON_AddRawToObject(item, label, print_num);
}

/*
    Add a three decimals value to a JSON object
*/
void cJSON_AddRounded3(cJSON * item, const char* label, double value)
{
    char print_num[20];
    snprintf(print_num, 18, "%.3f", value);
    cJSON_AddRawToObject(item, label, print_num);
}

/*
   Get an unsigned integer parameter from the environment or default
*/
void get_uint16_env(const char* env, uint16_t* value, uint16_t default_value)
{
    const char *s = getenv(env);
    *value = (s != NULL) ? atoi(s) : default_value;
}

/*
   Get an integer parameter from the environment or default
*/
void get_int_env(const char* env, int* value, int default_value)
{
    const char *s = getenv(env);
    *value = (s != NULL) ? atoi(s) : default_value;
}

/*
   Get a float parameter from the environment or default
*/
void get_float_env(const char* env, float* value, float default_value)
{
    const char *s = getenv(env);
    *value = (s != NULL) ? strtof(s, NULL) : default_value;
}

/*
   Get a string parameter from the environment or default
*/
void get_string_env(const char* env, char** value, char* default_value)
{
    char *s = getenv(env);
    *value = (s != NULL) ? s : default_value;
}


/*
    print_and_free_error
*/
void print_and_free_error(GError *error) 
{
  if (error)
  {
       g_print("Error: %s\n", error->message);
       g_error_free (error);
  }
}


/*
   Read byte array and compute hash from GVariant
*/
unsigned char *read_byte_array(GVariant *s_value, int *actualLength, uint16_t *hash)
{
    unsigned char byteArray[2048];
    int len = 0;

    GVariantIter *iter_array;
    guchar str;

    //g_debug("START read_byte_array\n");

    g_variant_get(s_value, "ay", &iter_array);

    while (g_variant_iter_loop(iter_array, "y", &str))
    {
        byteArray[len++] = str;
    }

    g_variant_iter_free(iter_array);

    unsigned char *allocdata = g_malloc(len);
    memcpy(allocdata, byteArray, len);

    for (int i = 0; i < len; i++)
    {
        *hash = (*hash <<5) + *hash + allocdata[i];
    }

    *actualLength = len;

    //g_debug("END read_byte_array\n");
    return allocdata;
}

/*
* Simple hash for string
*/
uint32_t hash_string(const char* input, int maxlen)
{
    uint32_t r = 0;
    while (maxlen-- > 0 && *input++ != '\0')
    {
        r = r * 37 + *input;
    }

    return r;
}


/*
* Read all lines in a JSONL file, ignore comments, call back for each line
*/
bool read_all_lines (const char * dirname, const char* filename, void (*call_back) (const char* line, void* state),
    void* state)
{
	g_return_val_if_fail (filename != NULL, FALSE);
	g_return_val_if_fail (dirname != NULL, FALSE);

    if (filename[0] == '/') filename++;

    bool delimited = (dirname[strlen(dirname)-1] == G_DIR_SEPARATOR);
    
    char fullpath[128];
    g_snprintf(fullpath, sizeof(fullpath), "%s%s%s", dirname, delimited ? "" : G_DIR_SEPARATOR_S, filename);

    g_debug("Read all lines %s", fullpath);

    GFile *file = g_file_new_for_path (fullpath);

    if (!G_IS_FILE(file)) {
        g_object_unref(file);
        return FALSE;
    }

    GError **error = NULL;
	GError *error_local = NULL;

	GFileInputStream* is = g_file_read (file, NULL, &error_local);
	if (is == NULL) {
		g_propagate_error (error, error_local);
        if (error_local->message == NULL) 
        {
            g_warning("Could not open file %s", fullpath);
        }
        else 
        {
            g_warning("Could not open file %s: %s", fullpath, error_local->message);
        }
        g_clear_error(&error_local);  // Free the error before returning
        g_object_unref(file);
        return FALSE;
	}

	GDataInputStream * input = g_data_input_stream_new (G_INPUT_STREAM (is));

	/* read file line by line */
    int line_count = 0;

	while (TRUE) {
		gchar *line;
        gsize length;
		line = g_data_input_stream_read_line (input, &length, NULL, NULL);
		if (line == NULL)
			break;

        line_count++;

        // no text on line
        if (length < 1) 
        {
            g_free(line); 
            continue;
        }

        // Skip comment lines
        if (string_starts_with(line, "#")) { g_free(line); continue; }

        trim(line);
        if (strlen(line) > 0)
        {
            //g_debug("%s", line);
            call_back(line, state);
        }
        g_free(line);
	}

    // close stream
    g_input_stream_close(G_INPUT_STREAM(is), NULL, &error_local);

    if (error_local) {
        g_warning("Error closing stream: %s", error_local->message);
        g_clear_error(&error_local);
    }
    g_object_unref(input);
    g_object_unref(is);
    g_object_unref(file);
	return TRUE;
}

// Returns the temperature of the main chip (or zero if it failed)
double get_internal_temp()
{
  const int BUFFER_SIZE = 100;
  char data[BUFFER_SIZE];
  FILE *finput;
  double temp = 0;
  finput = fopen("/sys/class/thermal/thermal_zone0/temp","r");
  if (finput != NULL) {
    memset(data,0,BUFFER_SIZE);
    //size_t bytes_read = 
    fread(data,BUFFER_SIZE,1,finput);
    temp = atoi(data);
    temp /= 1000;
    fclose(finput);
  }
  return temp;
}

