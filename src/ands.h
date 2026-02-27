#ifndef _ANDS_H_
#define _ANDS_H_

#include <stdint.h>

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

typedef struct ands_s ands_t;
typedef uint32_t atom_t;

ands_t *ands_new(int (*fn)(ands_t *s, int info), void *user);
void ands_free(ands_t *s);
int ands_consume(ands_t *s, char *line, int (*fn)(ands_t *s, int info));
uint32_t ands_atom_num(ands_t *s);
char *ands_atom_string(ands_t *s);
int ands_arg_len(ands_t *s);
double *ands_arg(ands_t *s);
void *ands_user(ands_t *s);
char *ands_string(ands_t *s);
int ands_string_len(ands_t *s);
void ands_chunk_mode(ands_t *s, int mode);
int ands_chunk_mode_get(ands_t *s);
double ands_defer_num(ands_t *s);
char *ands_defer_string(ands_t *s);
char ands_defer_mode(ands_t *s);
char *ands_atom_string(ands_t *s);
void ands_trace_set(ands_t *s, int n);
char *atom_string(int i);
double *ands_data(ands_t *s);
int ands_data_len(ands_t *s);
double ands_arg_push(ands_t *s, double n);
double ands_arg_push_many(ands_t *s, double *a, int n);
void ands_arg_clear(ands_t *s);
double ands_arg_drop(ands_t *s);
double ands_arg_swap(ands_t *s);
void ands_arg_len_set(ands_t *s, int n);
void ands_set_local(ands_t *s, int n, double x);
void ands_set_global(ands_t *s, double *p);
void ands_use_local(ands_t *s);
void ands_use_global(ands_t *s);
void ands_local_to_global(ands_t *s, int n);
void ands_global_to_local(ands_t *s, int n);
void ands_data_resize(ands_t *s, int len);
int ands_data_cap(ands_t *s);
double ands_get_local(ands_t *s, int n);

char *ands_string_to_external(ands_t *s, char *dst, int len);
char *ands_string_from_external(ands_t *s, char *src, int len);

#define ATOM4(c) (c)

#endif

