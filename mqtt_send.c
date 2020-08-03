/*
    MQTT code to send BLE status
    And utility functions
*/

#include "mqtt_send.h"
#include <MQTTClient.h>
#include <MQTTAsync.h>

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
#include <sys/types.h>


#include <sys/socket.h>
#include <netdb.h>
#include <fcntl.h>

#define CERTIFICATEFILE "IoTHubRootCA_Baltimore.pem"
#define MQTT_PUBLISH_QOS_0 0
#define MQTT_PUBLISH_QOS_1 1
#define MQTT_PUBLISH_QOS_2 2
#define MQTT_PUBLISH_DONT_RETAIN 0
#define MQTT_PUBLISH_RETAIN 1

// Azure does not allow arbitrary MQTT message topics, we have to insert this
#define AZURE_TOPIC_PREFIX "devices"
#define AZURE_TOPIC_SUFFIX "/messages/events"

static bool isAzure;      // Azure is 'limited' in what it can handle
static char* topicRoot;
static char* topicSuffix; // required by Azure
static char access_point_address[6];
static char* access_point_name;
static char* username;
static char* password;

MQTTClient client;

bool connected = false;

void connlost(void *context, char *cause)
{
    (void)context;
    //MQTTAsync client = (MQTTAsync)context;

    printf("\nConnection lost\n");
    printf("     cause: %s\n", cause);
    printf("     Make sure you don't have another copy running\n");

    connected = false;
}

void onDisconnectFailure(void* context, MQTTAsync_failureData* response)
{
    (void)context;
    (void)response;
    printf("Disconnect failed\n");
}

void onDisconnect(void* context, MQTTAsync_successData* response)
{
    (void)context;
    (void)response;
	printf("Successful disconnection\n");
}

void onSendFailure(void* context, MQTTAsync_failureData* response)
{
    (void)context;
    printf("Message send failed token %d error code %d\n", response->token, response->code);
}

void onSend(void* context, MQTTAsync_successData* response)
{
    (void)context;
    (void)response;
}


void onConnectFailure(void* context, MQTTAsync_failureData* response)
{
    (void)context;
    //MQTTAsync client = (MQTTAsync)context;
    printf("Connect failed, rc %d\n", response ? response->code : 0);
    connected = false;
}


void onConnect(void* context, MQTTAsync_successData* response)
{
    (void)response;
	MQTTAsync client = (MQTTAsync)context;
	int rc;

	printf("Successful connection\n");

	MQTTAsync_responseOptions opts = MQTTAsync_responseOptions_initializer;
	MQTTAsync_message pubmsg = MQTTAsync_message_initializer;
	opts.onSuccess = onSend;
	opts.onFailure = onSendFailure;
	opts.context = client;
        const char* topic = "devices/tiger/messages/events/";
	pubmsg.payload = "up";
	pubmsg.payloadlen = (int)strlen(pubmsg.payload) + 1;
	pubmsg.qos = 0;
	pubmsg.retained = 0;
	if ((rc = MQTTAsync_sendMessage(client, topic, &pubmsg, &opts)) != MQTTASYNC_SUCCESS)
	{
		printf("Failed to start sendMessage, return code %d\n", rc);
		exit(EXIT_FAILURE);
	}
}

int messageArrived(void* context, char* topicName, int topicLen, MQTTAsync_message* m)
{
    (void)context;
    (void)topicName;
    (void)m;
    printf("Message arrived %i", topicLen);
	/* not expecting any messages */
	return 1;
    ///* note that published->topic_name is NOT null-terminated (here we'll change it to a c-string) */
    //char *topic_name = (char *)malloc(published->topic_name_size + 1);
    //memcpy(topic_name, published->topic_name, published->topic_name_size);
    //topic_name[published->topic_name_size] = '\0';
    //printf("Received publish('%s'): %s\n", topic_name, (const char *)published->application_message);
    //g_free(topic_name);
}


void exit_mqtt()
{
    // TODO: Destroy MQTT Client

    if (access_point_name) g_free(access_point_name);
}


int ssl_error_cb(const char *str, size_t len, void *u) {
    (void)len;
    (void)u;
    printf("%s\n", str);
    return 0;
}

int connect_async(MQTTAsync client)
{
    connected = true;

    MQTTAsync_connectOptions opts = MQTTAsync_connectOptions_initializer;

    opts.username = username;
    opts.password = password;
    opts.MQTTVersion = MQTTVERSION_3_1_1; //MQTTVERSION_DEFAULT;  // 3.1.1 with fallback

    MQTTAsync_SSLOptions sslopts = MQTTAsync_SSLOptions_initializer;

    sslopts.ssl_error_cb = &ssl_error_cb;
    sslopts.sslVersion = MQTT_SSL_VERSION_TLS_1_2;
    //sslopts.CApath = CERTIFICATEFILE;
    sslopts.trustStore = CERTIFICATEFILE;
    sslopts.enableServerCertAuth = true;

    opts.ssl = &sslopts;

    opts.keepAliveInterval = 20;
    opts.cleansession = 1;
    opts.onSuccess = onConnect;
    opts.onFailure = onConnectFailure;

    opts.context = client;

    g_print("Connecting...\n");
    int rc;
    if ((rc = MQTTAsync_connect(client, &opts)) != MQTTASYNC_SUCCESS)
    {
       printf("Failed to start connect, return code %d\n", rc);
       exit(EXIT_FAILURE);
    }
    return rc;
}


