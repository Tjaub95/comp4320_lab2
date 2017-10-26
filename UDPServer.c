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
#define MAGIC_NUMBER 0x4A6F7921

struct __attribute__((__packed__)) packet_received {
    char magic_number[4];
    short total_message_len;
    char group_id;
    char checksum;
    char req_id;
    char hosts[MAX_BUFF - 9];
};

struct valid_return_message {
    int magic_number;
    short total_message_len;
    char group_id;
    char checksum;
    char req_id;
    char ip_addrs[MAX_BUFF - 9][4];
} __attribute__((__packed__));

struct __attribute__((__packed__)) invalid_return_message {
    int magic_number;
    short total_message_len;
    char group_id;
    char checksum;
    char byte_err_code;
} send_invalid_message;

// get sockaddr, IPv4 or IPv6
void *get_in_addr(struct sockaddr *sa) {
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in *) sa)->sin_addr);
    }

    return &(((struct sockaddr_in6 *) sa)->sin6_addr);
}

char calculate_checksum(char *message, int length) {
    int check_sum = 0;
    int i = 0;
    short int mask = 0x00FF;

    while (i < length) {
        check_sum += (int) message[i];
        i++;
    }

    if (check_sum > 255) {
        int lcheck = check_sum;
        lcheck = lcheck >> 8;

        int rcheck = check_sum & mask;
        check_sum = lcheck + rcheck;
    }

    check_sum = ~check_sum;
    check_sum = check_sum & mask;

    return (char) check_sum;
}

int main(char argc, char *argv[]) {
    int sockfd;
    struct addrinfo hints, *servinfo, *p;
    int rv;
    int numbytes;
    struct sockaddr_storage their_addr;
    char buf[MAX_BUFF];
    socklen_t addr_len;
    char s[INET6_ADDRSTRLEN];
    char *group_port;
    short length;
    int i, x, j, k, y;
    int index, host_index;
    char host_names[MAX_BUFF - 9][MAX_HOST];
    char ip_addrs[MAX_BUFF - 9][4];
    char sum;
    struct hostent *host_info;
    struct in_addr **ip_addresses;
    char *ip_parser;
    char *split_parser;

    if (argc != 2) {
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
        if ((numbytes = recvfrom(sockfd, buf, MAX_BUFF - 1, 0,
                                 (struct sockaddr *) &their_addr, &addr_len)) == -1) {
            perror("recvfrom");
            exit(1);
        }

        printf("packet received...\n");

        buf[numbytes] = '\0';

        struct packet_received received_message;

        received_message = *((struct packet_received *) buf);

        char mag[] = {74, 111, 121, 33};

        // Check that the correct magic number has been provided.
        if (strncmp(received_message.magic_number, mag, 4) == 0) {
            x = 0;
            sum = 0x00;
            // calculate checksum of sent message.
            while (x < numbytes) {
                sum += buf[x];
                x++;
            }

            sum = calculate_checksum((char *) &received_message, numbytes);

            if (sum == (char) 1) {
                sum = -sum;
            } else if (sum == (char) 0) {
                sum--;
            }
            // Check for data corruption
            if (sum == (char) -1) {
                received_message.total_message_len = ntohs(received_message.total_message_len);

                // Check that the message is the correct length
                if (received_message.total_message_len == (short) numbytes) {
                    i = 0;
                    index = -1;
                    host_index = 0;
                    while (i < numbytes - 9) {
                        int loop_len = received_message.hosts[i];
                        index++;

                        while (loop_len > 0) {
                            i++;
                            host_names[index][host_index] = received_message.hosts[i];
                            host_index++;
                            loop_len--;
                        }

                        host_index = 0;
                        i++;
                    }

                    x = 0;
                    while (x <= index) {
                        if ((host_info = gethostbyname(host_names[x])) == NULL) {
                            j = 0;
                            while (j < 4) {
                                ip_addrs[x][j] = (char) 255;
                                j++;
                            }
                        } else {
                            ip_parser = inet_ntoa(*((struct in_addr *) host_info->h_addr));

                            split_parser = strtok(ip_parser, ".");
                            k = 0;
                            y = 0;
                            while (y < 3) {
                                ip_addrs[x][k] = (unsigned char) atoi(split_parser);

                                split_parser = strtok(NULL, ".");

                                k++;
                                y++;
                            }

                            ip_addrs[x][k] = (unsigned char) atoi(split_parser);
                        }
                        x++;
                    }

                    length = 9 + (4 * x);

                    struct valid_return_message send_valid_message;

                    send_valid_message.magic_number = (int) ntohl(MAGIC_NUMBER);
                    send_valid_message.total_message_len = ntohs(length);
                    send_valid_message.group_id = received_message.group_id;
                    send_valid_message.checksum = (char) 0;
                    send_valid_message.req_id = received_message.req_id;
                    memcpy(send_valid_message.ip_addrs, ip_addrs, sizeof(ip_addrs));

                    send_valid_message.checksum = calculate_checksum((char *) &send_valid_message, length);

                    if ((numbytes = sendto(sockfd, (char *) &send_valid_message, length, 0,
                                           (struct sockaddr *) &their_addr, addr_len)) == -1) {

                        perror("server: sendto");
                        exit(1);
                    }
                } else {
                    send_invalid_message.magic_number = (int) ntohl(MAGIC_NUMBER);
                    send_invalid_message.checksum = (char) 0;
                    send_invalid_message.group_id = received_message.group_id;
                    send_invalid_message.total_message_len = (short) ntohs(9);
                    send_invalid_message.byte_err_code = (char) 4;

                    send_invalid_message.checksum = calculate_checksum((char *) &send_invalid_message,
                                                                       send_invalid_message.total_message_len);

                    if ((numbytes = sendto(sockfd, (char *) &send_invalid_message,
                                           send_invalid_message.total_message_len, 0,
                                           (struct sockaddr *) &their_addr, addr_len)) == -1) {
                        perror("server: sendto");
                        exit(1);
                    }
                }
            } else {
                send_invalid_message.magic_number = (int) ntohl(MAGIC_NUMBER);
                send_invalid_message.checksum = (char) 0;
                send_invalid_message.group_id = received_message.group_id;
                send_invalid_message.total_message_len = (short) ntohs(9);
                send_invalid_message.byte_err_code = (char) 2;

                send_invalid_message.checksum = calculate_checksum((char *) &send_invalid_message,
                                                                   send_invalid_message.total_message_len);

                if ((numbytes = sendto(sockfd, (char *) &send_invalid_message, send_invalid_message.total_message_len,
                                       0,
                                       (struct sockaddr *) &their_addr, addr_len)) == -1) {
                    perror("server: sendto");
                    exit(1);
                }
            }
        } else {
            send_invalid_message.magic_number = (int) ntohl(MAGIC_NUMBER);
            send_invalid_message.checksum = (char) 0;
            send_invalid_message.group_id = received_message.group_id;
            send_invalid_message.total_message_len = (short) ntohs(9);
            send_invalid_message.byte_err_code = (char) 1;

            send_invalid_message.checksum = calculate_checksum((char *) &send_invalid_message,
                                                               send_invalid_message.total_message_len);

            if ((numbytes = sendto(sockfd, (char *) &send_invalid_message, send_invalid_message.total_message_len, 0,
                                   (struct sockaddr *) &their_addr, addr_len)) == -1) {
                perror("server: sendto");
                exit(1);
            }
        }
    }

    close(sockfd);

    return 0;
}
