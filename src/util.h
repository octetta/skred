#ifndef _UTIL_H_
#define _UTIL_H_
void util_set_thread_name(char *s);

#define NS_TO_MS (1000000)
#define S_TO_MS (1000)

#include <time.h>

typedef struct {
  struct timespec a;
  struct timespec b;
  int64_t diff;
  int state;
  int order;
  int frames;
} sben_t;

#define BENLEN (4)

enum { BEN_0, BEN_A, BEN_B, BEN_D };

#define BEN_MARK_A(ben,benp,nf,bo) \
  { clock_gettime(BENCH_CLOCK, &ben[benp].a); \
  ben[benp].frames = nf; \
  ben[benp].order = bo; \
  ben[benp].state = BEN_A; }

#define BEN_MARK_B(bench, benchp, bencho) \
  { clock_gettime(BENCH_CLOCK, &bench[benchp].b); \
  bench[benchp].state = BEN_B; \
  bencho++; \
  benchp = ((bencho) % BENLEN); }

int64_t ts_diff_ns(const struct timespec *a, const struct timespec *b);

#define BENCH_CLOCK CLOCK_MONOTONIC

#endif
