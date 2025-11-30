#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include "udpmini.h"

udp_t *udp_open(const char *ip, int port) {
  udp_t *h = calloc(1, sizeof(udp_t));
  if (!h) return NULL;

  h->sockfd = socket(AF_INET, SOCK_DGRAM, 0);
  if (h->sockfd < 0) {
    free(h);
    return NULL;
  }

  memset(&h->dest_addr, 0, sizeof(h->dest_addr));
  h->dest_addr.sin_family = AF_INET;
  h->dest_addr.sin_port = htons(port);

  if (inet_pton(AF_INET, ip, &h->dest_addr.sin_addr) <= 0) {
    close(h->sockfd);
    free(h);
    return NULL;
  }

  return h;
}

int udp_send(udp_t *handle, const char *message, int len) {
  if (!handle || handle->sockfd < 0) return -1;

  return sendto(handle->sockfd, message, len, 0,
    (struct sockaddr*)&handle->dest_addr,
    sizeof(handle->dest_addr));
}

void udp_close(udp_t *handle) {
  if (handle) {
    if (handle->sockfd >= 0) {
      close(handle->sockfd);
    }
    free(handle);
  }
}