#ifndef _SKODE_H_
#define _SKODE_H_

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include <dirent.h>
#include <sys/types.h>

#define VOICE_STACK_LEN (8)

typedef struct {
  float s[VOICE_STACK_LEN];
  int ptr;
} voice_stack_t;

#include "ands.h"

#define SKODE_LOG_MAX (4096)

typedef struct skode_s {
  int voice;
  voice_stack_t stack;
  uint64_t defer_sample_time;
  float defer_last;
  int pattern;
  int step;
  int trace;
  int verbose;
  int events; // do incoming events go to the logger?
  ands_t *parse;
  int quit;
  int (*puts)(struct skode_s *w, const char *s);
  int (*printf)(struct skode_s *w, const char *fmt, ...);
  int log_enable;
  char log[SKODE_LOG_MAX + 1024];
  int log_max;
  int log_len;
  int flag;
  //
  int udp;
  int which;
  uint32_t ip;
  uint16_t port;
} skode_t;

int skode_consume(char *line, skode_t *w);
void show_threads(skode_t *w);
void system_show(skode_t *w);
int audio_show(skode_t *w);
int skode_load(skode_t *w, int voice, int n);
int wavetable_show(skode_t *w, int n);
char *skode_err_str(int n);

int skode_puts(skode_t *, const char *s);
int skode_printf(skode_t *, const char *fmt, ...);

int null_puts(const char *s);
int null_printf(const char *fmt, ...);

#define SKODE_EMPTY() { \
  .voice = 0, \
  .pattern = 0, \
  .step = -1, \
  .trace = 0, \
  .verbose = 0, \
  .events = 0, \
  .parse = NULL, \
  .quit = 0, \
  .puts = skode_puts, \
  .printf = skode_printf, \
  .log_enable = 0, \
  .log_len = 0, \
  .log_max = SKODE_LOG_MAX, \
}

//  .output = 0, 


void skode_init(skode_t *w);

#if 1

int perf_start(void);
void perf_stop(void);


#endif

#endif

