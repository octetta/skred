#include <unistd.h>
#include <sys/syscall.h>
#include <linux/futex.h>
#include <errno.h>
#include <stdatomic.h>

static volatile int g_futex = 0;
_Atomic int futex_var = 0;

void futex_wait(int *addr, int val) {
  // Wait as long as g_futex is 0.
  syscall(SYS_futex, addr, FUTEX_WAIT, val, NULL, NULL, 0);
}

void futex_wake(int *addr) {
  syscall(SYS_futex, addr, FUTEX_WAKE, 1, NULL, NULL, 0);
}

#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <sys/timerfd.h>
#include <errno.h>
#include <stdlib.h>
#include <pthread.h>

void motor_opts(int method, int priority) {
}

pthread_t motor_thread_id;
int motor_ms;
void (*motor_work)(void) = NULL;

static int motor_running = 1;
void motor_fini(void) {
  motor_running = 0;
}

void *motor_thread(void *v) {
  pthread_setname_np(pthread_self(), "motor");

  while (motor_running) {
      int expected = 0;
      if (atomic_load(&futex_var) == 0) {
        futex_wait((int *)&futex_var, 0);
      }
      if (atomic_exchange(&futex_var, 0) == 1) {
        if (motor_work) motor_work();
      }
  }
  return NULL;
}

void motor_trigger(void) {
  atomic_store(&futex_var, 1);
  futex_wake((int *)&futex_var);
}

int motor_init(int ms, void (*work)(void)) {
  printf("# motor_init()\n");
  motor_ms = ms;
  motor_work = work;
  pthread_create(&motor_thread_id, NULL, motor_thread, NULL);
  pthread_detach(motor_thread_id);
}
