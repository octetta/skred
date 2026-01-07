#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include "skode.h"

/**
 * SKODE GRAMMAR (EBNF-ish)
 * 
 * chunk      = (element)* (';' | EOT)
 * element    = atom | number | string | array | variable | defer | comment | push | pop
 * atom       = [a-zA-Z!@%^&*_=:"'<>?/]+
 * number     = [-]?[0-9]+('.'[0-9]+)?([eE][-+]?[0-9]+)? | '.' | '-' | 'e'
 * string     = '{' [^}]* '}'
 * array      = '(' (number (',' | ' ')*)* ')'
 * variable   = '$' [0-9]
 * defer      = ('+' | '~') number chunk
 * comment    = '#' [^\n;]* ('\n' | ';')
 * push       = '['
 * pop        = ']'
 * separator  = ' ' | ',' | '\t' | '\n' | '\r'
 * 
 * EXECUTION MODEL:
 * - Numbers accumulate in arg[] array
 * - When atom completes, FUNCTION callback fires with accumulated arg[]
 * - arg[] is cleared after callback returns 0
 * - ';' or EOT triggers CHUNK_END event
 * - Atom names are packed into 32-bit integers for fast switching
 * 
 * SPECIAL FEATURES:
 * - Whitespace optional between atoms and numbers: "v0w1f440" works
 * - Commas and spaces are interchangeable separators
 * - Bare '.', '-', or 'e' produce NaN (for future "default value" feature)
 * - Arguments can precede atoms (side effect): "440 f" works like "f440"
 * - Variables $0-$9 for value storage and recall
 */

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
#define IS_PUSH(c) (c == '[')
#define IS_POP(c) (c == ']')
#define IS_ATOM(c) (isalpha(c) || strchr("!@%^&*_=:\"'<>?/", c))
#define IS_NUMBER_EX(c) (isxdigit(c) || strchr("-.eExX", c))

static double skode_strtod(char *s) {
  double d = NAN;
  if (s[1] == '\0' && (s[0] == '-' || s[0] == 'e' || s[0] == '.')) return d;
  d = strtod(s, NULL);
  return d;
}

#define ARG_MAX (8)
#define ATOM_MAX (4)
#define ATOM_NIL (0x5f5f5f5f)
#define VAR_MAX (10)

// ============================================================================
// UNIFIED BUFFER MANAGEMENT
// ============================================================================

typedef struct {
    char *data;
    int len;
    int cap;
} buffer_t;

static void buffer_init(buffer_t *b, int cap) {
    b->data = (char*)malloc(cap * sizeof(char));
    b->len = 0;
    b->cap = cap;
    if (b->data) b->data[0] = '\0';
}

static void buffer_free(buffer_t *b) {
    if (b->data) free(b->data);
    b->data = NULL;
    b->len = 0;
    b->cap = 0;
}

static void buffer_clear(buffer_t *b) {
    b->len = 0;
    if (b->data) b->data[0] = '\0';
}

static void buffer_push(buffer_t *b, char c) {
    if (b->len < b->cap - 1) {
        b->data[b->len++] = c;
        b->data[b->len] = '\0';
    }
}

static char* buffer_str(buffer_t *b) {
    return b->data;
}

// ============================================================================
// MAIN SKODE STRUCTURE
// ============================================================================

typedef struct skode_s {
    // Unified buffers
    buffer_t num;
    buffer_t string;
    buffer_t atom;
    buffer_t defer;
    
    // Data array
    double *data;
    int data_len;
    int data_cap;
    
    // Defer state
    double defer_num;
    char defer_mode;
    
    // Argument stack
    double arg[ARG_MAX];
    int arg_len;
    int arg_cap;
    
    // Atom state
    int atom_num;
    
    // Parser state
    int state;
    
    // Variables
    double local_var[VAR_MAX];
    double *global_var;
    double *global_save;
    
    // Callback
    int (*fn)(struct skode_s *s, int info);
    void *user;
    
    // Mode
    int mode;
    
    // Trace
    int trace;
} skode_t;