uint8_t sendbuf[4 * 1024 * 1024]; /* 4MByte sendbuf should be large enough to hold multiple whole mqtt messages */
uint8_t recvbuf[2048];            /* recvbuf should be large enough any whole mqtt message expected to be received */



void prepare_mqtt(char *mqtt_uri, char *mqtt_topicRoot, char* client_id, char* mac_address,
                  char* user, char* pass)
{
    username = user;
    password = pass;
    topicRoot = mqtt_topicRoot;

    if (strcmp(topicRoot, AZURE_TOPIC_PREFIX) == 0) {
      isAzure = true;
      topicSuffix = AZURE_TOPIC_SUFFIX;
    } else {
      isAzure = false;
      topicSuffix = "";
    }

    if (mqtt_uri == NULL) {
      printf("MQTT Uri must be set");
      exit(-24);
    }

    if (mqtt_topicRoot == NULL) {
      printf("MQTT Topic Root must be set");
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

    printf("Starting MQTT `%s` topic root=`%s` client_id=`%s`\n", mqtt_uri, mqtt_topicRoot, client_id);
    printf("Username '%s'\n", username);
    printf("Password '%s'\n", password);
    if (isAzure) {
      printf("Sending only limited messages to Azure");
    }

    MQTTAsync_init_options inits = MQTTAsync_init_options_initializer;
    inits.do_openssl_init = 1;
    MQTTAsync_global_init(&inits);

    g_print("Create\n");
    MQTTAsync_create(&client, mqtt_uri, client_id, MQTTCLIENT_PERSISTENCE_DEFAULT, NULL);  // MQTTASYNC_SUCCESS

    //MQTTAsync_connectOptions opts = get_opts();
    //if (options.server_key_file)
    //    opts.ssl->trustStore = options.server_key_file; /*file of certificates trusted by client*/
    //opts.ssl->keyStore = CERTIFICATEFILE;  /*file of certificate for client to present to server*/
    //if (options.client_key_pass)
    //  opts.ssl->privateKeyPassword = options.client_key_pass;
    //if (options.client_private_key_file)
    //  opts.ssl->privateKey = options.client_private_key_file;
    //opts.ssl->verify = true;
    //opts.ssl->enabledCipherSuites = "ALL";

    // TODO opts.will
        //opts.will = &wopts;
	//opts.will->message = "will message";
	//opts.will->qos = 1;
	//opts.will->retained = 0;
	//opts.will->topicName = "will topic";
	//opts.will = NULL;

    int rc;


    g_print("set call backs\n");

    if ((rc = MQTTAsync_setCallbacks(client, NULL, connlost, messageArrived, NULL)) != MQTTASYNC_SUCCESS)
    {
        printf("Failed to set callback, return code %d\n", rc);
        exit(EXIT_FAILURE);
    }

    g_print("connect async\n");

    connect_async(client);

//	MQTTAsync_destroy(&client);
}


/* Create topic string */

void get_topic_short(char* topic, int topic_length, char* key)
{
    snprintf(topic, topic_length, "%s/%s%s/%s", topicRoot, access_point_name, topicSuffix, key);
}

void get_topic(char* topic, int topic_length, char* mac_address, char* key, int index) {

    if (index < 0)
    {
        snprintf(topic, topic_length, "%s/%s%s/%s/%s", topicRoot, access_point_name, topicSuffix, mac_address, key);
    }
    else
    {
        snprintf(topic, topic_length, "%s/%s%s/%s/%s/%d", topicRoot, access_point_name, topicSuffix, mac_address, key, index);
    }
}


void send_to_mqtt_null(char *mac_address, char *key)
{
    /* Create topic including mac address */
    char topic[256];
    get_topic(topic, sizeof(topic), mac_address, key, -1);

    printf("MQTT %s %s\n", topic, "NULL");

    MQTTAsync_responseOptions opts = MQTTAsync_responseOptions_initializer;
    MQTTAsync_message pubmsg = MQTTAsync_message_initializer;
    opts.onSuccess = onSend;
    opts.onFailure = onSendFailure;
    opts.context = client;
    pubmsg.payload = "";
    pubmsg.payloadlen = (int)strlen(pubmsg.payload) + 1;
    pubmsg.qos = 0;
    pubmsg.retained = 0;
    int rc = 0;
    if ((rc = MQTTAsync_sendMessage(client, topic, &pubmsg, &opts)) != MQTTASYNC_SUCCESS)
    {
        printf("Failed to send null message, return code %d\n", rc);
    }
}


/* SEND TO MQTT WITH ACCESS POINT MAC ADDRESS AND TIME STAMP */

static int send_errors = 0;
static uint16_t sequence = 0;


void send_to_mqtt_with_time_and_mac(char* topic, char *value, int value_length, int qos, int retained)
{
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

    MQTTAsync_responseOptions opts = MQTTAsync_responseOptions_initializer;
    MQTTAsync_message pubmsg = MQTTAsync_message_initializer;
    opts.onSuccess = onSend;
    opts.onFailure = onSendFailure;
    opts.context = client;
    pubmsg.payload = packet;
    pubmsg.payloadlen = packet_length;
    pubmsg.qos = qos;
    pubmsg.retained = retained;
    int rc = 0;
    if ((rc = MQTTAsync_sendMessage(client, topic, &pubmsg, &opts)) != MQTTASYNC_SUCCESS)
    {
        printf("Failed to start sendMessage, return code %d\n", rc);
        send_errors ++;
        if (send_errors > 10) {
            g_print("\n\nToo many send errors, restarting\n\n");
            exit(-1);
        }
    }
}

void send_to_mqtt_single(char *mac_address, char *key, char *value)
{
    if (isAzure) { printf("\n"); return; } // can't handle topics
    char topic[256];
    get_topic(topic, sizeof(topic), mac_address, key, -1);

    printf("MQTT %s %s\n", topic, value);
    send_to_mqtt_with_time_and_mac(topic, value, strlen(value) + 1, MQTT_PUBLISH_QOS_1, MQTT_PUBLISH_RETAIN);
}

void send_to_mqtt_array(char *mac_address, char *key, unsigned char *value, int length)
{
    if (isAzure) { printf("\n"); return; } // can't handle topics
    char topic[256];
    get_topic(topic, sizeof(topic), mac_address, key, -1);

    printf("MQTT %s bytes[%d]\n", topic, length);
    send_to_mqtt_with_time_and_mac(topic, (char *)value, length, MQTT_PUBLISH_QOS_1, MQTT_PUBLISH_RETAIN);
}

void send_to_mqtt_distances(unsigned char *value, int length)
{
    // Suitable for Azure
    char topic[256];
    get_topic_short(topic, sizeof(topic), "summary");

    printf("MQTT %s bytes[%d]\n", topic, length);
    send_to_mqtt_with_time_and_mac(topic, (char *)value, length, MQTT_PUBLISH_QOS_1, MQTT_PUBLISH_RETAIN);
}

void send_to_mqtt_uuids(char *mac_address, char *key, char **uuids, int length)
{
    if (isAzure) { printf("\n"); return; } // can't handle topics
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
        /* Create topic including mac address */
        char topic[256];
        get_topic(topic, sizeof(topic), mac_address, key, i);

        char *uuid = uuids[i];
        printf("MQTT %s uuid[%d]\n", topic, (int)strlen(uuid));
        send_to_mqtt_with_time_and_mac(topic, uuid, strlen(uuid) + 1, MQTT_PUBLISH_QOS_1, MQTT_PUBLISH_RETAIN);
        if (i < length-1) printf("    ");
    }
}

