#include "tcp_server.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <arpa/inet.h>
#include <openssl/sha.h>
#include <openssl/bio.h>
#include <openssl/evp.h>
#include <openssl/buffer.h>

#define MAX_CLIENTS 30
#define BUFFER_SIZE 4096
#define LINE_SIZE 2048

typedef struct {
    char line_buf[LINE_SIZE];
    int line_len;
    bool is_websocket;
    bool handshake_done;
    char handshake_buf[2048];
    int handshake_len;
} client_state_t;

struct tcp_server {
    int server_fd;
    int port;
    int running;
    bool websocket_mode;
    tcp_callbacks_t callbacks;
    int client_fds[MAX_CLIENTS];
    client_state_t client_states[MAX_CLIENTS];
    void *user_data;
};

// Base64 encode
static char* base64_encode(const unsigned char *input, int length) {
    BIO *bmem, *b64;
    BUF_MEM *bptr;
    
    b64 = BIO_new(BIO_f_base64());
    bmem = BIO_new(BIO_s_mem());
    b64 = BIO_push(b64, bmem);
    BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL);
    BIO_write(b64, input, length);
    BIO_flush(b64);
    BIO_get_mem_ptr(b64, &bptr);
    
    char *buff = malloc(bptr->length + 1);
    memcpy(buff, bptr->data, bptr->length);
    buff[bptr->length] = 0;
    
    BIO_free_all(b64);
    return buff;
}

// WebSocket handshake
static bool handle_websocket_handshake(int fd, client_state_t *state, char *data, int len) {
    // Accumulate handshake data
    if(state->handshake_len + len < sizeof(state->handshake_buf)) {
        memcpy(state->handshake_buf + state->handshake_len, data, len);
        state->handshake_len += len;
        state->handshake_buf[state->handshake_len] = '\0';
    }
    
    // Check if we have complete headers
    if(strstr(state->handshake_buf, "\r\n\r\n") == NULL) {
        return false; // Need more data
    }
    
    // Find Sec-WebSocket-Key
    char *key_start = strstr(state->handshake_buf, "Sec-WebSocket-Key: ");
    if(!key_start) return false;
    
    key_start += 19;
    char *key_end = strstr(key_start, "\r\n");
    if(!key_end) return false;
    
    char key[256];
    int key_len = key_end - key_start;
    memcpy(key, key_start, key_len);
    key[key_len] = '\0';
    
    // Create accept key
    char accept_source[512];
    snprintf(accept_source, sizeof(accept_source), "%s258EAFA5-E914-47DA-95CA-C5AB0DC85B11", key);
    
    unsigned char sha1_result[SHA_DIGEST_LENGTH];
    SHA1((unsigned char*)accept_source, strlen(accept_source), sha1_result);
    
    char *accept_key = base64_encode(sha1_result, SHA_DIGEST_LENGTH);
    
    // Send handshake response
    char response[512];
    snprintf(response, sizeof(response),
        "HTTP/1.1 101 Switching Protocols\r\n"
        "Upgrade: websocket\r\n"
        "Connection: Upgrade\r\n"
        "Sec-WebSocket-Accept: %s\r\n\r\n",
        accept_key);
    
    send(fd, response, strlen(response), 0);
    free(accept_key);
    
    state->handshake_done = true;
    return true;
}

// Decode WebSocket frame
static int websocket_decode_frame(unsigned char *data, int len, char *output, int output_size) {
    if(len < 2) return 0;
    
    bool fin = (data[0] & 0x80) != 0;
    int opcode = data[0] & 0x0F;
    bool masked = (data[1] & 0x80) != 0;
    uint64_t payload_len = data[1] & 0x7F;
    
    int offset = 2;
    
    // Handle extended payload length
    if(payload_len == 126) {
        if(len < 4) return 0;
        payload_len = (data[2] << 8) | data[3];
        offset = 4;
    } else if(payload_len == 127) {
        if(len < 10) return 0;
        payload_len = 0;
        for(int i = 0; i < 8; i++) {
            payload_len = (payload_len << 8) | data[2 + i];
        }
        offset = 10;
    }
    
    if(!masked) return 0; // Client frames must be masked
    
    unsigned char mask[4];
    memcpy(mask, data + offset, 4);
    offset += 4;
    
    if(len < offset + payload_len) return 0; // Incomplete frame
    
    // Decode payload
    int out_len = (payload_len < output_size - 1) ? payload_len : output_size - 1;
    for(int i = 0; i < out_len; i++) {
        output[i] = data[offset + i] ^ mask[i % 4];
    }
    output[out_len] = '\0';
    
    return offset + payload_len; // Return total bytes consumed
}

// Encode WebSocket frame
static void websocket_send_frame(int fd, const char *message) {
    int len = strlen(message);
    unsigned char frame[10];
    int frame_len = 0;
    
    frame[0] = 0x81; // FIN + text frame
    
    if(len < 126) {
        frame[1] = len;
        frame_len = 2;
    } else if(len < 65536) {
        frame[1] = 126;
        frame[2] = (len >> 8) & 0xFF;
        frame[3] = len & 0xFF;
        frame_len = 4;
    } else {
        frame[1] = 127;
        for(int i = 0; i < 8; i++) {
            frame[2 + i] = (len >> (56 - i * 8)) & 0xFF;
        }
        frame_len = 10;
    }
    
    send(fd, frame, frame_len, 0);
    send(fd, message, len, 0);
}

