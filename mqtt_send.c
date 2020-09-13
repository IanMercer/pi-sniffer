/*
    MQTT code to send BLE status
    And utility functions
*/

#include "mqtt_send.h"

#include <sys/socket.h>
#include <net/if.h>
#include <time.h>
#include <math.h>
#include <signal.h>
#include <glib.h>

#include <MQTTClient.h>
#include <MQTTAsync.h>


#define CERTIFICATEFILE "IoTHubRootCA_Baltimore.pem"
#define MQTT_PUBLISH_QOS_0 0
#define MQTT_PUBLISH_QOS_1 1
#define MQTT_PUBLISH_QOS_2 2
#define MQTT_PUBLISH_DONT_RETAIN 0
#define MQTT_PUBLISH_RETAIN 1

// Azure does not allow arbitrary MQTT message topics, we have to insert this
#define AZURE_TOPIC_PREFIX "devices"
#define AZURE_TOPIC_SUFFIX "/messages/events"

// Internal communication assumes a local MQTT broker to relay between access points
#define MESH_TOPIC "mesh"

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

    g_info("MQTT Connection lost '%s'", cause);
    g_debug("    Make sure you don't have another copy running");

    connected = false;
}

void onDisconnectFailure(void* context, MQTTAsync_failureData* response)
{
    (void)context;
    (void)response;
    g_info("MQTT Disconnect failed");
}

void onDisconnect(void* context, MQTTAsync_successData* response)
{
    (void)context;
    (void)response;
	g_info("MQTT Successful disconnection\n");
}

void onSendFailure(void* context, MQTTAsync_failureData* response)
{
    (void)context;
    g_info("MQTT Message send failed token %d error code %d\n", response->token, response->code);
}

void onSend(void* context, MQTTAsync_successData* response)
{
    (void)context;
    (void)response;
}


void onSubscribe(void* context, MQTTAsync_successData* response)
{
    (void)context;
    (void)response;
    g_info("MQTT Subscribe succeeded\n");
    // subscribed = 1;
}

void onSubscribeFailure(void* context, MQTTAsync_failureData* response)
{
    (void)context;
    g_info("MQTT Subscribe failed, rc %d\n", response ? response->code : 0);
    // finished = 1;
}



void onConnectFailure(void* context, MQTTAsync_failureData* response)
{
    (void)context;
    //MQTTAsync client = (MQTTAsync)context;
    g_info("MQTT Connect failed, rc %d\n", response ? response->code : 0);
    connected = false;
}


void onConnect(void* context, MQTTAsync_successData* response)
{
    (void)response;
    MQTTAsync client = (MQTTAsync)context;
    int rc;

    connected = true;

    g_info("Successful connection");

    MQTTAsync_responseOptions opts = MQTTAsync_responseOptions_initializer;
    MQTTAsync_message pubmsg = MQTTAsync_message_initializer;
    opts.onSuccess = onSend;
    opts.onFailure = onSendFailure;
    opts.context = client;

    /*
        Send an 'alive' message
    */

    char topic[128];
    snprintf(topic, sizeof(topic), "%s/%s/messages/events/up", topicRoot, access_point_name);

    pubmsg.payload = "{state:\"up\"}";
    pubmsg.payloadlen = (int)strlen(pubmsg.payload) + 1;
    pubmsg.qos = 0;
    pubmsg.retained = 0;
    if ((rc = MQTTAsync_sendMessage(client, topic, &pubmsg, &opts)) != MQTTASYNC_SUCCESS)
    {
        g_info("Failed to start sendMessage, return code %d", rc);
        exit(EXIT_FAILURE);
    }

    /*
        And subscribe so we get messages from all other scanners
    */

    MQTTAsync_responseOptions opts2 = MQTTAsync_responseOptions_initializer;
    //MQTTAsync_message pubmsg2 = MQTTAsync_message_initializer;

    opts2.onSuccess = onSubscribe;
    opts2.onFailure = onSubscribeFailure;
    opts2.context = client;
    //deliveredtoken = 0;

    if ((rc = MQTTAsync_subscribe(client, MESH_TOPIC, 0, &opts2)) != MQTTASYNC_SUCCESS)
    {
       g_info("Failed to start subscribe, return code %d", rc);
       exit(EXIT_FAILURE);
    }
}

