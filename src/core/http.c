/*
   Implements HTTP POST
*/

#include "http.h"
#include "device.h"
#include "rooms.h"
#include "cJSON.h"
#include <string.h>
#include <glib.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h> /* struct hostent, gethostbyname */
#include <arpa/inet.h>

#define BUFSIZE 4096

/*
   http post to hostname:port/path with Auth header (optional)
*/
int http_post(char* hostname, int port, char* path, char* auth, char* body, int body_len, char* result, int result_len)
{
    int ret;
    char header[BUFSIZE];

    static struct sockaddr_in serv_addr; /* static is zero filled on start up */

    struct hostent *server;
    int sockfd;

    /* create the socket */
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) { g_error("ERROR opening socket"); return -1; }

    /* lookup the ip address */
    server = gethostbyname(hostname);
    if (server == NULL) { g_error("ERROR, no such host"); return -1; }

    /* fill in the structure */
    memset(&serv_addr,0,sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);
    memcpy(&serv_addr.sin_addr.s_addr, server->h_addr,server->h_length);

    /* connect the socket */
    ret = connect(sockfd,(struct sockaddr *)&serv_addr,sizeof(serv_addr));

    if(ret <0)
    {
        g_warning("Connect failed to %s %i", hostname, ret);
        return -1;
    }

    /* Note spaces are important and the carriage-returns & newlines */
    /* db= is the datbase name, u= the username and p= the password */
    sprintf(header,
        "POST %s HTTP/1.1\r\nHost: %s:%i\r\n%s%s%sContent-Length: %i\r\n\r\n",
        path,
        hostname, port,
        auth == NULL ? "" : "Authorization:",
        auth == NULL ? "" : auth,
        auth == NULL ? "" :"\r\n",
        body_len);

    ret = write(sockfd, header, strlen(header));
    if (ret < 0)
    {
        g_warning("Write header request failed");
        close(sockfd);
        return -1;
    }

    ret = write(sockfd, body, strlen(body));
    if (ret < 0)
        g_warning("Write data body failed");

        /* Get back the acknowledgement */
        /* It worked if you get "HTTP/1.1 204 No Content" and some other fluff */
        ret = read(sockfd, result, result_len);
        if (ret < 0) g_warning("Reading the result from %s failed", hostname);

#if 0
        result[ret] = 0; /* terminate string */
        g_info("Result returned %i bytes.",ret);
        for (int j = 0; j < ret; j++){
            if (result[j] < 32) result[j] = 32;
            if (j < 32)
                g_debug("%02x : %c", result[j], result[j]);
        }
        g_info("->|%s|<-\n", result);
#endif

    close(sockfd);

    return ret;
}
