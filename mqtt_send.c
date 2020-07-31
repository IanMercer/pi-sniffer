/*
    MQTT code to send BLE status
    And utility functions
*/

#include "mqtt_send.h"

#include <glib.h>
#include <gio/gio.h>
#include <stdbool.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <net/if.h>
#include <time.h>
#include <math.h>
#include <signal.h>
#include <stdlib.h>

const char *topicRoot = "BLF";
static char access_point_address[6];

static void publish_callback(void **unused, struct mqtt_response_publish *published)
{
    (void)unused;
    /* note that published->topic_name is NOT null-terminated (here we'll change it to a c-string) */
    char *topic_name = (char *)malloc(published->topic_name_size + 1);
    memcpy(topic_name, published->topic_name, published->topic_name_size);
    topic_name[published->topic_name_size] = '\0';

    printf("Received publish('%s'): %s\n", topic_name, (const char *)published->application_message);

    g_free(topic_name);
}

void exit_mqtt(int status, int sockfd)
{
    if (sockfd != -1) close(sockfd);
    //return bt_shell_noninteractive_quit(EXIT_FAILURE);
    exit(status);
}

uint8_t sendbuf[4 * 1024 * 1024]; /* 4MByte sendbuf should be large enough to hold multiple whole mqtt messages */
uint8_t recvbuf[2048];            /* recvbuf should be large enough any whole mqtt message expected to be received */

/* Ensure we have a clean session */
uint8_t connect_flags = MQTT_CONNECT_CLEAN_SESSION;

void prepare_mqtt(char *mqtt_addr, char *mqtt_port, char* client_id, char* mac_address)
{
    memcpy(&access_point_address, mac_address, 6);

    if (mqtt_addr == NULL) {
      printf("MQTT Address must be set");
      exit(-24);
    }

    if (mqtt_port == NULL) {
      printf("MQTT Port must be set");
      exit(-24);
    }

    if (client_id == NULL) {
      printf("Client ID must be set");
      exit(-24);
    }

    printf("Starting MQTT %s:%s client_id=%s\n", mqtt_addr, mqtt_port, client_id);

    /* open the non-blocking TCP socket (connecting to the broker) */
    sockfd = open_nb_socket(mqtt_addr, mqtt_port);
    if (sockfd == -1)
    {
        perror("Failed to open socket: ");
        exit_mqtt(EXIT_FAILURE, sockfd);
    }

    printf("Opened socket\n");

    /* setup a client */
    mqtt_init(&mqtt, sockfd, sendbuf, sizeof(sendbuf), recvbuf, sizeof(recvbuf), publish_callback);
    //mqtt_connect(&mqtt, "publishing_client", NULL, NULL, 0, NULL, NULL, 0, 400);

    /* Send connection request to the broker. */
    mqtt_connect(&mqtt, client_id, NULL, NULL, 0, NULL, NULL, connect_flags, 400);

    /* check that we don't have any errors */
    if (mqtt.error != MQTT_OK)
    {
        fprintf(stderr, "\n\nERROR: MQTT STARTUP CONNECT FAILED:%s\n", mqtt_error_str(mqtt.error));
        exit_mqtt(EXIT_FAILURE, sockfd);
    }

}

void send_to_mqtt_null(char *mac_address, char *key)
{
    /* Create topic including mac address */
    char topic[256];
    snprintf(topic, sizeof(topic), "%s/%s/%s", topicRoot, mac_address, key);

    printf("MQTT %s %s\n", topicRoot, "NULL");

    mqtt_publish(&mqtt, topic, "", 0, MQTT_PUBLISH_QOS_0); // | MQTT_PUBLISH_RETAIN);

    /* check for errors */
    if (mqtt.error != MQTT_OK)
    {
        fprintf(stderr, "\n\nERROR MQTT Send: %s\n", mqtt_error_str(mqtt.error));
        exit_mqtt(EXIT_FAILURE, sockfd);
    }
}


/* SEND TO MQTT WITH ACCESS POINT MAC ADDRESS AND TIME STAMP */

static int send_errors = 0;
static uint16_t sequence = 0;


