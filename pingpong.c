#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/udp.h>
#include <netinet/ip.h>
#include <sys/socket.h>
#include <time.h>
#include <errno.h>

#define PORT 60441
#define PAYLOAD_SIZE 64

/* * High-resolution timer using the monotonic clock
 */
double get_time_ms() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec * 1000.0 + (double)ts.tv_nsec / 1000000.0;
}

/* * Applies QoS, disables corking, and shrinks buffers to prevent bufferbloat
 */
void optimize_udp_socket(int sockfd) {
    // 1. Set DSCP EF (Expedited Forwarding) for Wireless Priority
    int tos = 0xB8; 
    if (setsockopt(sockfd, IPPROTO_IP, IP_TOS, &tos, sizeof(tos)) < 0)
        perror("QoS IP_TOS failed");

    // 2. Disable UDP Corking to force immediate transmission
    int cork = 0;
    if (setsockopt(sockfd, IPPROTO_UDP, UDP_CORK, &cork, sizeof(cork)) < 0)
        perror("UDP_CORK failed");

    // 3. Shrink buffers to minimize internal OS lag
    int buf_size = 4096; 
    setsockopt(sockfd, SOL_SOCKET, SO_SNDBUF, &buf_size, sizeof(buf_size));
    setsockopt(sockfd, SOL_SOCKET, SO_RCVBUF, &buf_size, sizeof(buf_size));

    // 4. Linux-specific: SO_PRIORITY and Busy Poll
    int priority = 6;
    setsockopt(sockfd, SOL_SOCKET, SO_PRIORITY, &priority, sizeof(priority));
    
    int busy_poll = 50; // microseconds
    setsockopt(sockfd, SOL_SOCKET, SO_BUSY_POLL, &busy_poll, sizeof(busy_poll));
}

int main(int argc, char *argv[]) {
    int sockfd;
    struct sockaddr_in addr;
    char buffer[PAYLOAD_SIZE];

    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("Socket creation failed");
        exit(1);
    }

    optimize_udp_socket(sockfd);

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(PORT);

    if (argc > 1) { // --- CLIENT MODE ---
        addr.sin_addr.s_addr = inet_addr(argv[1]);
        printf("Starting Pro Ping-Pong Client to %s\n", argv[1]);
        printf("Running with RT priority (if launched with chrt)\n\n");

        for (int i = 0; i < 200; i++) {
            double start = get_time_ms();
            
            if (sendto(sockfd, "PING", 4, 0, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
                perror("Send failed");
                break;
            }
            
            struct sockaddr_in serv_addr;
            socklen_t len = sizeof(serv_addr);
            
            // Wait for response
            int n = recvfrom(sockfd, buffer, PAYLOAD_SIZE, 0, (struct sockaddr*)&serv_addr, &len);
            if (n > 0) {
                double rtt = get_time_ms() - start;
                printf("[%d] RTT: %.3f ms\n", i, rtt);
            }

            usleep(20000); // Send every 20ms (standard for music/control data)
        }
    } else { // --- SERVER MODE ---
        addr.sin_addr.s_addr = INADDR_ANY;
        if (bind(sockfd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
            perror("Bind failed");
            exit(1);
        }
        printf("Pro Server listening on port %d...\n", PORT);
        while (1) {
            struct sockaddr_in cli_addr;
            socklen_t len = sizeof(cli_addr);
            int n = recvfrom(sockfd, buffer, PAYLOAD_SIZE, 0, (struct sockaddr*)&cli_addr, &len);
            if (n > 0) {
                sendto(sockfd, buffer, n, 0, (struct sockaddr*)&cli_addr, len);
            }
        }
    }

    close(sockfd);
    return 0;
}
