#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define MAX_NUMBERS 8
#define MAX_BRACE_BUFFER 65536
#define MAX_PAREN_NUMBERS 128
#define MAX_VARIABLES 62

typedef enum {
  SKODE_START, SKODE_COMMAND, SKODE_NUMBER, SKODE_COMMENT,
  SKODE_BRACE_CONTENT, SKODE_PAREN_NUMBERS, SKODE_VARIABLE
} skode_state_t;

typedef struct {
  char cmd[3];
  float numbers[MAX_NUMBERS];
  int num_count;
  char brace_buf[MAX_BRACE_BUFFER];
  int brace_len;
  int brace_complete;
  float paren_numbers[MAX_PAREN_NUMBERS];
  int paren_count;
  int paren_complete;
} skode_command_t;

typedef struct {
  skode_state_t state;
  char cmd_buf[3], num_buf[32], var_buf[32];
  int cmd_len, num_len, var_len;
  skode_command_t current;
  float variables[MAX_VARIABLES];
  int var_defined[MAX_VARIABLES];
} skode_t;

void skode_init(skode_t *p) {
  memset(p, 0, sizeof(skode_t));
}

static void emit_command(skode_command_t *cmd) {
  if (cmd->cmd[0] && cmd->cmd[0] != '=') {
    printf("Command: %s", cmd->cmd);
    if (cmd->num_count > 0) {
      printf(" [");
      for (int i = 0; i < cmd->num_count; i++)
        printf("%s%.2f", i ? ", " : "", cmd->numbers[i]);
      printf("]");
    }
    printf("\n");
  }
  if (cmd->brace_len > 0 && cmd->brace_complete)
    printf("Brace content (%d bytes): {%.*s}\n", cmd->brace_len, cmd->brace_len, cmd->brace_buf);
  if (cmd->paren_count > 0 && cmd->paren_complete) {
    printf("Paren numbers [%d]: (", cmd->paren_count);
    for (int i = 0; i < cmd->paren_count; i++)
      printf("%s%.2f", i ? " " : "", cmd->paren_numbers[i]);
    printf(")\n");
  }
}

static void finish_number(skode_t *p) {
  if (p->num_len > 0) {
    p->num_buf[p->num_len] = '\0';
    if (p->current.num_count < MAX_NUMBERS)
      p->current.numbers[p->current.num_count++] = atof(p->num_buf);
    p->num_len = 0;
  }
}

static void finish_paren_number(skode_t *p) {
  if (p->num_len > 0) {
    p->num_buf[p->num_len] = '\0';
    if (p->current.paren_count < MAX_PAREN_NUMBERS)
      p->current.paren_numbers[p->current.paren_count++] = atof(p->num_buf);
    p->num_len = 0;
  }
}

static int var_name_to_index(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'z') return 10 + (c - 'a');
  if (c >= 'A' && c <= 'Z') return 36 + (c - 'A');
  return -1;
}

static void handle_variable_assignment(skode_t *p) {
  if (p->var_len == 1 && p->current.num_count == 1) {
    int idx = var_name_to_index(p->var_buf[0]);
    if (idx >= 0 && idx < MAX_VARIABLES) {
      p->variables[idx] = p->current.numbers[0];
      p->var_defined[idx] = 1;
      printf("Set $%c = %.2f\n", p->var_buf[0], p->variables[idx]);
    }
  }
  p->var_len = 0;
}

static float get_variable_value(skode_t *p, char c) {
  int idx = var_name_to_index(c);
  return (idx >= 0 && idx < MAX_VARIABLES && p->var_defined[idx]) ? p->variables[idx] : 0.0f;
}

void skode_finish(skode_t *p, int debug) {
  finish_number(p);
  if (p->current.cmd[0] == '=') handle_variable_assignment(p);
  if (debug) emit_command(&p->current);
  memset(&p->current, 0, sizeof(skode_command_t));
  p->state = SKODE_START;
}

static int is_cmd_char(char c) {
  return isalpha(c) || c == '/' || c == '+' || c == '~' || c == '=' || c == '$';
}

static int is_num_char(char c) {
  return isdigit(c) || c == '.' || c == '-' || c == 'e' || c == 'E';
}

