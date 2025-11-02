#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define MAX_ARGS 8
#define MAX_VARS 62
#define MAX_PAREN 128
#define MAX_BRACE 65536
#define MAX_NUMBUF 32

typedef struct {
  char cmd[3], nbuf[MAX_NUMBUF], vname;
  float arg[MAX_ARGS], vars[MAX_VARS], paren[MAX_PAREN];
  int narg, nvar, nparen, vdef[MAX_VARS], nlen;
  char brace[MAX_BRACE];
  int blen;
  enum { START, CMD, ARG, VAR, BRACE, PAREN, COMMENT } state;
} P;

static int vidx(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'z') return 10 + c - 'a';
  if (c >= 'A' && c <= 'Z') return 36 + c - 'A';
  return -1;
}

static void emit(P *p) {
  if (p->cmd[0] && p->cmd[0] != '=') {
    printf("%s", p->cmd);
    for (int i = 0; i < p->narg; i++) printf(" %.2f", p->arg[i]);
    printf("\n");
  }
  if (p->blen > 0) printf("{%.*s}\n", p->blen, p->brace);
  if (p->nparen > 0) {
    printf("(");
    for (int i = 0; i < p->nparen; i++) printf("%.2f ", p->paren[i]);
    printf(")\n");
  }
}

static void pushnum(P *p, int toparen) {
  if (p->nlen > 0) {
    p->nbuf[p->nlen] = 0;
    if (toparen && p->nparen < MAX_PAREN) p->paren[p->nparen++] = atof(p->nbuf);
    else if (!toparen && p->narg < MAX_ARGS) p->arg[p->narg++] = atof(p->nbuf);
    p->nlen = 0;
  }
}

static void finish(P *p) {
  pushnum(p, 0);
  if (p->cmd[0] == '=' && p->narg == 1) {
    int i = vidx(p->vname);
    if (i >= 0) {
      p->vars[i] = p->arg[0];
      p->vdef[i] = 1;
      printf("$%c=%.2f\n", p->vname, p->vars[i]);
    }
  }
  emit(p);
  memset(p->cmd, 0, 3);
  p->narg = p->blen = p->nparen = 0;
  p->state = START;
}

static void startcmd(P *p, char c) {
  finish(p);
  p->cmd[0] = c;
  p->cmd[1] = 0;
  p->state = (c == '/') ? CMD : ARG;
}

void parse(P *p, const char *s, int len) {
  for (int i = 0; i < len; i++) {
    char c = s[i];
    
    switch (p->state) {
      case COMMENT:
        if (c == '\n') p->state = START;
        break;
        
      case BRACE:
        if (c == '}') finish(p);
        else if (p->blen < MAX_BRACE - 1) p->brace[p->blen++] = c;
        break;
        
      case PAREN:
        if (isdigit(c) || strchr(".-eE", c)) {
          if (p->nlen < MAX_NUMBUF - 1) p->nbuf[p->nlen++] = c;
        } else {
          pushnum(p, 1);
          if (c == ')') finish(p);
        }
        break;
        
      case CMD:
        if (isalpha(c)) {
          p->cmd[1] = c;
          p->cmd[2] = 0;
          p->state = ARG;
        } else if (c == ';' || c == '\n') {
          finish(p);
        } else if (!isspace(c) && c != ',') {
          p->state = START;
        }
        break;
        
      case VAR:
        if (isalnum(c)) {
          if (p->cmd[0] == '=') {
            p->vname = c;
          } else {
            int idx = vidx(c);
            if (idx >= 0 && p->vdef[idx] && p->narg < MAX_ARGS)
              p->arg[p->narg++] = p->vars[idx];
          }
          p->state = ARG;
        } else {
          p->state = START;
          i--;
        }
        break;
        
      case ARG:
        if (isalpha(c)) {
          // Check if this could be exponential notation
          if (p->nlen > 0 && strchr("eE", c) && i > 0 && isdigit(s[i-1])) {
            // Likely exponent, treat as number char
            if (p->nlen < MAX_NUMBUF - 1) p->nbuf[p->nlen++] = c;
          } else {
            // New command
            pushnum(p, 0);
            finish(p);
            i--;
          }
        } else if (isdigit(c) || strchr(".-", c)) {
          if (p->nlen < MAX_NUMBUF - 1) p->nbuf[p->nlen++] = c;
        } else if (c == ' ' || c == '\t' || c == ',') {
          pushnum(p, 0);
        } else if (c == '$') {
          pushnum(p, 0);
          p->state = VAR;
        } else if (c == ';') {
          finish(p);
        } else if (c == '\n') {
          if (p->narg > 0 || p->nlen > 0 || p->cmd[0]) finish(p);
        } else if (c == '#') {
          finish(p);
          p->state = COMMENT;
        } else if (c == '{') {
          finish(p);
          p->state = BRACE;
        } else if (c == '(') {
          finish(p);
          p->state = PAREN;
        } else if (strchr("/+=~", c)) {
          pushnum(p, 0);
          finish(p);
          i--;
        }
        break;
        
      case START:
        if (c == '#') p->state = COMMENT;
        else if (c == ';') finish(p);
        else if (c == '{') { finish(p); p->state = BRACE; }
        else if (c == '(') { finish(p); p->state = PAREN; }
        else if (c == '$') p->state = VAR;
        else if (isalpha(c) || strchr("/+=~", c)) startcmd(p, c);
        else if (c == '\n') {
          if (p->narg > 0 || p->nlen > 0 || p->cmd[0]) finish(p);
        }
        break;
    }
  }
}

int main(int argc, char **argv) {
  P p = {0};
  const char *tests[] = {
    "f100\n", "f 100\n", "/f 10 20\n", "g 1 2 3\n", "{text}\n", 
    "(1 2 3)\n", "=a 10\n", "g $a\n", "m\n", "200 150\n", "ae1,2,3\n",
    "g1e-5\n", "f1.5e3x100\n"
  };
  
  if (argc > 1) {
    char *name = argv[1];
    FILE *f = stdin;
    if (strcmp(name, "-") != 0) f = fopen(name, "r");
    if (f) {
      char line[1024];
      while (fgets(line, 1024, f)) parse(&p, line, strlen(line));
      fclose(f);
    }
  } else {
    for (int i = 0; i < sizeof(tests)/sizeof(tests[0]); i++)
      parse(&p, tests[i], strlen(tests[i]));
  }
  
  if (p.cmd[0] || p.narg > 0) finish(&p);
  return 0;
}