// ============================================================================
// ATOM ENCODING - Simpler implementation with documentation
// ============================================================================

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#endif

/**
 * Pack atom string into 32-bit integer for fast switching.
 * Example: "f" becomes 'f___' (0x665f5f5f)
 * Uses network byte order for consistency.
 */
static void atom_finish(skode_t *s) {
    int i = ATOM_NIL;  // Start with '____' (0x5f5f5f5f)
    char *p = (char *)&i;
    int len = s->atom.len < ATOM_MAX ? s->atom.len : ATOM_MAX;
    for (int n = 0; n < len; n++) {
        p[n] = s->atom.data[n];
    }
    s->atom_num = ntohl(i);
}

static void atom_reset(skode_t *s) {
    s->atom_num = ATOM_NIL;
}

char* atom_string(int i) {
    static char str[5] = "****";
    char *p = (char *)&i;
    for (int j = 0; j < 4; j++) {
        str[3-j] = p[j];
    }
    return str;
}

// ============================================================================
// ACTION FUNCTIONS - Extracted for clarity
// ============================================================================

static void action_finish_number(skode_t *s) {
    double val = skode_strtod(buffer_str(&s->num));
    if (s->trace) printf("# ARG_PUSH %g\n", val);
    if (s->arg_len < s->arg_cap) {
        s->arg[s->arg_len++] = val;
    }
    buffer_clear(&s->num);
}

static void action_finish_atom(skode_t *s) {
    if (s->trace) printf("# ATOM %s\n", buffer_str(&s->atom));
    
    if (s->atom_num != ATOM_NIL) {
        if (s->fn(s, FUNCTION) == 0) {
            s->arg_len = 0;  // Clear args
        }
        atom_reset(s);
    }
    
    atom_finish(s);
    buffer_clear(&s->atom);
}

static void action_finish_defer(skode_t *s) {
    if (s->trace) printf("# DEFER\n");
    s->fn(s, DEFER);
    buffer_clear(&s->defer);
}

static void action_finish_array(skode_t *s) {
    // Push final number if any
    if (s->num.len > 0) {
        s->data[s->data_len++] = skode_strtod(buffer_str(&s->num));
        buffer_clear(&s->num);
    }
}

static void action_chunk_end(skode_t *s) {
    // Handle leftover atom
    if (s->atom_num != ATOM_NIL) {
        if (s->trace) printf("# left-over ATOM\n");
        s->fn(s, FUNCTION);
        atom_reset(s);
    }
    
    // Handle leftover defer
    if (s->defer.len > 0) {
        if (s->trace) printf("# left-over DEFER\n");
        s->fn(s, DEFER);
        buffer_clear(&s->defer);
    }
    
    if (s->trace) printf("# CHUNK_END\n");
    s->fn(s, CHUNK_END);
    s->arg_len = 0;  // Clear args
}

// ============================================================================
// STATE MACHINE
// ============================================================================

