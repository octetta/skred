#if 1

#include "scope-shared.h"

#else

#include <fcntl.h>
#include <signal.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#define SCOPE_SIZE (44100 * 2)

typedef struct {
  int writer_fd;
  int reader_fd;
  int size;
  int pointer;
  float data[SCOPE_SIZE];
  float mini[2048];
  char data_label[1024];
  float mini_label[80];
} scope_buffer_t;

#define SCOPE_NAME "skred.scope"

#endif

scope_buffer_t *scope_writer_setup(void) {
  int shm_fd = shm_open(SCOPE_NAME, O_CREAT | O_RDWR, 0666);
  if (shm_fd < 0) {
    perror("# shm_open failed");
    return NULL;
  }
  if (ftruncate(shm_fd, sizeof(scope_buffer_t)) == -1) {
    perror("# ftruncate failed");
    close(shm_fd);
    // shm_unlink(SCOPE_NAME);
    exit(1);
  }
  scope_buffer_t *scope = mmap(NULL, sizeof(scope_buffer_t), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
  if (scope == MAP_FAILED) {
    perror("mmap failed");
    close(shm_fd);
    // shm_unlink(SCOPE_NAME);
    exit(1);
  }
  scope->writer_fd = shm_fd;
  if (mlock(scope, sizeof(scope_buffer_t)) == -1) {  // Prevent paging
    perror("mlock failed");  // Non-fatal, but log
  }
  return scope;
}

void scope_writer_cleanup(scope_buffer_t *shared) {
  close(shared->writer_fd);
  munmap(shared, sizeof(scope_buffer_t));
}

scope_buffer_t *scope_reader_setup(void) {
  int shm_fd = shm_open(SCOPE_NAME, O_RDWR, 0666);
  if (shm_fd < 0) {
    perror("shm_open failed");
    return NULL;
  }
  scope_buffer_t *scope = mmap(NULL, sizeof(scope_buffer_t), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
  if (scope == MAP_FAILED) {
    perror("mmap failed");
    close(shm_fd);
  }
  scope->reader_fd = shm_fd;
  return scope;
}

void scope_reader_cleanup(scope_buffer_t *shared) {
  close(shared->reader_fd);
  munmap(shared, sizeof(scope_buffer_t));
}

#ifdef SCOPE_SHARED_DEMO

int main_running = 1;

void cleanup(int sig) {
  main_running = 0;
}

uint64_t rng_state = 1;

uint64_t rng_next(void) {
  rng_state = rng_state * 6364136223846793005ULL + 1442695040888963407ULL;
}

float rng_float(void) {
  uint64_t raw = rng_next();
  uint32_t val = (uint32_t)(raw >> 32);
  return (float)((int32_t)val) / 2147483648.0f;
}

void writer(void) {
  scope_buffer_t *scope = scope_writer_setup();
  if (scope == NULL) {
    printf("!PROBLEM!\n");
    return;
  }
  signal(SIGINT, cleanup);
  signal(SIGTERM, cleanup);
  printf("# writer startup\n");
  int i = 0;
  for (i = 0; i < SCOPE_SIZE; i++) scope->data[i] = 0;
  puts("# wait 2 seconds");
  sleep(2);
  float f = 0.1f;
  uint64_t t = 0;
  uint64_t c = 0;
  printf("# writer running\n");
#define WSLEEP (500) // 500 usec = .5 msec
  while (main_running) {
    scope->data[i % SCOPE_SIZE] = f;
    f = rng_float();
    usleep(WSLEEP);
    t+=WSLEEP;
    c++;
    if ((t % 1000000) == 0) {
      printf("# 1sec, %ld total writes\n", c);
    }
    i++;
    if (i >= SCOPE_SIZE) i = 0;
  }
  printf("# writer cleanup\n");
  scope_writer_cleanup(scope);
}

void reader(void) {
  scope_buffer_t *scope = scope_reader_setup();
  if (scope == NULL) {
    printf("!PROBLEM!\n");
    return;
  }
  signal(SIGINT, cleanup);
  signal(SIGTERM, cleanup);
  printf("# reader startup\n");
  while (main_running) {
    int plus = 0;
    int minus = 0;
    int zero = 0;
    for (int i = 0; i < SCOPE_SIZE; i++) {
      float f = scope->data[i];
      if (f < 0) minus++;
      if (f == 0) zero++;
      if (f > 0) plus++;
    }
    printf("# (<0)%d, (=0)%d (>0)%d\n", minus, zero, plus);
    sleep(1);
  }
  printf("# reader cleanup\n");
  scope_reader_cleanup(scope);
}

int main(int argc, char *argv[]) {
  if (argc > 1 && argv[1][0] == 'r') {
    reader();
  } else {
    writer();
  }
  return 0;
}

#endif
