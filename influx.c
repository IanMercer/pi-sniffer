// Post to InfluxDB

/* This is sample C code as an example */
/* Example of loading stats data into InfluxDB in its Line Protocol format over a network using HTTP POST */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h> /* struct hostent, gethostbyname */
#include <arpa/inet.h>
#include <glib.h>
#include <math.h>
#include <string.h>
#include <sys/types.h>
#include "device.h"

#define BUFSIZE 8196
#define CACHE_TOPICS 100


void post_to_influx_body(struct OverallState* state, char body[BUFSIZE], int len)
{
   if (state->influx_server == NULL || strlen(state->influx_server) == 0) return;

    int ret;
    char header[BUFSIZE];
    char result[BUFSIZE];

    static struct sockaddr_in serv_addr; /* static is zero filled on start up */

    struct hostent *server;
    int sockfd;

    /* create the socket */
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) g_error("ERROR opening socket");

    /* lookup the ip address */
    server = gethostbyname(state->influx_server);
    if (server == NULL) { g_error("ERROR, no such host"); return; }

    /* fill in the structure */
    memset(&serv_addr,0,sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(state->influx_port);
    memcpy(&serv_addr.sin_addr.s_addr, server->h_addr,server->h_length);

    /* connect the socket */
    ret = connect(sockfd,(struct sockaddr *)&serv_addr,sizeof(serv_addr));

    if(ret <0)
    {
        g_warning("Connect failed to InfluxDB %i", ret);
        return;
    }

        /* Note spaces are important and the carriage-returns & newlines */
        /* db= is the datbase name, u= the username and p= the password */
        sprintf(header,
            "POST /write?db=%s&u=%s&p=%s HTTP/1.1\r\nHost: %s:%i\r\nContent-Length: %i\r\n\r\n",
            state->influx_database, 
            state->influx_username, state->influx_password, 
            state->influx_server, state->influx_port, len);

        ret = write(sockfd, header, strlen(header));
        if (ret < 0)
        {
            g_warning("Write Header request to InfluxDB failed");
            close(sockfd);
            return;
        }

        ret = write(sockfd, body, strlen(body));
        if (ret < 0)
            g_warning("Write Data Body to InfluxDB failed");

        /* Get back the acknowledgement from InfluxDB */
        /* It worked if you get "HTTP/1.1 204 No Content" and some other fluff */
        ret = read(sockfd, result, sizeof(result));
        if (ret < 0) g_warning("Reading the result from InfluxDB failed");
        
        //result[ret] = 0; /* terminate string */
        // g_info("Result returned %i bytes from InfluxDB.",ret);
        // for (int j = 0; j < ret; j++){
        //     if (result[j] < 32) result[j] = 32;
        //     if (j < 32)
        //         g_debug("%02x : %c", result[j], result[j]);
        // }

        //g_info("->|%s|<-\n", result);

    close(sockfd);
}

struct cache_item 
{
    char name[80];
    double value;
    time_t time;
};


/*
   Append an Influx line message
*/
void append_influx_line(struct OverallState* state, char* line, int line_length,  const char* group, const char* topic, char* category, double value, time_t timestamp)
{
    /* InfluxDB line protocol note:
        measurement name
        tag is host=... - multiple tags separate with comma
        data is key value
        ending epoch time missing (3 spaces) so InfluxDB generates the timestamp */
    /* InfluxDB line protocol note: ending epoch time missing so InfluxDB greates it */
    int existing_length = strlen(line);

    snprintf(line+existing_length, line_length-existing_length, "%s,host=%s,from=%s,category=%s %s=%.3f %lu000000000\n",
        group,                    // A group of rooms
        topic,                    // access point name
        state->local->client_id,  // from = client_id
        category,
        "count", value,           // count = value
        timestamp);               // nanosecond timestamp
}

void post_to_influx(struct OverallState* state, char* body, int body_length)
{
    if (state->influx_server == NULL || strlen(state->influx_server) == 0) return;
    post_to_influx_body(state, body, body_length);
    //g_info("Posted %s", body);
}
