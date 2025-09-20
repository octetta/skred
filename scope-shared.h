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
