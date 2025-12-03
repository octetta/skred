#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "linenoise.h"

#define HISTORY_FILE ".skode_history"

#define IS_NUMBER(c) (isdigit(c) || strchr("-.", c))
#define IS_NUMBER_EX(c) (isdigit(c) || strchr("-.eE", c))
#define IS_ATOM(c) (isalpha(c) || strchr("!@%^&*_=:\"'<>[]?/", c))
#define IS_SEPARATOR(c) (isspace(c) || c == ',')
#define IS_STRING(c) (c == '{')
#define IS_STRING_END(c) (c == '}')
#define IS_ARRAY(c) (c == '(')
#define IS_ARRAY_END(c) (c == ')')
#define IS_VARIABLE(c) (c == '$')
#define IS_COMMENT(c) (c == '#')
#define IS_CHUNK_END(c) (c == ';' || c == 0x04) // 0x04 ASCII EOT / end of xmit
#define IS_DEFER(c) (c == '+' || c == '~')

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

typedef struct sk8_s {
  char *string;
  int string_len;
  int string_max;
} sk8_t;

char scr_acc[1024] = {};
int scr_len = 0;
int scr_cap = 1024;

void string_clear(void) {
  scr_len = 0;
  scr_acc[scr_len] = '\0';
}

void string_push(char c) {
  if (scr_len < scr_cap) {
    scr_acc[scr_len++] = c;
    scr_acc[scr_len] = '\0';
  }
}

char num_acc[1024] = {};
int num_len = 0;
int num_cap = 1024;

void num_clear(void) {
  num_len = 0;
  num_acc[num_len] = '\0';
}

void num_push(char c) {
  if (num_len < num_cap) {
    num_acc[num_len++] = c;
    num_acc[num_len] = '\0';
  }
}

double num_get(void) {
  return skode_strtod(num_acc);
}

double data[1024];
int data_len = 0;
int data_cap = 1024;

void array_clear(void) {
  data_len = 0;
}

void array_push(void) {
  if (num_len) data[data_len++] = num_get();
  num_clear();
}

#define ARG_MAX (8)
double arg[ARG_MAX];
int arg_len = 0;
int arg_cap = ARG_MAX;

void arg_clear(void)  { arg_len = 0; }
void arg_push(double d) { if (arg_len < arg_cap) arg[arg_len++] = d; }

char defer_acc[1024];
int defer_len = 0;
int defer_cap = 1024;
double defer_num = 0;
char defer_mode = '?';

void defer_clear(void) {
  defer_len = 0;
  defer_acc[defer_len] = '\0';
}

void defer_push(char c) {
  if (defer_len < defer_cap) {
    defer_acc[defer_len++] = c;
    defer_acc[defer_len] = '\0';
  }
}

#define ATOM_NIL (0x5f5f5f5f)

char atom_acc[1024];
int atom_cap = 4;
int atom_len = 0;
int atom_num = ATOM_NIL;

void atom_clear(void) {
  atom_len = 0;
  atom_acc[atom_len] = '\0';
}

void atom_push(char c) {
  if (atom_len < atom_cap) {
    atom_acc[atom_len++] = c;
    atom_acc[atom_len] = '\0';
  }
}

void atom_finish(void) {
  int i = ATOM_NIL;
  char *p = (char *)&i;
  for (int n = 0; n < atom_len; n++) p[n] = atom_acc[n];
  atom_num = i;
}

void atom_reset(void) {
  atom_num = ATOM_NIL;
}

char *atom_string(int i) {
  static char s[5] = "\0\0\0\0\0";
  char *p = (char *)&i;
  for (int i=0; i<4; i++) s[i] = p[i];
  return s;
}

int decide(int state) {
  double d;
  float f;
  int i;
  unsigned int u;
  char *p;
  switch (state) {
    case CHUNK_END:
    case GET_ATOM:
      if (atom_num != ATOM_NIL) {
        printf("DO %s WITH [", atom_string(atom_num));
        // run the callback here
        if (arg_len) {
          for (int n=0; n<arg_len; n++) printf(" %g", arg[n]);
        }
        printf(" ]\n");
        
          printf("{%s}\n", scr_acc);

          printf("(");
          for (int i=0; i<data_len; i++) printf(" %g", data[i]);
          printf(" )\n");

        atom_reset();
        arg_clear();
      }
      atom_finish();
      atom_clear();
      break;
    case GET_NUMBER:
      arg_push(num_get());
      num_clear();
      return START;
      break;
    case GET_DEFER_STRING:
      printf("DEFER %c %g '%s'\n", defer_mode, defer_num, defer_acc);
      defer_clear();
      break;
  }
  return START;
}

