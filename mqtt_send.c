/*
    MQTT code to send BLE status
    And utility functions
*/

#include "mqtt_send.h"
#include <MQTTClient.h>

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

#define CERTIFICATEFILE "IoTHubRootCA_Baltimore.pem"
#define MQTT_PUBLISH_QOS_0 0
#define MQTT_PUBLISH_QOS_1 1
#define MQTT_PUBLISH_QOS_2 2
#define MQTT_PUBLISH_DONT_RETAIN 0
#define MQTT_PUBLISH_RETAIN 1

const char *topicRoot = "BLF";
static char access_point_address[6];
static char* access_point_name;


//Callback functions
static void connect_callback(MQTTClient* mosq, void* obj, int result)
{
    (void)mosq;
    (void)obj;
    printf("Connect Callback returned : %i \r\n", result);
    if (result)
        printf("Connection Refused, please check your SAS Token, expired ?\r\n");
}

static void publish_callback(MQTTClient* mosq, void* userdata, int mid)
{
    (void)mosq;
    (void)userdata;
    (void)mid;
    g_print("Publish callback\n");
    ///* note that published->topic_name is NOT null-terminated (here we'll change it to a c-string) */
    //char *topic_name = (char *)malloc(published->topic_name_size + 1);
    //memcpy(topic_name, published->topic_name, published->topic_name_size);
    //topic_name[published->topic_name_size] = '\0';
    //printf("Received publish('%s'): %s\n", topic_name, (const char *)published->application_message);
    //g_free(topic_name);
}

void exit_mqtt(int status, int sockfd)
{
    if (sockfd != -1) close(sockfd);
    if (access_point_name) g_free(access_point_name);
    //return bt_shell_noninteractive_quit(EXIT_FAILURE);
    exit(status);
}

uint8_t sendbuf[4 * 1024 * 1024]; /* 4MByte sendbuf should be large enough to hold multiple whole mqtt messages */
uint8_t recvbuf[2048];            /* recvbuf should be large enough any whole mqtt message expected to be received */

static MQTTClient client;

void prepare_mqtt(char *mqtt_addr, char *mqtt_port, char* client_id, char* mac_address,
                  char* username, char* password)
{
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

    memcpy(&access_point_address, mac_address, 6);
    access_point_name = strdup(client_id);

    //char will_topic[256];
    //snprintf(will_topic, sizeof(will_topic), "%s/%s/%s", topicRoot, access_point_name, "state");

    //const char* will_message = "down";

    printf("Starting MQTT %s:%s client_id=%s\n", mqtt_addr, mqtt_port, client_id);
    printf("Username '%s'\n", username);
    printf("Password '%s'\n", password);

    MQTTClient_connectOptions opts = MQTTClient_connectOptions_initializer;

    opts.username = username;
    opts.password = password;
    opts.MQTTVersion = MQTTVERSION_DEFAULT;

    // TODO opts.will
        //opts.will = &wopts;
	//opts.will->message = "will message";
	//opts.will->qos = 1;
	//opts.will->retained = 0;
	//opts.will->topicName = "will topic";
	//opts.will = NULL;

    int rc;
    MQTTClient_create(&client, mqtt_addr, client_id, MQTTCLIENT_PERSISTENCE_DEFAULT, NULL);
    opts.keepAliveInterval = 20;
    opts.cleansession = 1;
    if ((rc = MQTTClient_connect(client, &opts)) != MQTTCLIENT_SUCCESS)
    {
        printf("Failed to connect, return code %d\n", rc);
        exit(-1);
    }

    MQTTClient_message pubmsg = MQTTClient_message_initializer;
    MQTTClient_deliveryToken token;
    pubmsg.payload = "up";
    pubmsg.payloadlen = strlen(pubmsg.payload)+1;
    pubmsg.qos = 0;
    pubmsg.retained = 0;

    MQTTClient_publishMessage(client, "BLF/tiger/status", &pubmsg, &token);
//    printf("Waiting for publication\n");
//    rc = MQTTClient_waitForCompletion(client, token, 10000);
//    printf("Message with delivery token %d delivered\n", token);
//    MQTTClient_disconnect(client, 10000);
//    MQTTClient_destroy(&client);
    //return rc;
}


void send_to_mqtt_null(char *mac_address, char *key)
{
    /* Create topic including mac address */
    char topic[256];
    snprintf(topic, sizeof(topic), "%s/%s/%s/%s", topicRoot, access_point_name, mac_address, key);

    printf("MQTT %s %s\n", topicRoot, "NULL");

    MQTTClient_message pubmsg = MQTTClient_message_initializer;
    pubmsg.payload = "";
    pubmsg.payloadlen = 0;
    pubmsg.qos = 0;
    pubmsg.retained = 0;

    MQTTClient_deliveryToken dt;
    int rc = MQTTClient_publishMessage(client, topic, &pubmsg, &dt);

 //   int rc = MQTTClient_waitForCompletion(client, dt, 10000);
 //   printf("Message with delivery token %d delivered\n", dt);

    /* check for errors */
    if (rc)
    {
        //fprintf(stderr, "\n\nERROR MQTT Send: %s\n", mqtt_error_str(mqtt.error));
        exit_mqtt(EXIT_FAILURE, sockfd);
    }
}