tcp_server_t* tcp_server_create(int port, tcp_callbacks_t *callbacks, bool websocket, void *user_data) {
    tcp_server_t *server = calloc(1, sizeof(tcp_server_t));
    if(!server) return NULL;
    
    server->port = port;
    server->running = 0;
    server->websocket_mode = websocket;
    server->user_data = user_data;
    if(callbacks) {
        server->callbacks = *callbacks;
    }
    
    return server;
}

void* tcp_server_get_user_data(tcp_server_t *server) {
    return server ? server->user_data : NULL;
}

void tcp_server_set_user_data(tcp_server_t *server, void *user_data) {
    if(server) {
        server->user_data = user_data;
    }
}

void tcp_server_send_line(int fd, const char *line) {
    if(fd <= 0) return;
    websocket_send_frame(fd, line);
}

void tcp_server_run(tcp_server_t *server) {
    if(!server) return;
    
    struct sockaddr_in addr;
    fd_set readfds;
    unsigned char buffer[BUFFER_SIZE];
    int max_sd, activity, new_socket, valread;
    
    server->server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if(server->server_fd < 0) {
        perror("socket failed");
        return;
    }
    
    int opt = 1;
    setsockopt(server->server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(server->port);
    
    if(bind(server->server_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind failed");
        close(server->server_fd);
        return;
    }
    
    if(listen(server->server_fd, 3) < 0) {
        perror("listen failed");
        close(server->server_fd);
        return;
    }
    
    printf("TCP server listening on port %d (WebSocket: %s)\n", 
           server->port, server->websocket_mode ? "yes" : "no");
    server->running = 1;
    
    while(server->running) {
        FD_ZERO(&readfds);
        FD_SET(server->server_fd, &readfds);
        max_sd = server->server_fd;
        
        for(int i = 0; i < MAX_CLIENTS; i++) {
            int sd = server->client_fds[i];
            if(sd > 0) FD_SET(sd, &readfds);
            if(sd > max_sd) max_sd = sd;
        }
        
        struct timeval timeout = {1, 0};
        activity = select(max_sd + 1, &readfds, NULL, NULL, &timeout);
        
        if(activity < 0) continue;
        
        if(FD_ISSET(server->server_fd, &readfds)) {
            socklen_t addrlen = sizeof(addr);
            new_socket = accept(server->server_fd, (struct sockaddr *)&addr, &addrlen);
            
            if(new_socket >= 0) {
                for(int i = 0; i < MAX_CLIENTS; i++) {
                    if(server->client_fds[i] == 0) {
                        server->client_fds[i] = new_socket;
                        memset(&server->client_states[i], 0, sizeof(client_state_t));
                        server->client_states[i].is_websocket = server->websocket_mode;
                        
                        if(!server->websocket_mode && server->callbacks.on_connect) {
                            server->callbacks.on_connect(new_socket, &addr, server->user_data);
                        }
                        break;
                    }
                }
            }
        }
        
        for(int i = 0; i < MAX_CLIENTS; i++) {
            int sd = server->client_fds[i];
            
            if(sd > 0 && FD_ISSET(sd, &readfds)) {
                valread = read(sd, buffer, BUFFER_SIZE);
                
                if(valread <= 0) {
                    if(server->callbacks.on_disconnect) {
                        server->callbacks.on_disconnect(sd, server->user_data);
                    }
                    close(sd);
                    server->client_fds[i] = 0;
                } else {
                    client_state_t *state = &server->client_states[i];
                    
                    if(state->is_websocket && !state->handshake_done) {
                        if(handle_websocket_handshake(sd, state, (char*)buffer, valread)) {
                            if(server->callbacks.on_connect) {
                                server->callbacks.on_connect(sd, &addr, server->user_data);
                            }
                        }
                    } else if(state->is_websocket && state->handshake_done) {
                        char decoded[BUFFER_SIZE];
                        int consumed = websocket_decode_frame(buffer, valread, decoded, sizeof(decoded));
                        
                        if(consumed > 0) {
                            // Process decoded message for lines
                            for(int j = 0; decoded[j]; j++) {
                                char c = decoded[j];
                                
                                if(c == '\n' || c == '\r') {
                                    if(state->line_len > 0) {
                                        state->line_buf[state->line_len] = '\0';
                                        if(server->callbacks.on_line) {
                                            server->callbacks.on_line(sd, state->line_buf, server->user_data);
                                        }
                                        state->line_len = 0;
                                    }
                                } else {
                                    if(state->line_len < LINE_SIZE - 1) {
                                        state->line_buf[state->line_len++] = c;
                                    }
                                }
                            }
                        }
                    } else {
                        // Plain TCP mode
                        for(int j = 0; j < valread; j++) {
                            char c = buffer[j];
                            
                            if(c == '\n' || c == '\r') {
                                if(state->line_len > 0) {
                                    state->line_buf[state->line_len] = '\0';
                                    if(server->callbacks.on_line) {
                                        server->callbacks.on_line(sd, state->line_buf, server->user_data);
                                    }
                                    state->line_len = 0;
                                }
                            } else {
                                if(state->line_len < LINE_SIZE - 1) {
                                    state->line_buf[state->line_len++] = c;
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    
    for(int i = 0; i < MAX_CLIENTS; i++) {
        if(server->client_fds[i] > 0) {
            close(server->client_fds[i]);
        }
    }
    close(server->server_fd);
}

void tcp_server_destroy(tcp_server_t *server) {
    if(!server) return;
    server->running = 0;
    sleep(1);
    free(server);
}