int messageArrived(void* context, char* topicName, int topicLen, MQTTAsync_message* m)
{
    (void)context;
    (void)topicLen;

    char* payloadptr = m->payload;

    // ignore messages from self
    if (strcmp(payloadptr, access_point_name) == 0) return 1;

    g_info("Incoming from: %s", payloadptr);  // starts with a string (the access point sending it)

    struct Device device;

    memcpy(&device, payloadptr+32, sizeof(struct Device));

    g_info("  Update for %s '%s'\n", device.mac, device.name);

    // TODO: Put this into the global array and update which access point is closest

    MQTTAsync_freeMessage(&m);
    MQTTAsync_free(topicName);
    return 1;
}


void exit_mqtt()
{
    // TODO: Destroy MQTT Client
    if (access_point_name) g_free(access_point_name);
}

int ssl_error_cb(const char *str, size_t len, void *u) {
    (void)len;
    (void)u;
    g_info("SSL error '%s'", str);
    return 0;
}

int connect_async(MQTTAsync client)
{
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

    g_info("MQTT Connecting...");
    int rc;
    if ((rc = MQTTAsync_connect(client, &opts)) != MQTTASYNC_SUCCESS)
    {
       g_info("MQTT Failed to start connect, return code %d", rc);
       exit(EXIT_FAILURE);
    }
    return rc;
}


uint8_t sendbuf[4 * 1024 * 1024]; /* 4MByte sendbuf should be large enough to hold multiple whole mqtt messages */
uint8_t recvbuf[2048];            /* recvbuf should be large enough any whole mqtt message expected to be received */

static bool isMQTTEnabled = false;

void prepare_mqtt(char *mqtt_uri, char *mqtt_topicRoot, char* client_id, char* mac_address,
                  char* user, char* pass)
{
    username = user;
    password = pass;
    topicRoot = mqtt_topicRoot;

    if (mqtt_uri == NULL || strlen(mqtt_uri) == 0) {
      g_info("MQTT Uri must be set, running with no MQTT");
      isMQTTEnabled = false;
      return;
    }

    if (strcmp(topicRoot, AZURE_TOPIC_PREFIX) == 0) {
      isAzure = true;
      topicSuffix = AZURE_TOPIC_SUFFIX;
    } else {
      isAzure = false;
      topicSuffix = "";
    }

    if (mqtt_topicRoot == NULL) {
      g_warning("MQTT Topic Root must be set");
      exit(-24);
    }

    if (client_id == NULL) {
      g_warning("Client ID must be set");
      exit(-24);
    }

    memcpy(&access_point_address, mac_address, 6);
    access_point_name = strdup(client_id);

    //char will_topic[256];
    //snprintf(will_topic, sizeof(will_topic), "%s/%s/%s", topicRoot, access_point_name, "state");

    //const char* will_message = "down";

    isMQTTEnabled = true;

    g_info("Starting MQTT `%s` topic root=`%s` client_id=`%s`\n", mqtt_uri, mqtt_topicRoot, client_id);
    g_info("Username '%s'\n", username);
    g_info("Password '%s'\n", password);
    if (isAzure) {
      g_info("Sending only limited messages to Azure");
    }

    MQTTAsync_init_options inits = MQTTAsync_init_options_initializer;
    inits.do_openssl_init = 1;
    MQTTAsync_global_init(&inits);

    g_info("Create MQTT Async\n");
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

    //int rc;

    /* LISTEN FOR MQTT - WAS USED FOR MESH, WILL BE USED FOR AZURE CONFIG MESSAGES
    g_print("set call backs\n");

    if ((rc = MQTTAsync_setCallbacks(client, NULL, connlost, messageArrived, NULL)) != MQTTASYNC_SUCCESS)
    {
        printf("Failed to set callback, return code %d\n", rc);
        exit(EXIT_FAILURE);
    }
    */

    g_info("MQTT connect async");

    connect_async(client);

//	MQTTAsync_destroy(&client);
}


/* Create topic string */

void get_topic(char* topic, int topic_length, char* mac_address, char* key)
{
    // Azure: devices/{device-id}/messages/events/
    //        devices/{device_id}/messages/events/{property_bag}

    (void)mac_address;   // Was used in a topic but Azure can't handle it
    snprintf(topic, topic_length, "%s/%s%s/%s", topicRoot, access_point_name, "/messages/events", key);
}

static int send_errors = 0;

/* Pseudo JSON formatters */

void json_int(char*message, int length, char* mac, char* field, int value) {
    time_t now = time(0);
    snprintf(message, length, "{\"mac\":\"%s\", \"time\":%lu, \"%s\":%i}", mac, now, field, value);
}

