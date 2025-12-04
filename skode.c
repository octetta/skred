#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "linenoise.h"

#define HISTORY_FILE ".skode_history"

#define IS_NUMBER(c) (isdigit(c) || strchr("-.", c))
#define IS_SEPARATOR(c) (isspace(c) || c == ',')
#define IS_STRING(c) (c == '{')
#define IS_STRING_END(c) (c == '}')
#define IS_ARRAY(c) (c == '(')
#define IS_ARRAY_END(c) (c == ')')
#define IS_VARIABLE(c) (c == '$')
#define IS_COMMENT(c) (c == '#')
#define IS_CHUNK_END(c) (c == ';' || c == 0x04) // 0x04 ASCII EOT / end of xmit
#define IS_DEFER(c) (c == '+' || c == '~')
// used above 0-9 - . , { } ( ) $ # ; + ~
#define IS_ATOM(c) (isalpha(c) || strchr("!@%^&*_=:\"'<>[]?/", c))
// used by array
//#define IS_NUMBER_EX(c) (isdigit(c) || strchr("-.eE", c))
#define IS_NUMBER_EX(c) (isxdigit(c) || strchr("-.eExX", c))

enum {
  START = 0, // 0
  GET_NUMBER,
  GET_VARIABLE,
  GET_DEFER_NUMBER,
  GET_DEFER_STRING,
  GET_ATOM,
  GET_STRING,
  GET_ARRAY,
  GET_COMMENT,
  CHUNK_END,
  DECIDE,
  ERROR,
};

#define GET_STRING_MAX (1024)

double skode_strtod(char *s) {
  double d = NAN;
  if (s[1] == '\0' && (s[0] == '-' || s[0] == 'e' || s[0] == '.')) return d;
  d = strtod(s, NULL);
  return d;
}

#define ARG_MAX (8)
#define NUM_ACC_MAX (1024)
#define ATOM_MAX (4)
#define ATOM_NIL (0x5f5f5f5f)

typedef struct skode_s {
  // scratch string
  char *scr_acc;
  int scr_len;
  int scr_cap;
  // number keeper
  char num_acc[NUM_ACC_MAX];
  int num_len;
  int num_cap;
  // data array
  double *data;
  int data_len;
  int data_cap;
  // defer stuff
  char *defer_acc;
  int defer_len;
  int defer_cap;
  double defer_num;
  char defer_mode;
  //
  double arg[ARG_MAX];
  int arg_len;
  int arg_cap;
  //
  char atom_acc[ATOM_MAX + 1];
  int atom_cap;
  int atom_len;
  int atom_num;
  //
  int state;
  void *user;
} skode_t;

skode_t *skode_new(void *user) {
  skode_t *s = (skode_t*)malloc(sizeof(skode_t));
  s->scr_cap = 1024;
  s->scr_len = 0;
  s->scr_acc = (char *)malloc(s->scr_cap * sizeof(char));
  //
  s->num_cap = 1024;
  s->num_len = 0;
  //
  s->data_cap = 1024;
  s->data_len = 0;
  s->data = (double *)malloc(s->data_cap * sizeof(double));
  //
  s->defer_cap = 1024;
  s->defer_len = 0;
  s->defer_acc = (char *)malloc(s->defer_cap * sizeof(char));
  s->defer_num = 0;
  s->defer_mode = '?';
  //
  s->arg_cap = ARG_MAX;
  s->arg_len = 0;
  //
  s->atom_cap = ATOM_MAX;
  s->atom_len = 0;
  s->atom_num = ATOM_NIL;
  //
  s->user = user;
  return s;
}

void skode_free(skode_t *s) {
  if (s->scr_acc) free(s->scr_acc);
  s->scr_acc = NULL;
  s->scr_cap = 0;
  s->scr_len = 0;
  //
  if (s->data) free(s->data);
  s->data = NULL;
  s->data_cap = 0;
  s->data_len = 0;
  //
  if (s->defer_acc) free(s->defer_acc);
  s->defer_acc = NULL;
  s->defer_cap = 0;
  s->defer_len = 0;
}

//

void string_clear(skode_t *s) {
  s->scr_len = 0;
  s->scr_acc[s->scr_len] = '\0';
}

void string_push(skode_t *s, char c) {
  if (s->scr_len < s->scr_cap) {
    s->scr_acc[s->scr_len++] = c;
    s->scr_acc[s->scr_len] = '\0';
  }
}

