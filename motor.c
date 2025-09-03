#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <sys/timerfd.h>
#include <errno.h>
#include <stdlib.h>
#include <pthread.h>

pthread_t motor_thread_id;
int motor_ms;
void (*motor_work)(void) = NULL;

static int motor_running = 1;
void motor_fini(void) {
  motor_running = 0;
}

static void set_timer_ms(int tfd, int ms) {
  struct itimerspec its;
  memset(&its, 0, sizeof(its));

  its.it_value.tv_sec  = ms / 1000;
  its.it_value.tv_nsec = (ms % 1000) * 1000000;
  its.it_interval = its.it_value;  // make it periodic

  if (timerfd_settime(tfd, 0, &its, NULL) < 0) {
    perror("timerfd_settime");
    exit(1);
  }
}

static void add_ms(struct timespec *t, int ms) {
  t->tv_sec  += ms / 1000;
  t->tv_nsec += (ms % 1000) * 1000000L;
  if (t->tv_nsec >= 1000000000L) {
      t->tv_sec++;
      t->tv_nsec -= 1000000000L;
  }
}

// arm timerfd with an absolute expiration
static void arm_absolute(int tfd, struct timespec *next) {
  struct itimerspec its;
  memset(&its, 0, sizeof(its));
  its.it_value = *next;  // absolute expiration time
  if (timerfd_settime(tfd, TFD_TIMER_ABSTIME, &its, NULL) < 0) {
      perror("timerfd_settime");
      exit(1);
  }
}

void motor_update(int ms) {
  motor_ms = ms;
}

#include <poll.h>

void *motor_thread(void *v) {
  pthread_setname_np(pthread_self(), "motor");
  
  int tfd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK);
  if (tfd < 0) {
    perror("# create");
    return NULL;
  }
  
  struct timespec next;
  clock_gettime(CLOCK_MONOTONIC, &next);

  int interval = motor_ms;
  add_ms(&next, interval);
  arm_absolute(tfd, &next);

  struct pollfd pfd = { .fd = tfd, .events = POLLIN };

  //set_timer_ms(tfd, my_ms);

  uint64_t expire;

  while (motor_running) {
    int r = poll(&pfd, 1, -1);
    if (r > 0 && (pfd.revents & POLLIN)) {
      read(tfd, &expire, sizeof(expire));
      if (expire != 1) {
        printf("# expire -> %ld\n", expire);
      }
      for (uint64_t e = 0; e < expire; e++) {
        if (motor_work) motor_work();
      }
    }
    if (interval != motor_ms) {
      interval = motor_ms;
      //set_timer_ms(tfd, interval);
    }

    add_ms(&next, interval);
    arm_absolute(tfd, &next);
  }
  //read(tfd, &expire, sizeof(expire));
  close(tfd);
  return NULL;
}

int motor_init(int ms, void (*work)(void)) {
  printf("# motor_init()\n");
  motor_ms = ms;
  motor_work = work;
  pthread_create(&motor_thread_id, NULL, motor_thread, NULL);
  pthread_detach(motor_thread_id);
}
