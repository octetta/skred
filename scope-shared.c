#include "scope-shared.h"

scope_buffer_t *scope_setup(scope_share_t *b, char *mode) {
  if (b == NULL) return NULL;
  if (mode == NULL) return NULL;
  if (mode[0] == 'w') {
    // WRITER
    int shm_fd = shm_open(SCOPE_NAME, O_CREAT | O_RDWR, 0666);
    if (shm_fd < 0) {
      perror("# shm_open failed");
      return NULL;
    }
    if (ftruncate(shm_fd, sizeof(scope_buffer_t)) != 0) {
      perror("# ftruncate failed");
      close(shm_fd);
      // shm_unlink(SCOPE_NAME);
      return NULL;
    }
    b->scope = mmap(NULL, sizeof(scope_buffer_t), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (b->scope == MAP_FAILED) {
      perror("mmap failed");
      close(shm_fd);
      // shm_unlink(SCOPE_NAME);
      return NULL;
    }
    b->shm_fd = shm_fd;
    if (mlock(b->scope, sizeof(scope_buffer_t)) == -1) {  // Prevent paging
      perror("mlock failed");  // Non-fatal, but log
    }
    return b->scope;
  } else if (mode[0] == 'r') {
    // READER
    int shm_fd = shm_open(SCOPE_NAME, O_RDWR, 0666);
    if (shm_fd < 0) {
      perror("shm_open failed");
      return NULL;
    }
    b->scope = mmap(NULL, sizeof(scope_buffer_t), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (b->scope == MAP_FAILED) {
      perror("mmap failed");
      close(shm_fd);
      return NULL;
    }
    b->shm_fd = shm_fd;
    return b->scope;
  } else {
    // WRONG!
    return NULL;
  }
}

void scope_cleanup(scope_share_t *b) {
  if (b == NULL) return;
  if (b->shm_fd >= 0) close(b->shm_fd);
  b->shm_fd = -1;
  munmap(b->scope, sizeof(scope_buffer_t));
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
  scope_share_t b;
  scope_buffer_t *scope = scope_setup(&b, "w");
  if (scope == NULL) {
    printf("!PROBLEM!\n");
    return;
  }
  signal(SIGINT, cleanup);
  signal(SIGTERM, cleanup);
  printf("# writer startup\n");
  int i = 0;
  for (i = 0; i < SCOPE_WIDTH_IN_SAMPLES; i++) scope->buffer_left[i] = 0;
  puts("# wait 2 seconds");
  sleep(2);
  float f = 0.1f;
  uint64_t t = 0;
  uint64_t c = 0;
  printf("# writer running\n");
#define WSLEEP (500) // 500 usec = .5 msec
  while (main_running) {
    scope->buffer_left[i % SCOPE_WIDTH_IN_SAMPLES] = f;
    f = rng_float();
    usleep(WSLEEP);
    t+=WSLEEP;
    c++;
    if ((t % 1000000) == 0) {
      printf("# 1sec, %ld total writes\n", c);
    }
    i++;
    if (i >= SCOPE_WIDTH_IN_SAMPLES) i = 0;
  }
  printf("# writer cleanup\n");
  scope_cleanup(&b);
}

void reader(void) {
  scope_share_t b;
  scope_buffer_t *scope = scope_setup(&b, "r");
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
    for (int i = 0; i < SCOPE_WIDTH_IN_SAMPLES; i++) {
      float f = scope->buffer_left[i];
      if (f < 0) minus++;
      if (f == 0) zero++;
      if (f > 0) plus++;
    }
    printf("# (<0)%d, (=0)%d (>0)%d\n", minus, zero, plus);
    sleep(1);
  }
  printf("# reader cleanup\n");
  scope_cleanup(&b);
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
