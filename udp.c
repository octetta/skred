#include <errno.h>
#include <pthread.h>
#include <stdint.h>
#include <arpa/inet.h>

#include "skred.h"
#include "wire.h"
#include "udp.h"

// Simple hash function for UDP address/port to array index
int get_connection_index(struct sockaddr_in *addr, int array_size) {
    uint32_t ip = addr->sin_addr.s_addr;
    uint16_t port = addr->sin_port;
    
    // Combine IP and port with XOR and multiply
    uint32_t hash = ip ^ (port << 16) ^ port;
    
    // Simple mixing
    hash = hash * 2654435761u;  // Knuth's multiplicative hash
    
    return hash % array_size;
}
int udp_port = UDP_PORT;
int udp_running = 1;

pthread_t udp_thread_handle;

struct sockaddr_in serve;

int udp_open(int port) {
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    int opt = 1;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(int));
    bzero(&serve, sizeof(serve));
    serve.sin_family = AF_INET;
    serve.sin_addr.s_addr = htonl(INADDR_ANY);
    serve.sin_port = htons(port);
    if (bind(sock, (struct sockaddr *)&serve, sizeof(serve)) >= 0) {
        return sock;
    }
    return -1;
}

void *udp_main(void *arg) {
  if (udp_port <= 0) {
    return NULL;
  }
  int sock = udp_open(udp_port);
  if (sock < 0) {
    puts("# udp thread cannot run");
    return NULL;
  }
  pthread_setname_np(pthread_self(), "udp");
  struct timeval tv;
  tv.tv_sec = 1;
  tv.tv_usec = 0;
  setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (struct timeval *)&tv, sizeof(struct timeval));
  struct sockaddr_in client;
  unsigned int client_len = sizeof(client);
  char line[1024];
  fd_set readfds;
  struct timeval timeout;
  wire_t w = WIRE();
  while (udp_running) {
    FD_ZERO(&readfds);
    FD_SET(sock, &readfds);
    timeout.tv_sec = 1;
    timeout.tv_usec = 0;
    int ready = select(sock+1, &readfds, NULL, NULL, &timeout);
    if (ready > 0 && FD_ISSET(sock, &readfds)) {
      ssize_t n = recvfrom(sock, line, sizeof(line), 0, (struct sockaddr *)&client, &client_len);
      if (n > 0) {
        line[n] = '\0';
        // printf("# from %d\n", ntohs(client.sin_port)); // port
        // in the future, this should get ip and port and use for
        // context amongst multiple udp clients
        wire(line, &w, 0);
      } else {
        if (errno == EAGAIN) continue;
      }
    } else if (ready == 0) {
      // timeout
    } else {
      perror("# select");
    }
  }
  if (debug) printf("# udp stopping\n");
  return NULL;
}

void udp_start(int port) {
  udp_running = 1;
  pthread_create(&udp_thread_handle, NULL, udp_main, NULL);
  pthread_detach(udp_thread_handle);
}

void udp_stop(void) {
  udp_running = 0;
}