int skode(skode_t *s, char *line, int (*fn)(skode_t *s, int info)) {
    char *ptr = line;
    char *end = ptr + strlen(ptr);
    
    while (1) {
        if (ptr >= end) {
            switch (s->state) {
                case GET_ATOM:
                    action_finish_atom(s);
                    s->state = START;
                    break;
                case GET_NUMBER:
                    action_finish_number(s);
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
                    buffer_clear(&s->num);
                    buffer_push(&s->num, *ptr);
                    s->state = GET_NUMBER;
                }
                else if (IS_SEPARATOR(*ptr)) { /* skip */ }
                else if (IS_PUSH(*ptr))      { s->fn(s, PUSH); }
                else if (IS_POP(*ptr))       { s->fn(s, POP); }
                else if (IS_STRING(*ptr))    { buffer_clear(&s->string); s->state = GET_STRING; }
                else if (IS_ARRAY(*ptr))     { buffer_clear(&s->num); s->data_len = 0; s->state = GET_ARRAY; }
                else if (IS_VARIABLE(*ptr))  { s->state = GET_VARIABLE; }
                else if (IS_COMMENT(*ptr))   { s->state = GET_COMMENT; }
                else if (IS_CHUNK_END(*ptr)) { action_chunk_end(s); s->state = START; }
                else if (IS_DEFER(*ptr))     { action_chunk_end(s); s->defer_mode = *ptr; s->state = GET_DEFER_NUMBER; }
                else if (iscntrl(*ptr))      { /* skip control chars */ }
                else {
                    buffer_clear(&s->atom);
                    buffer_push(&s->atom, *ptr);
                    s->state = GET_ATOM;
                }
                break;
                
            case GET_NUMBER:
                if (IS_NUMBER(*ptr)) {
                    buffer_push(&s->num, *ptr);
                } else if (*ptr == '$') {
                    printf("# VAR after number?\n");
                } else {
                    action_finish_number(s);
                    s->state = START;
                    goto reprocess;
                }
                break;
                
            case GET_STRING:
                if (IS_STRING_END(*ptr)) {
                    s->fn(s, GOT_STRING);
                    s->state = START;
                } else {
                    buffer_push(&s->string, *ptr);
                }
                break;
                
            case GET_ARRAY:
                if (IS_ARRAY_END(*ptr)) {
                    action_finish_array(s);
                    s->fn(s, GOT_ARRAY);
                    s->state = START;
                } else if (IS_NUMBER_EX(*ptr)) {
                    buffer_push(&s->num, *ptr);
                } else if (IS_SEPARATOR(*ptr)) {
                    if (s->num.len > 0) {
                        s->data[s->data_len++] = skode_strtod(buffer_str(&s->num));
                        buffer_clear(&s->num);
                    }
                }
                break;
                
            case GET_COMMENT:
                if (IS_CHUNK_END(*ptr)) {
                    action_chunk_end(s);
                    s->state = START;
                } else if (*ptr == '\n') {
                    s->state = START;
                }
                break;
                
            case GET_VARIABLE:
                if (isdigit(*ptr)) {
                    char c = *ptr;
                    double d = s->global_var[c-'0'];
                    if (s->arg_len < s->arg_cap) {
                        s->arg[s->arg_len++] = d;
                    }
                    if (s->trace) printf("# GET_VARIABLE %c (%g)\n", c, d);
                    s->state = START;
                } else {
                    if (s->trace) puts("# not a var");
                    s->state = START;
                    goto reprocess;
                }
                break;
                
            case GET_DEFER_NUMBER:
                if (IS_NUMBER(*ptr)) {
                    buffer_push(&s->num, *ptr);
                } else {
                    s->defer_num = skode_strtod(buffer_str(&s->num));
                    buffer_clear(&s->num);
                    s->state = GET_DEFER_STRING;
                    goto reprocess;
                }
                break;
                
            case GET_DEFER_STRING:
                if (IS_DEFER(*ptr)) {
                    s->defer_mode = *ptr;
                    action_finish_defer(s);
                    s->state = GET_DEFER_NUMBER;
                } else if (IS_CHUNK_END(*ptr)) {
                    action_finish_defer(s);
                    s->state = START;
                } else {
                    buffer_push(&s->defer, *ptr);
                }
                break;
                
            case GET_ATOM:
                if (IS_ATOM(*ptr)) {
                    buffer_push(&s->atom, *ptr);
                } else {
                    action_finish_atom(s);
                    s->state = START;
                    goto reprocess;
                }
                break;
                
            default:
                if (s->trace) puts("# default -> START");
                s->state = START;
                break;
        }
        ptr++;
    }
    
    if (s->mode == 0) {
        action_chunk_end(s);
        s->state = START;
    }
    return 0;
}

