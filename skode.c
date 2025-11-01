#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define MAX_NUMBERS 8

typedef enum {
  SKODE_START,
  SKODE_COMMAND,
  SKODE_NUMBER,
  SKODE_COMMENT
} skode_state_t;

typedef struct {
  char cmd[3];       // Command (e.g., "f", "/f", "+", "~")
  float numbers[MAX_NUMBERS];
  int num_count;
} skode_command;

typedef struct {
  skode_state_t state;
  char cmd_buf[3];
  int cmd_len;
  char num_buf[32];
  int num_len;
  skode_command current;
} Skoder;

void skode_init(Skoder *p) {
  p->state = SKODE_START;
  p->cmd_len = 0;
  p->num_len = 0;
  p->current.cmd[0] = '\0';
  p->current.num_count = 0;
}

void emit_command(skode_command *cmd) {
  if (cmd->cmd[0] == '\0') return;
  
  printf("Command: %s", cmd->cmd);
  if (cmd->num_count > 0) {
    printf(" [");
    for (int i = 0; i < cmd->num_count; i++) {
      printf("%.2f", cmd->numbers[i]);
      if (i < cmd->num_count - 1) printf(", ");
    }
    printf("]");
  }
  printf("\n");
}

void finish_number(Skoder *p) {
  if (p->num_len > 0) {
    p->num_buf[p->num_len] = '\0';
    if (p->current.num_count < MAX_NUMBERS) {
      p->current.numbers[p->current.num_count++] = atof(p->num_buf);
    }
    p->num_len = 0;
  }
}

void skode_finish(Skoder *p) {
  finish_number(p);
  emit_command(&p->current);
  p->current.cmd[0] = '\0';
  p->current.num_count = 0;
  p->state = SKODE_START;
}

int is_command_char(char c) {
  return isalpha(c) || c == '/' || c == '+' || c == '~';
}

int is_number_char(char c) {
  return isdigit(c) || c == '.' || c == '-' || c == 'e' || c == 'E';
}

void skode_chunk(Skoder *p, const char *chunk, int len) {
  for (int i = 0; i < len; i++) {
    char c = chunk[i];
    
    switch (p->state) {
      case SKODE_START:
        if (c == '#') {
          skode_finish(p);
          p->state = SKODE_COMMENT;
        } else if (c == ';') {
          skode_finish(p);
        } else if (c == '\n') {
          // Newline only finishes command if we have numbers
          if (p->current.num_count > 0 || p->num_len > 0) {
            skode_finish(p);
          }
          // Otherwise stay in NUMBER state to accept numbers on next line
        } else if (c == '/') {
          skode_finish(p);
          p->cmd_buf[0] = '/';
          p->cmd_len = 1;
          p->state = SKODE_COMMAND;
        } else if (c == '+' || c == '~') {
          // Timing command
          skode_finish(p);
          p->cmd_buf[0] = c;
          p->cmd_len = 1;
          p->cmd_buf[1] = '\0';
          strcpy(p->current.cmd, p->cmd_buf);
          p->state = SKODE_NUMBER;
        } else if (isalpha(c)) {
          skode_finish(p);
          p->cmd_buf[0] = tolower(c);
          p->cmd_len = 1;
          p->cmd_buf[1] = '\0';
          strcpy(p->current.cmd, p->cmd_buf);
          p->state = SKODE_NUMBER;
        } else if (!isspace(c) && c != ',') {
          // Invalid character at start
        }
        break;
        
      case SKODE_COMMAND:
        if (isalpha(c)) {
          p->cmd_buf[p->cmd_len++] = tolower(c);
          p->cmd_buf[p->cmd_len] = '\0';
          strcpy(p->current.cmd, p->cmd_buf);
          p->state = SKODE_NUMBER;
        } else if (c == '\n' || c == ';') {
          skode_finish(p);
        } else if (!isspace(c) && c != ',') {
          // Invalid after slash
          p->state = SKODE_START;
        }
        break;
        
      case SKODE_NUMBER:
        if (is_number_char(c)) {
          if (p->num_len == 0 || p->num_len < 31) {
            p->num_buf[p->num_len++] = c;
          }
        } else if (c == ' ' || c == '\t' || c == ',') {
          finish_number(p);
        } else if (c == ';') {
          skode_finish(p);
        } else if (c == '\n') {
          // Newline only finishes command if we have numbers
          if (p->current.num_count > 0 || p->num_len > 0) {
            skode_finish(p);
          }
          // Otherwise stay in NUMBER state to accept numbers on next line
        } else if (c == '#') {
          skode_finish(p);
          p->state = SKODE_COMMENT;
        } else if (is_command_char(c)) {
          // Any command character starts a new command
          skode_finish(p);
          i--; // Reprocess this character
        }
        break;
        
      case SKODE_COMMENT:
        if (c == '\n') {
          p->state = SKODE_START;
        }
        break;
    }
  }
}

int main() {
  Skoder parser;
  skode_init(&parser);
  
  // Test cases
  const char *tests[] = {
    "f100 ",
    "f 100\n",
    "f 100.1 -5.1\n",
    "/f 10 10\n",
    "a 100 ; b ",
    "; c 100 100.\n",
    "# comment line\n",
    "g 1 2 3\n",
    "x 1.5e3 2.1e-2 3E+4\n",
    "v0Tv1T+.5~.5\n",
    "/s/s/s\n",
    "+ .5\n",
    "~ .5\n",
    "m\n",
    "200 150\n"
  };
  
  printf("Parsing test chunks:\n");
  printf("====================\n\n");
  
  for (int i = 0; i < sizeof(tests)/sizeof(tests[0]); i++) {
    printf("Chunk %d: \"%s\"\n", i+1, tests[i]);
    skode_chunk(&parser, tests[i], strlen(tests[i]));
  }
  
  // Finish any remaining command
  if (parser.current.cmd[0] != '\0' || parser.num_len > 0) {
    skode_finish(&parser);
  }
  
  return 0;
}
