#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ands.h"

int example_callback(ands_t *s, int info);

int patch_load(int which) {
  char file[1024];
  sprintf(file, "%d.sk", which);
  FILE *in = fopen(file, "r");
  int r = 0;
  if (in) {
    int user = 0;
    ands_t *s = ands_new(example_callback, &user);
    char line[1024];
    while (fgets(line, sizeof(line), in) != NULL) {
      size_t len = strlen(line);
      if (len > 0 && line[len-1] == '\n') line[len-1] = ';';
      printf("# %s\n", line);
      ands_consume(s, line, example_callback);
    }
    fclose(in);
    ands_free(s);
  }
  return r;
}

int example_callback(ands_t *ctx, int info) {
  int *user = (int*)ands_user(ctx);
  if (info == FUNCTION) {
    printf("FUNCTION %s [", atom_string(ands_atom_num(ctx)));
    // run the callback here
    if (ands_arg_len(ctx)) {
      for (int n=0; n<ands_arg_len(ctx); n++) printf(" %g", ands_arg(ctx)[n]);
    }
    printf(" ]");
    if (ands_string_len(ctx)) printf(" {%s}", ands_string(ctx));
    if (ands_data_len(ctx)) {
      printf(" (");
      for (int i=0; i<ands_data_len(ctx); i++) {
        if (i < 4 || i == ands_data_len(ctx)-1) printf(" %g", ands_data(ctx)[i]);
        else if (i == 4) printf(" ...skip %d...", ands_data_len(ctx)-5);
      }
      printf(" )");
    }
    printf("\n");
    switch (ands_atom_num(ctx)) {
      case ATOM4('drop'):
        puts("DROP");
        ands_arg_drop(ctx);
        return 1;
        break;
      case 'swap':
        puts("SWAP");
        ands_arg_swap(ctx);
        return 1;
        break;
      case ATOM4('f---'):
        puts("FREQ");
        if (ands_arg_len(ctx) == 0) {
          ands_arg_clear(ctx);
          ands_arg(ctx)[0] = 355;
          ands_arg(ctx)[1] = 113;
          ands_arg_len_set(ctx, 2);
          return 1;
        }
        break;
      case ATOM4('push'):
        puts("PUSH");
        {
          double x = 0;
          if (ands_arg_len(ctx) == 0) return 0;
          x = ands_arg(ctx)[0];
          ands_arg_drop(ctx);
          ands_arg_push(ctx, x);
          return 1;
        }
        break;
      case ATOM4('=---'):
        puts("ASSIGN");
        if (ands_arg_len(ctx) > 1) {
          int n = (int)ands_arg(ctx)[0];
          double x = ands_arg(ctx)[1];
          if (n>=0&&n<=9) {
            printf("%d <- %g\n", n, x);
            ands_set_local(ctx, n, x);
          }
        }
        break;
      case ATOM4('/q--'):
        printf("QUIT\n");
        *user = -1;
        break;
      case ATOM4('/l--'):
        puts("LOAD");
        if (ands_arg_len(ctx)) {
          printf("patch_load %d\n", (int)ands_arg(ctx)[0]);
          patch_load((int)ands_arg(ctx)[0]);
        }
        break;
      case ATOM4('/t--'):
        puts("TRACE");
        if (ands_arg_len(ctx)) {
          ands_trace_set(ctx, (int)ands_arg(ctx)[0]);
        }
        break;
    }
  } else if (info == DEFER) {
    printf("DEFER %c %g '%s'\n",
      ands_defer_mode(ctx),
      ands_defer_num(ctx),
      ands_defer_string(ctx));
  }
  return 0;
}

#include "bestline.h"
#define HISTORY_FILE ".ands_history"

int main(int argc, char *arg[]) {
  int user = 0;
  ands_t *ctx = ands_new(example_callback, &user);
  bestlineHistoryLoad(HISTORY_FILE);
  while (1) {
    char *line = NULL;
    line = bestlineWithHistory("# ", NULL);
    if (line == NULL) break;
    bestlineHistoryAdd(line);
    ands_consume(ctx, line, example_callback);
    free(line);
    if (user == -1) {
      printf("must quit\n");
      break;
    }
  }
  bestlineHistorySave(HISTORY_FILE);
  ands_free(ctx);
  return 0;
}
