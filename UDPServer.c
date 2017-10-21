#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>

#define MAX_BUFF 1000
#define MAX_IP 30
#define MAX_HOST 100
#define GID 12

struct __attribute__((__packed__)) packet_received {
    unsigned int magic_number;
    unsigned short total_message_len;
    unsigned char group_id;
    unsigned char checksum;
    unsigned char req_id;
    unsigned char hosts[MAX_BUFF - 9];
} received_message;

struct __attribute__((__packed__)) valid_return_message {
    unsigned int magic_number;
    unsigned short total_message_len;
    unsigned char group_id;
    unsigned char checksum;
    unsigned char req_id;
    unsigned int ip_addrs[MAX_BUFF - 9];
} send_valid_message;

struct __attribute__((__packed__)) invalid_return_message {
    unsigned int magic_number;
    unsigned short total_message_len;
    unsigned char group_id;
    unsigned char checksum;
    unsigned char byte_err_code;
} send_invalid_message;

// get sockaddr, IPv4 or IPv6
void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

char calculate_checksum(char *message, int length)
{
    char check_sum = 0x00;
    int i = 0;

    while (i < length)
    {
        check_sum += *(message + i);
        i++;
    }

    return ~check_sum;
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
    char * group_port;
    short length;
    int i, x, j, k, y;
    int index, host_index;
    char host_names[MAX_BUFF - 9][MAX_HOST];
    unsigned char ip_addrs[MAX_BUFF - 9][MAX_IP];
    char sum;
    struct hostent *host_info;
    struct in_addr **ip_addresses;
    char* ip_parser;
    char* split_parser;

    if (argc != 2)
    {
        fprintf(stderr, "usage: Need portNum\n");
        exit(1);
    }

    group_port = argv[1];

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC; // set to AF_INET to force IPv4
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_flags = AI_PASSIVE; // use my IP

    if ((rv = getaddrinfo(NULL, group_port, &hints, &servinfo)) != 0) {
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

    while (1) {
        printf("listener: waiting to recvfrom...\n");

        addr_len = sizeof their_addr;
        if ((numbytes = recvfrom(sockfd, buf, MAX_BUFF-1, 0,
                        (struct sockaddr *) &their_addr, &addr_len)) == -1) {
            perror("recvfrom");
            exit(1);
        }

        printf("packet received...\n");

        buf[numbytes] = '\0';

        received_message = *((struct packet_received *)buf);

        received_message.magic_number = ntohs(received_message.magic_number);

        // Check that the correct magic number has been provided.
        if (received_message.magic_number == (int) 1248819489)
        {
            x = 0;
            sum = 0x00;
            // calculate checksum of sent message.
            while (x < numbytes)
            {
                sum += buf[x]
                x++;
            }

            // Check for data corruption
            if (sum == (char) -1)
            {
                received_message.total_message_len = ntohs(received_message.total_message_len);

                // Check that the message is the correct length
                if (received_message.total_message_len == (short) numbytes)
                {
                    i = 0;
                    index = -1;
                    host_index = 0;
                    while (i < received_message.total_message_len - 9) {
                        if (received_message.hosts[i] == '~')
                        {
                            index++;
                            host_index = 0;
                        }
                        else
                        {
                            host_names[index][host_index] = received_message.hosts[i];
                            host_index++;
                        }

                        i++;
                    }

                    x = 0;
                    while (x <= index)
                    {
                        if ((host_info = gethostbyname(host_names[x])) == NULL)
                        {
                            j = 0;
                            while (j < 4)
                            {
                                ip_addrs[x][j] = (char) 255;
                                j++;
                            }
                        }
                        else
                        {
                            ip_parser = inet_ntoa(*((struct in_addr *)host_info->h_addr));

                            split_parser = strtok(ip_parser, ".");
                            k = 0;
                            y = 0;
                            while (y < 3)
                            {
                                ip_addrs[x][k] = (unsigned char) atoi(split_parser);

                                split_parser = strtok(NULL, ".");

                                k++;
                                y++;
                            }

                            ip_addrs[x][k] = (unsigned char) atoi(split_parser);
                        }
                        x++;
                    }

                    length = 9 + (4*x);
                    send_valid_message.magic_number = ntohs(received_message.magic_number);
                    send_valid_message.total_message_len = ntohs(length);
                    send_valid_message.group_id = (char) GID;
                    send_valid_message.checksum = (char) 0;
                    send_valid_message.req_id = received_message.req_id;

                    j = 0
                    while (j < k)
                    {
                        k = 0;

                        while (k < 4)
                        {
                            send_valid_message.ip_addrs[(4*j + k)] = ip_addrs[j][k];
                            k++;
                        }

                        j++;
                    }

                    send_valid_message.checksum = calculate_checksum((char*)&send_valid_message, length);

                    if ((numbytes = sendto(sockfd, (char*)&send_valid_message, length, 0,
                            (struct sockaddr *)&their_addr, addr_len)) == -1) {

                        perror("server: sendto");
                        exit(1);
                    }
                }
            }
        }
    }

    close(sockfd);

    return 0;
}