#define VAR_MAX (26)
double local_var[VAR_MAX];
double global_var[VAR_MAX];

int parse(char *line, int *entry_state) {
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
          decide(state);
          state = START;
          break;
        case GET_NUMBER:
          decide(state);
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
          num_clear();
          num_push(*ptr);
          state = GET_NUMBER;
        }
        else if (IS_SEPARATOR(*ptr)) { /* skip whitespace and , */ }
        else if (IS_STRING(*ptr))    { string_clear(); state = GET_STRING; }
        else if (IS_ARRAY(*ptr))     { num_clear(); array_clear(); state = GET_ARRAY; }
        else if (IS_VARIABLE(*ptr))  { state = GET_VARIABLE; }
        else if (IS_COMMENT(*ptr))   { state = GET_COMMENT; }
        else if (IS_CHUNK_END(*ptr)) { decide(CHUNK_END); state = START; }
        else if (IS_DEFER(*ptr))     { decide(CHUNK_END); defer_mode = *ptr; state = GET_DEFER_NUMBER; }
        else if (iscntrl(*ptr)) { puts("# iscntrl !!!!"); }
        else {
          //printf("-> GET_ATOM '%c'\n", *ptr);
          // i hope this is at the right catch point...
          atom_clear();
          atom_push(*ptr);
          state = GET_ATOM;
        }
        break;
      case GET_NUMBER:
        if (IS_NUMBER(*ptr)) {
          num_push(*ptr);
        } else if (*ptr == '$') {
          printf("VAR?");
        } else {
          state = decide(state);
          // we got a character we need to process
          goto reprocess;
        }
        break;
      case GET_STRING:
        if (IS_STRING_END(*ptr)) {
          decide(state);
          state = START;
        } else {
          string_push(*ptr);
        }
        break;
      case GET_ARRAY:
        if (IS_ARRAY_END(*ptr)) {
          array_push();
          decide(state);
          state = START;
        } else if (IS_NUMBER_EX(*ptr)) {
          num_push(*ptr);
        } else if (IS_SEPARATOR(*ptr)) {
          array_push();
        } else {
          //printf("# ignore %c\n", *ptr);
          // ignore stuff we don't know
        }
        break;
      case GET_COMMENT:
        if (IS_CHUNK_END(*ptr)) {
          decide(CHUNK_END);
          state = START;
        } else if (*ptr == '\n') {
          decide(state);
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
          arg_push(d);
        } else {
          // not a var, so ignore and hope the next this is valid
          state = START;
          goto reprocess;
        }
        state = START;
        break;
      case GET_DEFER_NUMBER:
        if (IS_NUMBER(*ptr)) {
          num_push(*ptr);
        } else {
          defer_num = num_get();
          num_clear();
          state = GET_DEFER_STRING;
          goto reprocess;
        }
        break;
      case GET_DEFER_STRING:
        if (IS_DEFER(*ptr)) {
          defer_mode = *ptr;
          decide(GET_DEFER_STRING);
          state = GET_DEFER_NUMBER;
        } else if (IS_CHUNK_END(*ptr)) {
          decide(GET_DEFER_STRING);
          state = START;
        } else {
          defer_push(*ptr);
        }
        break;
      case GET_ATOM:
        if (IS_ATOM(*ptr)) {
          atom_push(*ptr);
        } else {
          decide(state);
          state = START;
          goto reprocess;
        }
        break;
      default:
        puts("default ->START");
        decide(state);
        state = START;
        break;
    } 
    ptr++;
  }
  *entry_state = state;
}

int main(int argc, char *arg[]) {
  linenoiseHistoryLoad(HISTORY_FILE);
  int state = START;
  while (1) {
    char *line = NULL;
    line = linenoise("# ");
    if (line == NULL) break;
    linenoiseHistoryAdd(line);
    int r = parse(line, &state);
    linenoiseFree(line);
  }
  linenoiseHistorySave(HISTORY_FILE);
  return 0;
}
