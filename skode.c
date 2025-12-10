#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "skode.h"

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
// hide [ and ] because they can't work as ATOM easily to push/pop voice/etc.
//#define IS_ATOM(c) (isalpha(c) || strchr("!@%^&*_=:\"'<>[]?/", c))
#define IS_ATOM(c) (isalpha(c) || strchr("!@%^&*_=:\"'<>?/", c))
// used by array... allows hex constants too via 0x... 0X...
#define IS_NUMBER_EX(c) (isxdigit(c) || strchr("-.eExX", c))

static double skode_strtod(char *s) {
  double d = NAN;
  if (s[1] == '\0' && (s[0] == '-' || s[0] == 'e' || s[0] == '.')) return d;
  d = strtod(s, NULL);
  return d;
}

#define ARG_MAX (8)
#define NUM_ACC_MAX (1024)
#define ATOM_MAX (4)
#define ATOM_NIL (0x5f5f5f5f)
#define VAR_MAX (10)

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
  //
  double local_var[VAR_MAX];
  double *global_var;
  double *global_save;
  //
  int (*fn)(struct skode_s *s, int info);
  void *user;
  //
  int mode;
  //
  int trace;
} skode_t;

skode_t *skode_new(int (*fn)(skode_t *s, int info), void *user) {
  skode_t *s = (skode_t*)malloc(sizeof(skode_t));
  s->global_var = s->local_var;
  s->global_save = s->local_var;
  for (int i=0; i<VAR_MAX; i++) {
    s->local_var[i] = 0;
  }
  s->scr_cap = 1024;
  s->scr_len = 0;
  s->scr_acc = (char*)malloc(s->scr_cap * sizeof(char));
  //
  s->num_cap = 1024;
  s->num_len = 0;
  //
  s->data_cap = 1024;
  s->data_len = 0;
  s->data = (double*)malloc(s->data_cap * sizeof(double));
  //
  s->defer_cap = 1024;
  s->defer_len = 0;
  s->defer_acc = (char*)malloc(s->defer_cap * sizeof(char));
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
  s->fn = fn;
  s->user = user;
  //
  s->state = START;
  //
  s->mode = 0;
  //
  s->trace = 0;
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

void string_clear(skode_t *s) {
  s->scr_len = 0;
  s->scr_acc[0] = '\0';
}

void string_push(skode_t *s, char c) {
  if (s->scr_len < s->scr_cap) {
    s->scr_acc[s->scr_len++] = c;
    s->scr_acc[s->scr_len] = '\0';
  }
}

void num_clear(skode_t *s) {
  s->num_len = 0;
  s->num_acc[0] = '\0';
}

void num_push(skode_t *s, char c) {
  if (s->num_len < s->num_cap) {
    s->num_acc[s->num_len++] = c;
    s->num_acc[s->num_len] = '\0';
  }
}

double num_get(skode_t *s) { return skode_strtod(s->num_acc); }

void array_clear(skode_t *s) { s->data_len = 0; }

void array_push(skode_t *s) {
  if (s->num_len) s->data[s->data_len++] = num_get(s);
  num_clear(s);
}

void arg_clear(skode_t *s) { s->arg_len = 0; }

void arg_push(skode_t *s, double d) {
  if (s->trace) printf("arg_push %g\n", d);
  if (s->arg_len < s->arg_cap) s->arg[s->arg_len++] = d;
}

void defer_clear(skode_t *s) {
  s->defer_len = 0;
  s->defer_acc[0] = '\0';
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
  static char s[5] = "****";
  char *p = (char *)&i;
  for (int i=0; i<4; i++) s[3-i] = p[i];
  return s;
}

static int action(skode_t *s, int state) {
  int pushes = 0;
  if (state == CHUNK_END) {
    if (s->atom_num != ATOM_NIL) {
      if (s->trace) printf("# left-over ATOM\n");
      pushes = s->fn(s, FUNCTION);
      atom_reset(s);
    }
    if (s->defer_len) {
        if (s->trace) printf("# left-over DEFER\n");
        s->fn(s, DEFER);
        defer_clear(s);
    }
    if (s->trace) printf("# CHUNK_END\n");
    s->fn(s, CHUNK_END);
    if (pushes == 0) arg_clear(s);
    return 0;
  }
  ////
  switch (state) {
    case GET_ATOM:
      if (s->trace) printf("# ATOM\n");
      if (s->atom_num != ATOM_NIL) {
        if (s->fn(s, FUNCTION) == 0) arg_clear(s);
        atom_reset(s);
      }
      atom_finish(s);
      atom_clear(s);
      break;
    case GET_NUMBER:
      if (s->trace) printf("# ARG_PUSH\n");
      arg_push(s, num_get(s));
      num_clear(s);
      break;
    case GET_DEFER_STRING:
      if (s->trace) printf("# DEFER\n");
      s->fn(s, DEFER);
      // how much of the following should the called function do?
      defer_clear(s);
      break;
  }
  return START;
}

int skode(skode_t *s, char *line, int (*fn)(skode_t *s, int info)) {
  char *ptr = line;
  char *end = ptr + strlen(ptr);
  while (1) {
    if (ptr >= end) {
      switch (s->state) {
        case GET_ATOM:
          action(s, s->state);
          s->state = START;
          break;
        case GET_NUMBER:
          action(s, s->state);
          s->state = START;
          break;
        default:
          break;
      }
      break;
    }
    reprocess:
    switch (s->state) {
      case START:
        if (IS_NUMBER(*ptr)) {
          num_clear(s);
          num_push(s, *ptr);
          s->state = GET_NUMBER;
        }
        else if (IS_SEPARATOR(*ptr)) { /* skip whitespace and , */ }
        else if (IS_STRING(*ptr))    { string_clear(s); s->state = GET_STRING; }
        else if (IS_ARRAY(*ptr))     { num_clear(s); array_clear(s); s->state = GET_ARRAY; }
        else if (IS_VARIABLE(*ptr))  { s->state = GET_VARIABLE; }
        else if (IS_COMMENT(*ptr))   { s->state = GET_COMMENT; }
        else if (IS_CHUNK_END(*ptr)) { action(s, CHUNK_END); s->state = START; }
        else if (IS_DEFER(*ptr))     { action(s, CHUNK_END); s->defer_mode = *ptr; s->state = GET_DEFER_NUMBER; }
        else if (iscntrl(*ptr)) { puts("# iscntrl !!!!"); }
        else {
          // i hope this is at the right catch point...
          atom_clear(s);
          atom_push(s, *ptr);
          s->state = GET_ATOM;
        }
        break;
      case GET_NUMBER:
        if (IS_NUMBER(*ptr)) {
          num_push(s, *ptr);
        } else if (*ptr == '$') {
          printf("VAR?");
        } else {
          s->state = action(s, s->state);
          // we got a character we need to process
          goto reprocess;
        }
        break;
      case GET_STRING:
        if (IS_STRING_END(*ptr)) {
          action(s, s->state);
          s->state = START;
        } else {
          string_push(s, *ptr);
        }
        break;
      case GET_ARRAY:
        if (IS_ARRAY_END(*ptr)) {
          array_push(s);
          action(s, s->state);
          s->state = START;
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
          action(s, CHUNK_END);
          s->state = START;
        } else if (*ptr == '\n') {
          action(s, s->state);
          s->state = START;
        }
        break;
      case GET_VARIABLE:
        if (isdigit(*ptr)) {
          char c = *ptr;
          double d = s->global_var[c-48];
          arg_push(s, d);
          if (s->trace) printf("GET_VARIABLE %c (%g)\n", c, d);
          s->state = START;
        } else {
          // not a var, so ignore and hope the next this is valid
          if (s->trace) puts("not a var");
          s->state = START;
          goto reprocess;
        }
        s->state = START;
        break;
      case GET_DEFER_NUMBER:
        if (IS_NUMBER(*ptr)) {
          num_push(s, *ptr);
        } else {
          s->defer_num = num_get(s);
          num_clear(s);
          s->state = GET_DEFER_STRING;
          goto reprocess;
        }
        break;
      case GET_DEFER_STRING:
        if (IS_DEFER(*ptr)) {
          s->defer_mode = *ptr;
          action(s, GET_DEFER_STRING);
          s->state = GET_DEFER_NUMBER;
        } else if (IS_CHUNK_END(*ptr)) {
          action(s, GET_DEFER_STRING);
          s->state = START;
        } else {
          defer_push(s, *ptr);
        }
        break;
      case GET_ATOM:
        if (IS_ATOM(*ptr)) {
          atom_push(s, *ptr);
        } else {
          action(s, s->state);
          s->state = START;
          goto reprocess;
        }
        break;
      default:
        puts("default ->START");
        action(s, s->state);
        s->state = START;
        break;
    } 
    ptr++;
  }
  if (s->mode == 0) { action(s, CHUNK_END); s->state = START; }
  return 0;
}

int skode_atom_num(skode_t *s) { return s->atom_num; }
int skode_arg_len(skode_t *s) { return s->arg_len; }
double *skode_arg(skode_t *s) { return s->arg; }
void *skode_user(skode_t *s) { return s->user; }
char *skode_string(skode_t *s) { return s->scr_acc; }
int skode_string_len(skode_t *s) { return s->scr_len; }
void skode_chunk_mode(skode_t *s, int mode) { s->mode = mode; }
void skode_trace_set(skode_t *s, int n) { s->trace = n; }
double skode_defer_num(skode_t *s) { return s->defer_num; }
char *skode_defer_string(skode_t *s) { return s->defer_acc; }
char skode_defer_mode(skode_t *s) { return s->defer_mode; }
char *skode_atom_string(skode_t *s) { return atom_string(s->atom_num); }
double *skode_data(skode_t *s) { return s->data; }
int skode_data_len(skode_t *s) { return s->data_len; }

void skode_arg_clear(skode_t *s) { arg_clear(s); }

double skode_arg_push(skode_t *s, double n) {
  arg_push(s, n);
  return n;
}

void skode_arg_len_set(skode_t *s, int n) { s->arg_len = n; }

double skode_arg_drop(skode_t *s) {
  int n = s->arg_len;
  double x = 0;
  if (n>0) {
    x = s->arg[0];
    for (int i=1; i<ARG_MAX; i++) s->arg[i-1] = s->arg[i];
    s->arg_len--;
  }
  return x;
}

double skode_arg_swap(skode_t *s) {
  if (s->arg_len > 1) {
    double t = s->arg[0];
    s->arg[0] = s->arg[1];
    s->arg[1] = t;
  }
  return 0;
}

double skode_arg_push_many(skode_t *s, double *a, int n) {
  // there's more gymnastics needed for this to be possible
  // need an array the caller can use???
  for (int i=0; i<n; i++) arg_push(s, a[i]);
  return 0;
}

void skode_set_local(skode_t *s, int n, double x) { s->global_var[n] = x; }

void skode_set_global(skode_t *s, double *p) { s->global_var = p; s->global_save = p; }
void skode_use_local(skode_t *s) { s->global_var = s->local_var; }
void skode_use_global(skode_t *s) { s->global_var = s->global_save; }
