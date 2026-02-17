#include <errno.h>

#ifndef _WIN32
#include <pthread.h>
#endif

#include <stdint.h>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
//#define close closesocket
typedef int socklen_t;
#else
#include <arpa/inet.h>
#define SOCKET int
#define INVALID_SOCKET (-1)
#define SOCKET_ERROR (-1)
#include <sys/select.h>
#endif


#include "skred.h"
#include "skode.h"
#include "udp.h"
#include "util.h"

// Simple hash function for UDP address/port to array index
static int get_connection_index(struct sockaddr_in *addr, int array_size) {
    uint32_t ip = addr->sin_addr.s_addr;
    uint16_t port = addr->sin_port;
    
    // Combine IP and port with XOR and multiply
    uint32_t hash = ip ^ (port << 16) ^ port;
    
    // Simple mixing
    hash = hash * 2654435761u;  // Knuth's multiplicative hash
    
    return hash % array_size;
}

static int udp_port = 0;
static int udp_running = 1;

static pthread_t udp_thread_handle;
static pthread_attr_t udp_attr;

static struct sockaddr_in serve;

static int udp_open(int port) {
#ifdef _WIN32
  static int first = 1;
  if (first) {
    WSADATA wsa;
    WSAStartup(MAKEWORD(2,2), &wsa);
    first = 0;
  }
#endif
    SOCKET sock = socket(AF_INET, SOCK_DGRAM, 0);
    int opt = 1;
#ifdef _WIN32
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (char *)&opt, sizeof(int));
    memset(&serve, 0, sizeof(serve));
#else
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(int));
    bzero(&serve, sizeof(serve));
#endif
    serve.sin_family = AF_INET;
    serve.sin_addr.s_addr = htonl(INADDR_ANY);
    serve.sin_port = htons(port);
    if (bind(sock, (struct sockaddr *)&serve, sizeof(serve)) >= 0) {
        return sock;
    }
    return -1;
}

typedef struct {
  skode_t w;
  int in_use;
  int last_use;
} udp_state_t;

#define UDP_PORT_MAX (127)

static void *udp_main(void *arg) {
  if (udp_port <= 0) {
    return NULL;
  }
  int sock = udp_open(udp_port);
  if (sock < 0) {
    puts("# udp thread cannot run");
    return NULL;
  }
  util_set_thread_name("udp");
  struct sockaddr_in client;
#ifdef _WIN32
  int client_len = sizeof(client);
#else
  unsigned int client_len = sizeof(client);
#endif
  char line[1024];
  fd_set readfds;
  struct timeval timeout;
  udp_state_t user[UDP_PORT_MAX];
  for (int i = 0; i < UDP_PORT_MAX; i++) {
    skode_init(&user[i].w);
    user[i].in_use = 0;
    user[i].last_use = 0;
  }
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
        int which = get_connection_index(&client, UDP_PORT_MAX);
        skode_t *w = &user[which].w;
        if (w->udp == 0) {
          struct sockaddr_in *addr = &client;
          w->udp = 1;
          w->which = which;
          w->ip = addr->sin_addr.s_addr;
          w->port = addr->sin_port;
        }
        skode_consume(line, w);
        //
        if (w->log_len) {
          sendto(sock, w->log, w->log_len, 0, (struct sockaddr *)&client, client_len);
        }
        //
      } else {
        if (errno == EAGAIN) continue;
      }
    } else if (ready == 0) {
      // timeout
    } else {
      perror("# select");
    }
  }
  for (int i = 0; i < UDP_PORT_MAX; i++) {
    // need to free alloc-ed stuff here
  }
  return NULL;
}

int udp_start(int port) {
  if (port == 0) return 0;
  udp_port = port;
  udp_running = 1;
  pthread_attr_init(&udp_attr);
  pthread_attr_setstacksize(&udp_attr, 2 * 1024 * 1024);
  pthread_create(&udp_thread_handle, &udp_attr, udp_main, NULL);
  pthread_detach(udp_thread_handle);
  return port;
}

void udp_stop(void) {
  udp_running = 0;
#ifdef _WIN32
  WSACleanup();
#endif
}

int udp_info(void) {
  return udp_port;
}

//

int udp_send_to(int fd, void *c, int len, void *addr, unsigned int client_len) {
    struct sockaddr *client;
    client = addr;
    //printf("fd=%d c=%p len=%d client=%p client_len=%d\n",
    //    fd, c, len, client, client_len);
    int n = sendto(fd, c, len, 0, client, client_len);
    if (n < 0) {
        //printf("n = %d\n", n);
        perror("# sendto()");
    }
    return n;
}

#if 0
static struct sockaddr_in client;
static unsigned int client_len = sizeof(client);
static char client_read_something = 0;

int udp_send(int fd, void *c, int len) {
    if (client_read_something) {
        int n = sendto(fd, c, len, 0,
            (struct sockaddr *)&client,
            client_len);
        return n;
    }
    return 0;
}
#endif

