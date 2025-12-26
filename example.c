#include "tcp_server.h"
#include <pthread.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>

// Example user data structure
typedef struct {
    int message_count;
    char app_name[64];
    pthread_mutex_t lock;
} app_data_t;

void on_connect(int fd, struct sockaddr_in *addr, void *user_data) {
    app_data_t *app = (app_data_t*)user_data;
    printf("Client connected to %s: fd=%d\n", app->app_name, fd);
    
    char welcome[256];
    snprintf(welcome, sizeof(welcome), "Welcome to %s!", app->app_name);
    tcp_server_send_line(fd, welcome);
}

void on_line(int fd, char *line, void *user_data) {
    app_data_t *app = (app_data_t*)user_data;
    
    pthread_mutex_lock(&app->lock);
    app->message_count++;
    int count = app->message_count;
    pthread_mutex_unlock(&app->lock);
    
    printf("[%s] Message #%d: %s\n", app->app_name, count, line);
    
    char response[512];
    snprintf(response, sizeof(response), 
             "[%s] Echo #%d: %s", app->app_name, count, line);
    tcp_server_send_line(fd, response);
}

void on_disconnect(int fd, void *user_data) {
    app_data_t *app = (app_data_t*)user_data;
    printf("[%s] Client disconnected: fd=%d\n", app->app_name, fd);
}

void* server_thread(void *arg) {
    tcp_server_t *server = (tcp_server_t*)arg;
    tcp_server_run(server);
    return NULL;
}

int main() {
    // Initialize user data
    app_data_t app_data = {
        .message_count = 0,
        .app_name = "MyAwesomeApp"
    };
    pthread_mutex_init(&app_data.lock, NULL);
    
    tcp_callbacks_t callbacks = {
        .on_connect = on_connect,
        .on_line = on_line,
        .on_disconnect = on_disconnect
    };
    
    // Create server with user data
    tcp_server_t *server = tcp_server_create(8888, &callbacks, true, &app_data);
    
    pthread_t thread;
    pthread_create(&thread, NULL, server_thread, server);
    
    // Main program can access and modify user data
    sleep(10);
    pthread_mutex_lock(&app_data.lock);
    printf("Total messages so far: %d\n", app_data.message_count);
    pthread_mutex_unlock(&app_data.lock);
    
    sleep(300);
    
    tcp_server_destroy(server);
    pthread_join(thread, NULL);
    pthread_mutex_destroy(&app_data.lock);
    
    return 0;
}
