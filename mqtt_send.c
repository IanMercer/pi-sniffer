/*
    MQTT code to send BLE status
    And utility functions
*/

#include "mqtt_send.h"
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

#define CERTIFICATEFILE "IoTHubRootCA_Baltimore.pem"
#define MQTT_PUBLISH_QOS_0 0
#define MQTT_PUBLISH_QOS_1 1
#define MQTT_PUBLISH_QOS_2 2
#define MQTT_PUBLISH_DONT_RETAIN 0
#define MQTT_PUBLISH_RETAIN 1

const char *topicRoot = "BLF";
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
//	MQTTAsync client = (MQTTAsync)context;
//	MQTTAsync_disconnectOptions opts = MQTTAsync_disconnectOptions_initializer;
//	int rc;

	printf("Message send failed token %d error code %d\n", response->token, response->code);
//	opts.onSuccess = onDisconnect;
//	opts.onFailure = onDisconnectFailure;
//	opts.context = client;
//	if ((rc = MQTTAsync_disconnect(client, &opts)) != MQTTASYNC_SUCCESS)
//	{
//		printf("Failed to start disconnect, return code %d\n", rc);
//		exit(EXIT_FAILURE);
//	}
}

void onSend(void* context, MQTTAsync_successData* response)
{
    (void)context;
//	MQTTAsync client = (MQTTAsync)context;
//	MQTTAsync_disconnectOptions opts = MQTTAsync_disconnectOptions_initializer;
//	int rc;

	printf("Message with token value %d delivery confirmed\n", response->token);
//	opts.onSuccess = onDisconnect;
//	opts.onFailure = onDisconnectFailure;
//	opts.context = client;
//	if ((rc = MQTTAsync_disconnect(client, &opts)) != MQTTASYNC_SUCCESS)
//	{
//		printf("Failed to start disconnect, return code %d\n", rc);
//		exit(EXIT_FAILURE);
//	}
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


void exit_mqtt(int status, int sockfd)
{
    // TODO: Destroy MQTT Client

    if (sockfd != -1) close(sockfd);
    if (access_point_name) g_free(access_point_name);

    exit(status);
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



void prepare_mqtt(char *mqtt_addr, char *mqtt_port, char* client_id, char* mac_address,
                  char* user, char* pass)
{
    username = user;
    password = pass;

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

    MQTTAsync_init_options inits = MQTTAsync_init_options_initializer;
    inits.do_openssl_init = 1;
    MQTTAsync_global_init(&inits);

    g_print("Create\n");
    MQTTAsync_create(&client, mqtt_addr, client_id, MQTTCLIENT_PERSISTENCE_DEFAULT, NULL);  // MQTTASYNC_SUCCESS

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


void send_to_mqtt_null(char *mac_address, char *key)
{
    /* Create topic including mac address */
    char topic[256];
    snprintf(topic, sizeof(topic), "%s/%s/%s/%s", topicRoot, access_point_name, mac_address, key);

    printf("MQTT %s %s\n", topicRoot, "NULL");

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
		printf("Failed to start sendMessage, return code %d\n", rc);
		exit(EXIT_FAILURE);
	}

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
             exit_mqtt(EXIT_FAILURE, sockfd);
           }
        }
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
  //g_print("Sync\n");

  if (!connected) {
    printf("Reconnecting\n");
    connect_async(client);
  }
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
