#ifndef _SKODE_H_
#define _SKODE_H_

#define MAX_CMD 3
#define MAX_ARGS 10
#define MAX_VARS 62 // 10 digits + 26 lowercase + 26 uppercase
#define MAX_PAREN 128
#define MAX_BRACE 65536
#define MAX_QUEUE 65536
#define MAX_NUMBUF 32

typedef struct skode_s {
  char cmd[MAX_CMD];
  char nbuf[MAX_NUMBUF];
  char vname;
  float arg[MAX_ARGS];
  float vars[MAX_VARS];
  // float paren[MAX_PAREN];
  float *paren;
  int parenc;
  int narg;
  int nvar;
  int nparen;
  int vdef[MAX_VARS];
  int nlen;
  char brace[MAX_BRACE];
  int blen;
  float queue_time;
  char queue[MAX_QUEUE];
  int qlen;
  enum { START, CMD, ARG, VAR, BRACE, PAREN, QUEUE, QUEUE_TIME, QUEUE_STR, COMMENT } state;
  //
  void (*fn)(struct skode_s *p);
  int voice;
  int pattern;
  int major;
  int minor;
  int last_major;
  int last_minor;
  int output;
  int debug;
  int trace;
  //
} skode_t;

int is_cmd(skode_t *p);
int is_brace(skode_t *p);
int is_paren(skode_t *p);
int is_queue(skode_t *p);
void sparse_init(skode_t *p, void (*fn)(skode_t *p));
void sparse(skode_t *p, const char *s, int len);
void sparse_complete(skode_t *p);
void sparse_free(skode_t *p);
void sparse_paren_resize(skode_t *p, int len);
#endif