// ============================================================================
// PUBLIC API
// ============================================================================

skode_t *skode_new(int (*fn)(skode_t *s, int info), void *user) {
    skode_t *s = (skode_t*)malloc(sizeof(skode_t));
    
    s->global_var = s->local_var;
    s->global_save = s->local_var;
    for (int i = 0; i < VAR_MAX; i++) {
        s->local_var[i] = 0;
    }
    
    buffer_init(&s->num, 1024);
    buffer_init(&s->string, 1024);
    buffer_init(&s->atom, ATOM_MAX + 1);
    buffer_init(&s->defer, 1024);
    
    s->data_cap = 1024;
    s->data_len = 0;
    s->data = (double*)malloc(s->data_cap * sizeof(double));
    
    s->defer_num = 0;
    s->defer_mode = '?';
    
    s->arg_cap = ARG_MAX;
    s->arg_len = 0;
    
    s->atom_num = ATOM_NIL;
    
    s->fn = fn;
    s->user = user;
    
    s->state = START;
    s->mode = 0;
    s->trace = 0;
    
    return s;
}

void skode_free(skode_t *s) {
    buffer_free(&s->num);
    buffer_free(&s->string);
    buffer_free(&s->atom);
    buffer_free(&s->defer);
    
    if (s->data) free(s->data);
    s->data = NULL;
    s->data_cap = 0;
    s->data_len = 0;
}

// Accessor functions
int skode_atom_num(skode_t *s) { return s->atom_num; }
int skode_arg_len(skode_t *s) { return s->arg_len; }
double *skode_arg(skode_t *s) { return s->arg; }
void *skode_user(skode_t *s) { return s->user; }
char *skode_string(skode_t *s) { return buffer_str(&s->string); }
int skode_string_len(skode_t *s) { return s->string.len; }
void skode_chunk_mode(skode_t *s, int mode) { s->mode = mode; }
void skode_trace_set(skode_t *s, int n) { s->trace = n; }
double skode_defer_num(skode_t *s) { return s->defer_num; }
char *skode_defer_string(skode_t *s) { return buffer_str(&s->defer); }
char skode_defer_mode(skode_t *s) { return s->defer_mode; }
char *skode_atom_string(skode_t *s) { return atom_string(s->atom_num); }
double *skode_data(skode_t *s) { return s->data; }
int skode_data_len(skode_t *s) { return s->data_len; }

void skode_arg_clear(skode_t *s) { s->arg_len = 0; }

double skode_arg_push(skode_t *s, double n) {
    if (s->arg_len < s->arg_cap) {
        s->arg[s->arg_len++] = n;
    }
    return n;
}

void skode_arg_len_set(skode_t *s, int n) { s->arg_len = n; }

double skode_arg_drop(skode_t *s) {
    int n = s->arg_len;
    double x = 0;
    if (n > 0) {
        x = s->arg[0];
        for (int i = 1; i < ARG_MAX; i++) {
            s->arg[i-1] = s->arg[i];
        }
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
    for (int i = 0; i < n && s->arg_len < s->arg_cap; i++) {
        s->arg[s->arg_len++] = a[i];
    }
    return 0;
}

void skode_set_local(skode_t *s, int n, double x) { 
    if (n >= 0 && n < VAR_MAX) {
        s->global_var[n] = x;
    }
}

void skode_set_global(skode_t *s, double *p) { s->global_var = p; s->global_save = p; }
void skode_use_local(skode_t *s) { s->global_var = s->local_var; }
void skode_use_global(skode_t *s) { s->global_var = s->global_save; }

void skode_local_to_global(skode_t *s, int n) {
    if (n >= 0 && n < VAR_MAX) {
        s->global_var[n] = s->local_var[n];
    }
}

void skode_global_to_local(skode_t *s, int n) {
    if (n >= 0 && n < VAR_MAX) {
        s->local_var[n] = s->global_var[n];
    }
}
