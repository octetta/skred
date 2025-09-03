#include <stdio.h>
#include <string.h>
#include "linenoise.h"

#include "parse.h"

#define HISTORY_FILE ".scanner-history"

char *ignore = " \t\r\n;";

typedef struct {
  int func;
  int subfunc;
  int next;
  int argc;
  float args[8];
} value_t;

int debug = 1;
int trace = 1;

void dump(value_t v) {
  printf("# %s", func_func_str(v.func));
  if (v.subfunc != FUNC_NULL) printf(" %s", func_func_str(v.subfunc));
  printf(" [");
  for (int i=0; i<v.argc; i++) {
    if (i) printf(" ");
    printf("%g", v.args[i]);
  }
  puts("]");
}

#define ERR_INVALID_VOICE 100

#define VOICE_MAX (32)

float amp[VOICE_MAX];
float freq[VOICE_MAX];
int fmod_osc[VOICE_MAX];

float fmod_depth[VOICE_MAX];

void show_voice(int v, int c) {
  printf("# v%d f%g a%g F%d,%g\n", v, freq[v], amp[v], fmod_osc[v], fmod_depth[v]);
}

value_t parse(char *ptr, int func, int subfunc, int argc, int multi) {
  value_t v;
  v.func = func;
  v.subfunc = subfunc;
  int limit = argc;
  if (multi) limit = argc;
  for (int i = argc; i >= limit; i--) {
    switch (i) {
      case 1:
        v.argc = sscanf(ptr, "%g%n", &v.args[0], &v.next);
        break;
      case 2:
        v.argc = sscanf(ptr, "%g,%g%n", &v.args[0], &v.args[1], &v.next);
        break;
      case 3:
        v.argc = sscanf(ptr, "%g,%g,%g%n", &v.args[0], &v.args[1], &v.args[2], &v.next);
        break;
      case 4:
        v.argc = sscanf(ptr, "%g,%g,%g,%g%n", &v.args[0], &v.args[1], &v.args[2], &v.args[3], &v.next);
        break;
      default:
        v.argc = 0;
        v.next = 0;
        break;
    }
    if (argc == v.argc) printf("# yes %d\n", argc); break;
  }
  if (debug) {
    printf("# argc:%d next:%d", v.argc, v.next);
    puts("");
  }
  if (trace) dump(v);

  return v;
}

int wire(char *line, int *this_voice, int output) {
  int len = strlen(line);
  char *ptr = line;
  
  int func;
  int subfunc;
  
  value_t v;
  int voice = 0;
  if (this_voice) voice = *this_voice;
  
  int more = 1;
  int status = 0;
  
  int n;
  int r = 0;
  
  while (more) {
    if (*ptr == '\0') break;
    // skip whitespace and semi-colons
    ptr += strspn(ptr, ignore);
    if (debug) printf("# [%ld] '%c' (%d)\n", ptr-line, *ptr, *ptr);
    switch (*ptr++) {
      case '\0':
        puts("# NULL!");
        break;
      case ':':
        puts("# COLON");
        switch (*ptr++) {
          case '\0': return 100;
          case 'q': return -1;
          case 't':
            if (*ptr == '0') {
              trace = 0;
              ptr++;
            } else if (*ptr == '1') {
              trace = 1;
              ptr++;
            } else return 888;
            break;
          case 'd':
            if (*ptr == '0') {
              debug = 0;
              ptr++;
            } else if (*ptr == '1') {
              debug = 1;
              ptr++;
            } else return 888;
            break;
          default: return 999;
        }
        break;
      case '?':
        show_voice(voice, 1);
        break;
      case 'a':
        v = parse(ptr, FUNC_AMP, FUNC_NULL, 1, 0);
        if (v.argc == 1) {
          amp[voice] = v.args[0];
          ptr += v.next;
        }
        break;
      case 'f':
        v = parse(ptr, FUNC_FREQ, FUNC_NULL, 1, 0);
        if (v.argc == 1) {
          freq[voice] = v.args[0];
          ptr += v.next;
        }
        break;
      case 'v':
        v = parse(ptr, FUNC_VOICE, FUNC_NULL, 1, 0);
        if (v.argc == 1) {
          n = (int)v.args[0];
          if (n >= 0 && n < VOICE_MAX) voice = n;
          else return ERR_INVALID_VOICE;
          ptr += v.next;
        } else return ERR_INVALID_VOICE;
        break;
      case 'F':
        v = parse(ptr, FUNC_FMOD, FUNC_NULL, 2, 1);
        if (v.argc == 2) {
          fmod_osc[voice] = v.args[0];
          fmod_depth[voice] = v.args[1];
          ptr += v.next;
        } else if (v.argc == 1) {
          fmod_osc[voice] = -1;
          fmod_depth[voice] = 0;
          ptr += v.next;
        } else return 1000;
        break;
      default:
        printf("# not sure\n");
        more = 0;
        break;
    }
  }
  if (this_voice) *this_voice = voice;
  return 0;
}

int main_running = 1;
int current_voice = 0;

int main(int argc, char *argv[]) {
  linenoiseHistoryLoad(HISTORY_FILE);
  while (main_running) {
    char *line = linenoise("# ");
    if (line == NULL) {
      main_running = 0;
      break;
    }
    if (strlen(line) == 0) continue;
    linenoiseHistoryAdd(line);
    int n = wire(line, &current_voice, 1);
    if (n < 0) break; // request to stop or error
    if (n > 0) {
      printf("??? %d\n", n);
    }
    linenoiseFree(line);
  }
  linenoiseHistorySave(HISTORY_FILE);
  return 0;
}