#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <stdint.h>

#include "motor.h"

//#define _GNU_SOURCE
#include <linux/futex.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <errno.h>
#include <stdatomic.h>

// Futex wait: block until *addr != val
int futex_wait(_Atomic int *addr, int val) {
  return syscall(SYS_futex, addr, FUTEX_WAIT, val, NULL, NULL, 0);
}

// Futex wake: wake up n waiters (usually 1)
int futex_wake(_Atomic int *addr, int n) {
  return syscall(SYS_futex, addr, FUTEX_WAKE, n, NULL, NULL, 0);
}

int64_t nsec_now(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC_RAW, &ts);
    return (int64_t)ts.tv_sec * 1000000000LL + ts.tv_nsec;
}

void motor_wake_tick(void) {
}

pthread_t motor_thread_id;

void (*motor_work)(void) = NULL;

static int motor_running = 1;

void motor_fini(void) {
  motor_running = 0;
}

double current_period = 0.04;

void motor_update(double ms) {
  current_period = ms / 1000.0;
}

static inline int64_t ms_to_ns(double ms) { return (int64_t)(ms * 1e6); }
static inline int64_t get_time_ns() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (int64_t)ts.tv_sec * 1000000000LL + ts.tv_nsec;
}

_Atomic int futex_var = 0;

void *motor_thread(void *v) {
  pthread_setname_np(pthread_self(), "motor");
  int64_t next_time = get_time_ns();
  int local;
  while (motor_running) {
    local = futex_var;
    futex_wait(&futex_var, local);
    if (motor_work) motor_work();
  }
  printf("# motor_thread is stopping\n");
  return NULL;
}

int motor_init(void (*work)(void)) {
  printf("# motor_init()\n");
  motor_work = work;
}

int motor_go(void) {
  pthread_create(&motor_thread_id, NULL, motor_thread, NULL);
  pthread_detach(motor_thread_id);
}