void num_clear(skode_t *s) {
  s->num_len = 0;
  s->num_acc[s->num_len] = '\0';
}

void num_push(skode_t *s, char c) {
  if (s->num_len < s->num_cap) {
    s->num_acc[s->num_len++] = c;
    s->num_acc[s->num_len] = '\0';
  }
}

double num_get(skode_t *s) {
  return skode_strtod(s->num_acc);
}

void array_clear(skode_t *s) {
  s->data_len = 0;
}

void array_push(skode_t *s) {
  if (s->num_len) s->data[s->data_len++] = num_get(s);
  num_clear(s);
}

void arg_clear(skode_t *s)  { s->arg_len = 0; }
void arg_push(skode_t *s, double d) { if (s->arg_len < s->arg_cap) s->arg[s->arg_len++] = d; }

void defer_clear(skode_t *s) {
  s->defer_len = 0;
  s->defer_acc[s->defer_len] = '\0';
}

void defer_push(skode_t *s, char c) {
  if (s->defer_len < s->defer_cap) {
    s->defer_acc[s->defer_len++] = c;
    s->defer_acc[s->defer_len] = '\0';
  }
}

void atom_clear(skode_t *s) {
  s->atom_len = 0;
  s->atom_acc[s->atom_len] = '\0';
}

void atom_push(skode_t *s, char c) {
  if (s->atom_len < s->atom_cap) {
    s->atom_acc[s->atom_len++] = c;
    s->atom_acc[s->atom_len] = '\0';
  }
}

#include <stdint.h>
#include <arpa/inet.h>

void atom_finish(skode_t *s) {
  int i = ATOM_NIL;
  char *p = (char *)&i;
  for (int n = 0; n < s->atom_len; n++) p[n] = s->atom_acc[n];
  s->atom_num = ntohl(i);
}

void atom_reset(skode_t *s) {
  s->atom_num = ATOM_NIL;
}

char *atom_string(int i) {
  static char s[5] = "\0\0\0\0\0";
  char *p = (char *)&i;
  for (int i=0; i<4; i++) s[i] = p[i];
  return s;
}

int decide(skode_t *s, int state, int (*fn)(skode_t *s)) {
  double d;
  float f;
  int i;
  unsigned int u;
  char *p;
  switch (state) {
    case CHUNK_END:
    case GET_ATOM:
      if (s->atom_num != ATOM_NIL) {
        fn(s);
        atom_reset(s);
        arg_clear(s);
      }
      atom_finish(s);
      atom_clear(s);
      break;
    case GET_NUMBER:
      arg_push(s, num_get(s));
      num_clear(s);
      return START;
      break;
    case GET_DEFER_STRING:
      printf("DEFER %c %g '%s'\n", s->defer_mode, s->defer_num, s->defer_acc);
      defer_clear(s);
      break;
  }
  return START;
}

#define VAR_MAX (26)
double local_var[VAR_MAX];
double global_var[VAR_MAX];

