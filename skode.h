#ifndef _SKODE_H_
#define _SKODE_H_

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
  //
  GOT_NUMBER,
  GOT_ATOM,
  //
  FUNCTION,
  DEFER,
  GOT_STRING,
  GOT_ARRAY,
  PUSH,
  POP,
};

typedef struct skode_s skode_t;

skode_t *skode_new(int (*fn)(skode_t *s, int info), void *user);
void skode_free(skode_t *s);
int skode(skode_t *s, char *line, int (*fn)(skode_t *s, int info));
int skode_atom_num(skode_t *s);
char *skode_atom_string(skode_t *s);
int skode_arg_len(skode_t *s);
double *skode_arg(skode_t *s);
void *skode_user(skode_t *s);
char *skode_string(skode_t *s);
int skode_string_len(skode_t *s);
void skode_chunk_mode(skode_t *s, int mode);
double skode_defer_num(skode_t *s);
char *skode_defer_string(skode_t *s);
char skode_defer_mode(skode_t *s);
char *skode_atom_string(skode_t *s);
void skode_trace_set(skode_t *s, int n);
char *atom_string(int i);
double *skode_data(skode_t *s);
int skode_data_len(skode_t *s);
double skode_arg_push(skode_t *s, double n);
double skode_arg_push_many(skode_t *s, double *a, int n);
void skode_arg_clear(skode_t *s);
double skode_arg_drop(skode_t *s);
double skode_arg_swap(skode_t *s);
void skode_arg_len_set(skode_t *s, int n);
void skode_set_local(skode_t *s, int n, double x);
void skode_set_global(skode_t *s, double *p);
void skode_use_local(skode_t *s);
void skode_use_global(skode_t *s);
void skode_local_to_global(skode_t *s, int n);
void skode_global_to_local(skode_t *s, int n);

#endif

