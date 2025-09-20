#include <fcntl.h>
#include <signal.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#define SCOPE_SIZE (44100 * 2)

typedef struct {
  int fd;
  int size;
  int pointer;
  float data[SCOPE_SIZE];
  float mini[2048];
  char data_label[1024];
  float mini_label[80];
} scope_buffer_t;

#define SCOPE_NAME "skred.scope"

scope_buffer_t *scope_buffer_setup(void) {
  int shm_fd = shm_open(SCOPE_NAME, O_CREAT | O_RDWR, 0666);
  if (shm_fd == -1) {
    perror("shm_open failed");
    exit(1);  // Or handle retry/recovery
  }
  if (ftruncate(shm_fd, sizeof(scope_buffer_t)) == -1) {
    perror("ftruncate failed");
    close(shm_fd);
    shm_unlink(SCOPE_NAME);
    exit(1);
  }
  scope_buffer_t *scope = mmap(NULL, sizeof(scope_buffer_t), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
  if (scope == MAP_FAILED) {
    perror("mmap failed");
    close(shm_fd);
    shm_unlink(SCOPE_NAME);
    exit(1);
  }
  scope->fd = shm_fd;
  if (mlock(scope, sizeof(scope_buffer_t)) == -1) {  // Prevent paging
    perror("mlock failed");  // Non-fatal, but log
  }
  return scope;
}

void scope_buffer_cleanup(scope_buffer_t *shared) {
  close(shared->fd);
  munmap(shared, sizeof(scope_buffer_t));
}

int main_running = 1;

void cleanup(int sig) {
  main_running = 0;
}

int main(int argc, char *argv[]) {
  scope_buffer_t *scope = scope_buffer_setup();
  if (scope == NULL) {
    printf("!PROBLEM!\n");
    exit(1);
  }
  signal(SIGINT, cleanup);
  signal(SIGTERM, cleanup);
  printf("# startup\n");
  int i = 0;
  float f = 0.1f;
  printf("# running\n");
  while (main_running) {
    scope->data[i] = f;
    f += .1357281033;
    usleep(500000);
    i++;
    if (i > SCOPE_SIZE) i = 0;
  }
  printf("# cleanup\n");
  scope_buffer_cleanup(scope);
  return 0;
}