void send_to_mqtt_with_time_and_mac(char *mac_address, char *key, int i, char *value, int value_length, int flags)
{
    //        g_print("send_to_mqtt_with_time_and_mac\n");

    /* Create topic including mac address */
    char topic[256];

    if (i < 0)
    {
        snprintf(topic, sizeof(topic), "%s/%s/%s", topicRoot, mac_address, key);
    }
    else
    {
        snprintf(topic, sizeof(topic), "%s/%s/%s/%d", topicRoot, mac_address, key, i);
    }

    // Add time and access point mac address to packet
    //  00-05  access_point_address
    //  06-13  time
    //  14-..  data
    char packet[2048];

    memset(packet, 0, 14 + 20);

    time_t now = time(0);

    memcpy(&packet[00], &access_point_address, 6);
    memcpy(&packet[06], &now, 8);

    sequence++;
    //memcpy(&packet[14], &sequence, 2);

    memcpy(&packet[14], value, value_length);
    int packet_length = value_length + 14;

    // MQTT PUBLISH APPEARS TO BE TAKING UP TO 4 MINUTES!!
    mqtt_publish(&mqtt, topic, packet, packet_length, flags);


    time_t end_t = time(0);
    int diff = (int)difftime(end_t, now);
    if (diff > 0) g_print("MQTT execution time = %is\n", diff);

    /* check for errors */
    if (mqtt.error != MQTT_OK)
    {
        fprintf(stderr, "\n\nERROR Send w time and mac: %s\n", mqtt_error_str(mqtt.error));

        send_errors ++;
        if (send_errors > 10) {
          g_print("\n\nToo many send errors, restarting\n\n");
          exit_mqtt(EXIT_FAILURE, sockfd);
        }
    }

    /*
	int s;
        g_print(" ");

	for(s = 0; s < packet_length; s++ )
	{
	    g_print("%.2X", (unsigned char)packet[s]);
            if (s == 6 || s == 6+8) g_print(" ");
	}
        g_print("\n");
*/
}

void send_to_mqtt_single(char *mac_address, char *key, char *value)
{
    printf("MQTT %s/%s/%s %s\n", topicRoot, mac_address, key, value);
    send_to_mqtt_with_time_and_mac(mac_address, key, -1, value, strlen(value) + 1, MQTT_PUBLISH_QOS_1 | MQTT_PUBLISH_RETAIN);
}

void send_to_mqtt_array(char *mac_address, char *key, unsigned char *value, int length)
{
    printf("MQTT %s/%s/%s bytes[%d]\n", topicRoot, mac_address, key, length);
    send_to_mqtt_with_time_and_mac(mac_address, key, -1, (char *)value, length, MQTT_PUBLISH_QOS_1 | MQTT_PUBLISH_RETAIN);
}

void send_to_mqtt_uuids(char *mac_address, char *key, char **uuids, int length)
{
    if (uuids == NULL)
    {
        printf("MQTT null UUIDs to send\n");
        return;
    }

    if (length < 1)
    {
        printf("MQTT zero UUIDs to send\n");
        return;
    }

    for (int i = 0; i < length; i++)
    {
        char *uuid = uuids[i];
        printf("MQTT %s/%s/%s/%d uuid[%d]\n", topicRoot, mac_address, key, i, (int)strlen(uuid));
        send_to_mqtt_with_time_and_mac(mac_address, key, i, uuid, strlen(uuid) + 1, MQTT_PUBLISH_QOS_1 | MQTT_PUBLISH_RETAIN);
        if (i < length-1) printf("    ");
    }
}

// numeric values that change all the time not retained, all others retained by MQTT

void send_to_mqtt_single_value(char *mac_address, char *key, int32_t value)
{
    char rssi[12];
    snprintf(rssi, sizeof(rssi), "%i", value);
    printf("MQTT %s/%s/%s %s\n", topicRoot, mac_address, key, rssi);
    send_to_mqtt_with_time_and_mac(mac_address, key, -1, rssi, strlen(rssi) + 1, MQTT_PUBLISH_QOS_1);
}

void send_to_mqtt_single_value_keep(char *mac_address, char *key, int32_t value)
{
    char rssi[12];
    snprintf(rssi, sizeof(rssi), "%i", value);
    printf("MQTT %s/%s/%s %s\n", topicRoot, mac_address, key, rssi);
    send_to_mqtt_with_time_and_mac(mac_address, key, -1, rssi, strlen(rssi) + 1, MQTT_PUBLISH_QOS_1 | MQTT_PUBLISH_RETAIN);
}

void send_to_mqtt_single_float(char *mac_address, char *key, float value)
{
    char rssi[12];
    snprintf(rssi, sizeof(rssi), "%.3f", value);
    printf("MQTT %s/%s/%s %s\n", topicRoot, mac_address, key, rssi);
    send_to_mqtt_with_time_and_mac(mac_address, key, -1, rssi, strlen(rssi) + 1, MQTT_PUBLISH_QOS_1);
}
