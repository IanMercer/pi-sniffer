// Client side implementation of UDP client-server model 
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#define MAXLINE 1024

void udp_send(int port, const char* message, int message_length) {
    int sockfd;

    const int opt = 1;
    struct sockaddr_in     servaddr;

    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0 ) {
        perror("socket creation failed");
        return;
    }
    if (setsockopt(sockfd, SOL_SOCKET, SO_BROADCAST, (char*)&opt, sizeof(opt)) < 0) {
        perror("setsockopt error");
        close(sockfd); 
        return;
    }

    memset(&servaddr, 0, sizeof(struct sockaddr_in));

    // Filling server information
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(port);
    //servaddr.sin_addr.s_addr = INADDR_ANY;
    servaddr.sin_addr.s_addr = htonl(INADDR_BROADCAST);

    int sent = sendto(sockfd, message, message_length, 0, (const struct sockaddr *) &servaddr, sizeof(struct sockaddr_in)); 
    printf("Message sent to port %i - %i.\n", port, sent);
    
//    char buffer[MAXLINE];
//    n = recvfrom(sockfd, (char *)buffer, MAXLINE, MSG_WAITALL, (struct sockaddr *) &servaddr, &len); 
//    buffer[n] = '\0'; 
//    printf("Server : %s\n", buffer); 
  
    close(sockfd); 
} 
