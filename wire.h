#ifndef _WIRE_H_
#define _WIRE_H_

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

typedef struct {
  int func;
  int sub_func;
  int next;
  int argc;
  float args[8];
} value_t;

#define WIRE_SCRATCH_MAX (1024)

#include "skode.h"

typedef struct wire_s {
  int voice;
  voice_stack_t stack;
#if 0
  int state;
  int scratch_pointer;
  float *data;
  int data_max;
  int data_pointer;
  int data_len;
  char data_acc[64];
  int data_acc_ptr;
#endif
  char queued[QUEUED_MAX];
  int queued_pointer;
  float defer_last;
  uint64_t defer_sample_time;
  int pattern;
  int step;
  int output;
  int trace;
  int verbose;
  char scratch[WIRE_SCRATCH_MAX];
  int events; // do incoming events go to the logger?
  skode_t *sk;
  int quit;
  int (*puts)(struct wire_s *w, const char *s);
  int (*printf)(struct wire_s *w, const char *fmt, ...);
  int log_enable;
  char log[4096];
  int log_max;
  int log_len;
} wire_t;

int wire(char *line, wire_t *w);
void show_threads(wire_t *w);
void system_show(wire_t *w);
int audio_show(wire_t *w);
int sk_load(wire_t *w, int voice, int n, int output);
int wavetable_show(wire_t *w, int n);
char *wire_err_str(int n);

int wire_puts(wire_t *, const char *s);
int wire_printf(wire_t *, const char *fmt, ...);

int null_puts(const char *s);
int null_printf(const char *fmt, ...);

#define WIRE() { \
  .voice = 0, \
  .scratch[0] = '\0', \
  .pattern = 0, \
  .step = -1, \
  .output = 0, \
  .trace = 0, \
  .verbose = 0, \
  .events = 0, \
  .sk = NULL, \
  .quit = 0, \
  .puts = wire_puts, \
  .printf = wire_printf, \
  .log_enable = 0, \
  .log_len = 0, \
  .log_max = 4096, \
}

void wire_init(wire_t *w);

#if 1

int perf_start(void);
void perf_stop(void);


#endif

#endif

