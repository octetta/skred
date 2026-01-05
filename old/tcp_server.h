#ifndef TCP_SERVER_H
#define TCP_SERVER_H

#include <netinet/in.h>
#include <stdbool.h>

typedef struct {
    void (*on_connect)(int fd, struct sockaddr_in *addr, void *user_data);
    void (*on_line)(int fd, char *line, void *user_data);
    void (*on_disconnect)(int fd, void *user_data);
} tcp_callbacks_t;

typedef struct tcp_server tcp_server_t;

// Create a new server instance with optional user data
tcp_server_t* tcp_server_create(int port, tcp_callbacks_t *callbacks, bool websocket, void *user_data);

// Run the server (blocking - call from a thread)
void tcp_server_run(tcp_server_t *server);

// Send a line to a client
void tcp_server_send_line(int fd, const char *line);

// Get/set user data
void* tcp_server_get_user_data(tcp_server_t *server);
void tcp_server_set_user_data(tcp_server_t *server, void *user_data);

// Stop and cleanup
void tcp_server_destroy(tcp_server_t *server);

#endif