void skode_chunk(skode_t *p, const char *chunk, int len, int debug) {
  for (int i = 0; i < len; i++) {
    char c = chunk[i];
    
    switch (p->state) {
      case SKODE_START:
        if (c == '#') { skode_finish(p, debug); p->state = SKODE_COMMENT; }
        else if (c == ';') skode_finish(p, debug);
        else if (c == '\n' && (p->current.num_count > 0 || p->num_len > 0)) skode_finish(p, debug);
        else if (c == '{') { skode_finish(p, debug); p->state = SKODE_BRACE_CONTENT; }
        else if (c == '(') { skode_finish(p, debug); p->state = SKODE_PAREN_NUMBERS; }
        else if (c == '/') { skode_finish(p, debug); p->cmd_buf[0] = '/'; p->cmd_len = 1; p->state = SKODE_COMMAND; }
        else if (c == '=') { skode_finish(p, debug); strcpy(p->current.cmd, "="); p->var_len = 0; p->state = SKODE_VARIABLE; }
        else if (c == '$') { p->var_len = 0; p->state = SKODE_VARIABLE; }
        else if (c == '+' || c == '~' || isalpha(c)) {
          skode_finish(p, debug);
          p->current.cmd[0] = c; p->current.cmd[1] = '\0';
          p->state = SKODE_NUMBER;
        }
        break;
        
      case SKODE_COMMAND:
        if (isalpha(c)) {
          p->cmd_buf[p->cmd_len++] = c; p->cmd_buf[p->cmd_len] = '\0';
          strcpy(p->current.cmd, p->cmd_buf);
          p->state = SKODE_NUMBER;
        } else if (c == '\n' || c == ';') skode_finish(p, debug);
        else if (!isspace(c) && c != ',') p->state = SKODE_START;
        break;
        
      case SKODE_NUMBER:
        if (c == '$') { finish_number(p); p->var_len = 0; p->state = SKODE_VARIABLE; }
        else if (is_num_char(c) && p->num_len < 31) p->num_buf[p->num_len++] = c;
        else if (c == ' ' || c == '\t' || c == ',') finish_number(p);
        else if (c == ';') skode_finish(p, debug);
        else if (c == '\n' && (p->current.num_count > 0 || p->num_len > 0)) skode_finish(p, debug);
        else if (c == '#') { skode_finish(p, debug); p->state = SKODE_COMMENT; }
        else if (is_cmd_char(c)) { skode_finish(p, debug); i--; }
        else if (c == '{') { skode_finish(p, debug); p->state = SKODE_BRACE_CONTENT; }
        else if (c == '(') { skode_finish(p, debug); p->state = SKODE_PAREN_NUMBERS; }
        break;
        
      case SKODE_BRACE_CONTENT:
        if (c == '}') { p->current.brace_complete = 1; skode_finish(p, debug); }
        else if (p->current.brace_len < MAX_BRACE_BUFFER - 1) 
          p->current.brace_buf[p->current.brace_len++] = c;
        break;
        
      case SKODE_PAREN_NUMBERS:
        if (is_num_char(c) && p->num_len < 31) p->num_buf[p->num_len++] = c;
        else if (c == ' ' || c == '\t' || c == '\n' || c == ',') finish_paren_number(p);
        else if (c == ')') { finish_paren_number(p); p->current.paren_complete = 1; skode_finish(p, debug); }
        break;
        
      case SKODE_VARIABLE:
        if ((c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')) {
          if (p->var_len < 31) p->var_buf[p->var_len++] = c;
          if (p->current.cmd[0] != '=') {
            if (p->current.num_count < MAX_NUMBERS)
              p->current.numbers[p->current.num_count++] = get_variable_value(p, c);
            p->var_len = 0;
          }
          p->state = SKODE_NUMBER;
        } else { p->var_len = 0; p->state = SKODE_START; i--; }
        break;
        
      case SKODE_COMMENT:
        if (c == '\n') p->state = SKODE_START;
        break;
    }
  }
}

void test(skode_t *parser) {
  const char *tests[] = {
    "f100 ", "f 100\n", "f 100.1 -5.1\n", "/f 10 10\n", "a 100 ; b ", "; c 100 100.\n",
    "# comment line\n", "g 1 2 3\n", "x 1.5e3 2.1e-2 3E+4\n", "v0Tv1T+.5~.5\n",
    "/s/s/s\n", "+ .5\n", "~ .5\n", "m\n", "200 150\n", "{hello world}\n",
    "(1.5 2.3 -4.5)\n", "g1 {some text} (10 20 30)\n", "(1 2 3 4 5\n", "-1.5 1e-5\n",
    "1 2 3\n", "100\n", ")\n", "=a 10\n", "=b 20.5\n", "=0 100\n", "g $a $b\n",
    "f $0\n", "x $a $b $0\n"
  };
  
  printf("Parsing test chunks:\n====================\n\n");
  for (int i = 0; i < sizeof(tests)/sizeof(tests[0]); i++) {
    printf("Chunk %d: \"%s\"\n", i+1, tests[i]);
    skode_chunk(parser, tests[i], strlen(tests[i]), 1);
  }
  if (parser->current.cmd[0] || parser->num_len > 0) skode_finish(parser, 1);
}

int main(int argc, char *argv[]) {
  skode_t parser;
  skode_init(&parser);
  
  if (argc > 1) {
    FILE *in = stdin;
    char *name = argv[1];
    if (strcmp(name, "-") != 0) in = fopen(name, "r");
    if (in) {
      char line[1024];
      while (fgets(line, sizeof(line), in))
        if (strlen(line) > 0) { printf("<%s>\n", line); skode_chunk(&parser, line, strlen(line), 1); }
      fclose(in);
    }
  } else test(&parser);
  
  return 0;
}
