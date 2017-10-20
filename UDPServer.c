#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>

#define PORT "10022"
#define MAX_BUFF 1000

struct packet_received {
    unsigned int magic_number;
    unsigned short total_message_len;
    unsigned char group_id;
    unsigned char checksum;
    unsigned char req_id;
    unsigned char hosts[MAX_BUFF - 9];
} __attribute__((__packed__));

typedef struct packet_received pr;

// get sockaddr, IPv4 or IPv6
void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

int main(char argc, char *argv[])
{
    int sockfd;
    struct addrinfo hints, *servinfo, *p;
    int rv;
    int numbytes;
    struct sockaddr_storage their_addr;
    char buf[MAX_BUFF];
    socklen_t addr_len;
    char s[INET6_ADDRSTRLEN];
    if (argc != 2)
    {
        fprintf(stderr, "usage: Need portNum\n");
        exit(1);
    }

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC; // set to AF_INET to force IPv4
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_flags = AI_PASSIVE; // use my IP

    if ((rv = getaddrinfo(NULL, argv[1], &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }

    // Loop through all the results and bind to the first we can
    for (p = servinfo; p != NULL; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype,
                        p->ai_protocol)) == -1) {
            perror("listener: socket");
            continue;
        }

        if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sockfd);
            perror("listener: bind");
            continue;
        }

        break;
    }

    if (p == NULL) {
        fprintf(stderr, "listener: failed to bind socket\n");
        return 2;
    }

    freeaddrinfo(servinfo);

    addr_len = sizeof their_addr;
    while (1) {
        printf("listener: waiting to recvfrom...\n");
        pr client_message;
        if ((numbytes = recvfrom(sockfd, buf, MAX_BUFF-1, 0,
                        (struct sockaddr *) &their_addr, &addr_len)) == -1) {
            perror("recvfrom");
            exit(1);
        }

        buf[numbytes] = '\0';

        client_message = *((struct packet_received *)buf);

        printf("client sent: %s\n", client_message);
    }
    close(sockfd);

    return 0;
}