int parse(skode_t *s, char *line, int *entry_state, int (*fn)(skode_t *s)) {
  static int first = 1;
  if (first) {
    for (int i=0; i<VAR_MAX; i++) {
      local_var[i] = i+1000;
      global_var[i] = i+2000;
    }
    first = 0;
  }
  int state = *entry_state;
  char *ptr = line;
  char *end = ptr + strlen(ptr);
  char *tmp;
  char next;
  while (1) {
    if (ptr >= end) {
      switch (state) {
        case GET_ATOM:
          decide(s, state, fn);
          state = START;
          break;
        case GET_NUMBER:
          decide(s, state, fn);
          state = START;
          break;
        default:
          break;
      }
      break;
    }
    next = *(ptr+1);
    reprocess:
    switch (state) {
      case START:
        if (IS_NUMBER(*ptr)) {
          num_clear(s);
          num_push(s, *ptr);
          state = GET_NUMBER;
        }
        else if (IS_SEPARATOR(*ptr)) { /* skip whitespace and , */ }
        else if (IS_STRING(*ptr))    { string_clear(s); state = GET_STRING; }
        else if (IS_ARRAY(*ptr))     { num_clear(s); array_clear(s); state = GET_ARRAY; }
        else if (IS_VARIABLE(*ptr))  { state = GET_VARIABLE; }
        else if (IS_COMMENT(*ptr))   { state = GET_COMMENT; }
        else if (IS_CHUNK_END(*ptr)) { decide(s, CHUNK_END, fn); state = START; }
        else if (IS_DEFER(*ptr))     { decide(s, CHUNK_END, fn); s->defer_mode = *ptr; state = GET_DEFER_NUMBER; }
        else if (iscntrl(*ptr)) { puts("# iscntrl !!!!"); }
        else {
          // i hope this is at the right catch point...
          atom_clear(s);
          atom_push(s, *ptr);
          state = GET_ATOM;
        }
        break;
      case GET_NUMBER:
        if (IS_NUMBER(*ptr)) {
          num_push(s, *ptr);
        } else if (*ptr == '$') {
          printf("VAR?");
        } else {
          state = decide(s, state, fn);
          // we got a character we need to process
          goto reprocess;
        }
        break;
      case GET_STRING:
        if (IS_STRING_END(*ptr)) {
          decide(s, state, fn);
          state = START;
        } else {
          string_push(s, *ptr);
        }
        break;
      case GET_ARRAY:
        if (IS_ARRAY_END(*ptr)) {
          array_push(s);
          decide(s, state, fn);
          state = START;
        } else if (IS_NUMBER_EX(*ptr)) {
          num_push(s, *ptr);
        } else if (IS_SEPARATOR(*ptr)) {
          array_push(s);
        } else {
          //printf("# ignore %c\n", *ptr);
          // ignore stuff we don't know
        }
        break;
      case GET_COMMENT:
        if (IS_CHUNK_END(*ptr)) {
          decide(s, CHUNK_END, fn);
          state = START;
        } else if (*ptr == '\n') {
          decide(s, state, fn);
          state = START;
        }
        break;
      case GET_VARIABLE:
        if (isalpha(*ptr)) {
          int i;
          double d;
          if (islower(*ptr)) {
            // a-z
            i = (*ptr) - 'a';
            d = local_var[i];
          } else {
            // A-Z
            i = (*ptr) - 'A';
            d = global_var[i];
          }
          arg_push(s, d);
        } else {
          // not a var, so ignore and hope the next this is valid
          state = START;
          goto reprocess;
        }
        state = START;
        break;
      case GET_DEFER_NUMBER:
        if (IS_NUMBER(*ptr)) {
          num_push(s, *ptr);
        } else {
          s->defer_num = num_get(s);
          num_clear(s);
          state = GET_DEFER_STRING;
          goto reprocess;
        }
        break;
      case GET_DEFER_STRING:
        if (IS_DEFER(*ptr)) {
          s->defer_mode = *ptr;
          decide(s, GET_DEFER_STRING, fn);
          state = GET_DEFER_NUMBER;
        } else if (IS_CHUNK_END(*ptr)) {
          decide(s, GET_DEFER_STRING, fn);
          state = START;
        } else {
          defer_push(s, *ptr);
        }
        break;
      case GET_ATOM:
        if (IS_ATOM(*ptr)) {
          atom_push(s, *ptr);
        } else {
          decide(s, state, fn);
          state = START;
          goto reprocess;
        }
        break;
      default:
        puts("default ->START");
        decide(s, state, fn);
        state = START;
        break;
    } 
    ptr++;
  }
  *entry_state = state;
}

#ifdef DEMO
int demo(skode_t *s) {
  int *user = (int*)s->user;
  printf("DO %s WITH [", atom_string(s->atom_num));
  // run the callback here
  if (s->arg_len) {
    for (int n=0; n<s->arg_len; n++) printf(" %g", s->arg[n]);
  }
  printf(" ]\n");
  if (s->scr_len) printf("{%s}\n", s->scr_acc);
  if (s->data_len) {
    printf("(");
    for (int i=0; i<s->data_len; i++) printf(" %g", s->data[i]);
    printf(" )\n");
  }
  switch (s->atom_num) {
    case '=a__':
      local_var[0] = s->arg[0];
      break;
    case ':q__':
      printf("QUIT\n");
      *user = -1;
      break;
  }
}
int main(int argc, char *arg[]) {
  int user = 0;
  skode_t *s = skode_new(&user);
  linenoiseHistoryLoad(HISTORY_FILE);
  int state = START;
  while (1) {
    char *line = NULL;
    line = linenoise("# ");
    if (line == NULL) break;
    linenoiseHistoryAdd(line);
    int r = parse(s, line, &state, demo);
    linenoiseFree(line);
    if (user == -1) {
      printf("must quit\n");
      break;
    }
  }
  linenoiseHistorySave(HISTORY_FILE);
  skode_free(s);
  return 0;
}
#endif