void json_long(char*message, int length, char* mac, char* field, long value) {
    time_t now = time(0);
    snprintf(message, length, "{\"mac\":\"%s\", \"time\":%lu, \"%s\":%lu}", mac, now, field, value);
}

void json_float(char*message, int length, char* mac, char* field, float value) {
    time_t now = time(0);
    snprintf(message, length, "{\"mac\":\"%s\", \"time\":%lu, \"%s\":%.3f}", mac, now, field, value);
}

void json_double(char*message, int length, char* mac, char* field, double value) {
    time_t now = time(0);
    snprintf(message, length, "{\"mac\":\"%s\", \"time\":%lu, \"%s\":%f}", mac, now, field, value);
}

void json_string(char*message, int length, char* mac, char* field, char* value) {
    time_t now = time(0);
    snprintf(message, length, "{\"mac\":\"%s\", \"time\":%lu, \"%s\":\"%s\"}", mac, now, field, value);
}

void json_int_array(char*message, int length, char* mac, char* field, int* values, int values_length) {
    time_t now = time(0);
    snprintf(message, length, "{\"mac\":\"%s\", \"time\":%lu, \"%s\":[", mac, now, field);

    int ptr = strlen(message);
    for(int i = 0; i < values_length; i++)
    {
        ptr += snprintf(message + ptr, length - ptr, "%i,", values[i]);
    }

    // go back one for the comma and overwrite
    snprintf(message + ptr - 1, length - ptr, "]}");
}

void json_byte_array(char*message, int length, char* mac, char* field, char* key, unsigned char* values, int values_length) {
    time_t now = time(0);
    snprintf(message, length, "{\"mac\":\"%s\", \"time\":%lu, \"%s\":{ \"key\",\"%s\", [", mac, now, field, key);

    int ptr = strlen(message);
    for(int i = 0; i < values_length; i++)
    {
        ptr += snprintf(message + ptr, length - ptr, "%i,", values[i]);
    }

    // go back one for the comma and overwrite
    snprintf(message + ptr - 1, length - ptr, "]}}");
}

void json_string_array(char*message, int length, char* mac, char* field, char** values, int values_length) {
    time_t now = time(0);
    snprintf(message, length, "{\"mac\":\"%s\", \"time\":%lu, \"%s\":[", mac, now, field);

    int ptr = strlen(message);
    for(int i = 0; i < values_length; i++)
    {
        ptr += snprintf(message + ptr, length - ptr, "\"%s\",", values[i]);
    }

    // go back one for the comma and overwrite
    snprintf(message + ptr - 1, length - ptr, "]}");
}

void json_array_no_mac(char*message, int length, char* field, unsigned char* values, int values_length) {
    time_t now = time(0);
    snprintf(message, length, "{\"time\":%lu, \"%s\":[", now, field);

    int ptr = strlen(message);
    for(int i = 0; i < values_length; i++)
    {
        ptr += snprintf(message + ptr, length - ptr, "%i,", values[i]);
    }

    // go back one for the comma and overwrite
    snprintf(message + ptr - 1, length - ptr, "]}");
}

/* Send JSON string to MQTT */

void send_to_mqtt(char* topic, char *json, int qos, int retained)
{
    if (!isMQTTEnabled) return;
    if (!connected){
        g_debug("MQTT Could not send message, not connected yet, discarding");
    }

    g_debug("MQTT %s %s", topic, json);
    int length = strlen(json);

    MQTTAsync_responseOptions opts = MQTTAsync_responseOptions_initializer;
    MQTTAsync_message pubmsg = MQTTAsync_message_initializer;
    opts.onSuccess = onSend;
    opts.onFailure = onSendFailure;
    opts.context = client;
    pubmsg.payload = json;
    pubmsg.payloadlen = length + 1;
    pubmsg.qos = qos;
    pubmsg.retained = retained;
    int rc = 0;
    if ((rc = MQTTAsync_sendMessage(client, topic, &pubmsg, &opts)) != MQTTASYNC_SUCCESS)
    {
        g_warning("Failed to start sendMessage, return code %d", rc);
        send_errors ++;
        if (send_errors > 10) {
            g_warning("Too many send errors, restarting");
            exit(-1);
        }
    }
}