// numeric values that change all the time not retained, all others retained by MQTT

void send_to_mqtt_single_value(char *mac_address, char *key, int32_t value)
{
    if (isAzure) { printf("\n"); return; } // can't handle topics
    /* Create topic including mac address */
    char topic[256];
    get_topic(topic, sizeof(topic), mac_address, key, -1);

    char rssi[12];
    snprintf(rssi, sizeof(rssi), "%i", value);
    printf("MQTT %s %s\n", topicRoot, rssi);
    send_to_mqtt_with_time_and_mac(topic, rssi, strlen(rssi) + 1, MQTT_PUBLISH_QOS_1, 0);
}

void send_to_mqtt_single_value_keep(char *mac_address, char *key, int32_t value)
{
    if (isAzure) { printf("\n"); return; } // can't handle topics
    /* Create topic including mac address */
    char topic[256];
    get_topic(topic, sizeof(topic), mac_address, key, -1);

    char rssi[12];
    snprintf(rssi, sizeof(rssi), "%i", value);
    printf("MQTT %s %s\n", topic, rssi);
    send_to_mqtt_with_time_and_mac(topic, rssi, strlen(rssi) + 1, MQTT_PUBLISH_QOS_1, MQTT_PUBLISH_RETAIN);
}

void send_to_mqtt_single_float(char *mac_address, char *key, float value)
{
    if (isAzure) { printf("\n"); return; } // can't handle topics
    /* Create topic including mac address */
    char topic[256];
    get_topic(topic, sizeof(topic), mac_address, key, -1);

    char rssi[12];
    snprintf(rssi, sizeof(rssi), "%.3f", value);
    printf("MQTT %s %s\n", topic, rssi);
    send_to_mqtt_with_time_and_mac(topic, rssi, strlen(rssi) + 1, MQTT_PUBLISH_QOS_1, 0);
}

void mqtt_sync()
{
  //g_print("Sync\n");

  if (!connected) {
    printf("Reconnecting\n");
    connect_async(client);
  }
}
