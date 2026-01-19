#ifndef UDP_CLIENT_H
#define UDP_CLIENT_H

#include <netinet/in.h>

typedef struct {
  int sockfd;
  struct sockaddr_in dest_addr;
} udp_t;

// Open a UDP connection (non-blocking, reusable)
udp_t *udp_open(const char *ip, int port);

// Send data (returns number of bytes sent or -1 on error)
int udp_send(udp_t *handle, const char *message, int len);

// Close the UDP handle
void udp_close(udp_t *handle);

#endif