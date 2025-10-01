#ifndef _SCOPE_SHARED_H_
#define _SCOPE_SHARED_H_

#include <fcntl.h>
#include <signal.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include "skred.h"

#define SCOPE_WIDTH_IN_SAMPLES (44100 * 2)
#define SCOPE_WIDTH_IN_PIXELS (800)
#define SCOPE_HEIGHT_IN_PIXELS (480)
#define SCOPE_WAVE_WIDTH (SCOPE_WIDTH_IN_PIXELS / 4)
#define SCOPE_WAVE_HEIGHT (SCOPE_HEIGHT_IN_PIXELS / 2)

/*
wave

----

voice
scope               status
*/

typedef struct {
  int buffer_len;
  int buffer_pointer;
  int wave_len;
  float wave_data[SCOPE_WAVE_WIDTH];
  float wave_min[SCOPE_WAVE_WIDTH];
  float wave_max[SCOPE_WAVE_WIDTH];
  float buffer_left[SCOPE_WIDTH_IN_SAMPLES];
  float buffer_right[SCOPE_WIDTH_IN_SAMPLES];
  float mini[2048];
  char wave_text[1024];
  char voice_text[1024];
  char status_text[1024];
  char debug_text[1024];
} scope_buffer_t;

typedef struct {
  int shm_fd;
  scope_buffer_t *scope;
} scope_share_t;

#define SCOPE_NAME "skred.scope"

scope_buffer_t *scope_setup(scope_share_t *b, char *mode);
void scope_cleanup(scope_share_t *b);
#endif