/* SEND TO MQTT WITH ACCESS POINT MAC ADDRESS AND TIME STAMP */

static int send_errors = 0;
static uint16_t sequence = 0;


void send_to_mqtt_with_time_and_mac(char *mac_address, char *key, int i, char *value, int value_length, int qos, int retained)
{
    //        g_print("send_to_mqtt_with_time_and_mac\n");

    /* Create topic including mac address */
    char topic[256];

    if (i < 0)
    {
        snprintf(topic, sizeof(topic), "%s/%s/%s/%s", topicRoot, access_point_name, mac_address, key);
    }
    else
    {
        snprintf(topic, sizeof(topic), "%s/%s/%s/%s/%d", topicRoot, access_point_name, mac_address, key, i);
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

    MQTTClient_message pubmsg = MQTTClient_message_initializer;
    pubmsg.payload = packet;
    pubmsg.payloadlen = packet_length;
    pubmsg.qos = qos;
    pubmsg.retained = retained;

    MQTTClient_deliveryToken dt;
    int rc = MQTTClient_publishMessage(client, topic, &pubmsg, &dt);

    //int rc = MQTTClient_waitForCompletion(client, dt, 10000);
    //printf("Message with delivery token %d delivered\n", dt);

    time_t end_t = time(0);
    int diff = (int)difftime(end_t, now);
    if (diff > 0) g_print("MQTT execution time = %is\n", diff);

    /* check for errors */
    if (rc)
    {
        fprintf(stderr, "\n\nERROR Send w time and mac: %i\n", rc);

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
    printf("MQTT %s/%s/%s/%s %s\n", topicRoot, access_point_name, mac_address, key, value);
    send_to_mqtt_with_time_and_mac(mac_address, key, -1, value, strlen(value) + 1, MQTT_PUBLISH_QOS_1, MQTT_PUBLISH_RETAIN);
}

void send_to_mqtt_array(char *mac_address, char *key, unsigned char *value, int length)
{
    printf("MQTT %s/%s/%s/%s bytes[%d]\n", topicRoot, access_point_name, mac_address, key, length);
    send_to_mqtt_with_time_and_mac(mac_address, key, -1, (char *)value, length, MQTT_PUBLISH_QOS_1, MQTT_PUBLISH_RETAIN);
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
        printf("MQTT %s/%s/%s/%s/%d uuid[%d]\n", topicRoot, access_point_name, mac_address, key, i, (int)strlen(uuid));
        send_to_mqtt_with_time_and_mac(mac_address, key, i, uuid, strlen(uuid) + 1, MQTT_PUBLISH_QOS_1, MQTT_PUBLISH_RETAIN);
        if (i < length-1) printf("    ");
    }
}

// numeric values that change all the time not retained, all others retained by MQTT

void send_to_mqtt_single_value(char *mac_address, char *key, int32_t value)
{
    char rssi[12];
    snprintf(rssi, sizeof(rssi), "%i", value);
    printf("MQTT %s/%s/%s/%s %s\n", topicRoot, access_point_name, mac_address, key, rssi);
    send_to_mqtt_with_time_and_mac(mac_address, key, -1, rssi, strlen(rssi) + 1, MQTT_PUBLISH_QOS_1, 0);
}

void send_to_mqtt_single_value_keep(char *mac_address, char *key, int32_t value)
{
    char rssi[12];
    snprintf(rssi, sizeof(rssi), "%i", value);
    printf("MQTT %s/%s/%s/%s %s\n", topicRoot, access_point_name, mac_address, key, rssi);
    send_to_mqtt_with_time_and_mac(mac_address, key, -1, rssi, strlen(rssi) + 1, MQTT_PUBLISH_QOS_1, MQTT_PUBLISH_RETAIN);
}

void send_to_mqtt_single_float(char *mac_address, char *key, float value)
{
    char rssi[12];
    snprintf(rssi, sizeof(rssi), "%.3f", value);
    printf("MQTT %s/%s/%s/%s %s\n", topicRoot, access_point_name, mac_address, key, rssi);
    send_to_mqtt_with_time_and_mac(mac_address, key, -1, rssi, strlen(rssi) + 1, MQTT_PUBLISH_QOS_1, 0);
}

void mqtt_sync()
{
//  g_print("Sync\n");

//    MQTTClient_message pubmsg = MQTTClient_message_initializer;
//    MQTTClient_deliveryToken token;
//    pubmsg.payload = "sync";
//    pubmsg.payloadlen = strlen(pubmsg.payload)+1;
//    pubmsg.qos = 0;
//    pubmsg.retained = 0;

//    MQTTClient_publishMessage(client, "BLF/tiger/status", &pubmsg, &token);

 // not needed for Paho?
//   mosquitto_loop();
}