/*
     Access points also communicate directly with each other to inform of changes
*/
void send_device_mqtt(struct Device* device)
{
    if (!isMQTTEnabled) return;
    if (!connected){
        g_debug("MQTT Could not send message, not connected yet, discarding");
    }

    g_debug("    MQTT %s device %s '%s'\n", MESH_TOPIC, device->mac, device->name);

    char buffer[2048];
    memcpy(buffer, access_point_name, strlen(access_point_name)+1);  // assume this is <32 characters
    buffer[31] = '\0'; // Just in case it wasn't

    memcpy(buffer+32, device, sizeof(struct Device));
    int length = 32 + sizeof(struct Device);

    MQTTAsync_responseOptions opts = MQTTAsync_responseOptions_initializer;
    MQTTAsync_message pubmsg = MQTTAsync_message_initializer;
    opts.onSuccess = onSend;
    opts.onFailure = onSendFailure;
    opts.context = client;
    pubmsg.payload = buffer;
    pubmsg.payloadlen = length;
    pubmsg.qos = 1;
    pubmsg.retained = 0;
    int rc = 0;
    if ((rc = MQTTAsync_sendMessage(client, MESH_TOPIC, &pubmsg, &opts)) != MQTTASYNC_SUCCESS)
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
    if (!isMQTTEnabled) return;

    char topic[256];
    get_topic(topic, sizeof(topic), mac_address, key);

    char packet[2048];
    json_string(packet, sizeof(packet), mac_address, key, value);
    send_to_mqtt(topic, packet, MQTT_PUBLISH_QOS_1, 0);
}

void send_to_mqtt_array(char *mac_address, char *key, char* valuekey, unsigned char *value, int length)
{
    if (!isMQTTEnabled) return;

    char topic[256];
    get_topic(topic, sizeof(topic), mac_address, key);
    char packet[2048];
    json_byte_array(packet, sizeof(packet), mac_address, key, valuekey, value, length);
    send_to_mqtt(topic, packet, MQTT_PUBLISH_QOS_1, 0);
}

void send_to_mqtt_distances(unsigned char *value, int length)
{
    if (!isMQTTEnabled) return;
    //g_debug("send_to_mqtt_distances");

    char topic[256];
    get_topic(topic, sizeof(topic), "no-mac", "summary");
    char packet[2048];
    json_array_no_mac(packet, sizeof(packet), "distances", value, length);
    send_to_mqtt(topic, packet, MQTT_PUBLISH_QOS_1, 0);
}

void send_to_mqtt_uuids(char *mac_address, char *key, char **uuids, int length)
{
    if (!isMQTTEnabled) return;
    //g_debug("send_to_mqtt_uuids");

    char topic[256];
    get_topic(topic, sizeof(topic), mac_address, key);

    char packet[2048];
    json_string_array(packet, sizeof(packet), mac_address, "uuids", uuids, length);
    send_to_mqtt(topic, packet, MQTT_PUBLISH_QOS_1, 0);
}

void send_to_mqtt_single_value(char *mac_address, char *key, int32_t value)
{
    if (!isMQTTEnabled) return;
    //g_debug("send_to_mqtt_single_value");

    char topic[256];
    get_topic(topic, sizeof(topic), mac_address, key);

    char packet[2048];
    json_int(packet, sizeof(packet), mac_address, key, value);
    send_to_mqtt(topic, packet, MQTT_PUBLISH_QOS_1, 0);
}

void send_to_mqtt_single_value_keep(char *mac_address, char *key, int32_t value)
{
    if (!isMQTTEnabled) return;
    //g_debug("send_to_mqtt_single_value_keep");

    char topic[256];
    get_topic(topic, sizeof(topic), mac_address, key);

    char packet[2048];
    json_int(packet, sizeof(packet), mac_address, key, value);

    send_to_mqtt(topic, packet, MQTT_PUBLISH_QOS_1, MQTT_PUBLISH_RETAIN);
}

void send_to_mqtt_single_float(char *mac_address, char *key, float value)
{
    if (!isMQTTEnabled) return;
    //g_debug("send_to_mqtt_single_float");

    char topic[256];
    get_topic(topic, sizeof(topic), mac_address, key);

    char packet[2048];
    json_float(packet, sizeof(packet), mac_address, key, value);

    send_to_mqtt(topic, packet, MQTT_PUBLISH_QOS_1, 0);
}

void mqtt_sync()
{
    if (!isMQTTEnabled) return;
    //g_debug("mqtt_sync");
  
    if (!connected) {
        printf("Reconnecting\n");
        connect_async(client);
    }
}
