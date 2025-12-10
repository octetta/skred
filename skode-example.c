#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "skode.h"

int wire_cb(skode_t *s, int info);

int patch_load(int which) {
  char file[1024];
  sprintf(file, "%d.sk", which);
  FILE *in = fopen(file, "r");
  int r = 0;
  if (in) {
    int user = 0;
    skode_t *s = skode_new(wire_cb, &user);
    char line[1024];
    while (fgets(line, sizeof(line), in) != NULL) {
      size_t len = strlen(line);
      if (len > 0 && line[len-1] == '\n') line[len-1] = ';';
      printf("# %s\n", line);
      skode(s, line, wire_cb);
    }
    fclose(in);
    skode_free(s);
  }
  return r;
}

int wire_cb(skode_t *s, int info) {
  int *user = (int*)skode_user(s);
  if (info == FUNCTION) {
    printf("FUNCTION %s [", atom_string(skode_atom_num(s)));
    // run the callback here
    if (skode_arg_len(s)) {
      for (int n=0; n<skode_arg_len(s); n++) printf(" %g", skode_arg(s)[n]);
    }
    printf(" ]");
    if (skode_string_len(s)) printf(" {%s}", skode_string(s));
    if (skode_data_len(s)) {
      printf(" (");
      for (int i=0; i<skode_data_len(s); i++) {
        if (i < 4 || i == skode_data_len(s)-1) printf(" %g", skode_data(s)[i]);
        else if (i == 4) printf(" ...skip %d...", skode_data_len(s)-5);
      }
      printf(" )");
    }
    printf("\n");
    switch (skode_atom_num(s)) {
      case 'drop':
        skode_arg_drop(s);
        return 1;
        break;
      case 'swap':
        skode_arg_swap(s);
        return 1;
        break;
      case 'f___':
        if (skode_arg_len(s) == 0) {
          skode_arg_clear(s);
          skode_arg(s)[0] = 355;
          skode_arg(s)[1] = 113;
          skode_arg_len_set(s, 2);
          return 1;
        }
        break;
      case 'push':
        {
          double x = 0;
          if (skode_arg_len(s) == 0) return 0;
          x = skode_arg(s)[0];
          skode_arg_drop(s);
          skode_arg_push(s, x);
          return 1;
        }
        break;
      case '=___':
        if (skode_arg_len(s) > 1) {
          int n = (int)skode_arg(s)[0];
          double x = skode_arg(s)[1];
          if (n>=0&&n<=0) {
            printf("%d <- %g\n", n, x);
            skode_set_local(s, n, x);
          }
        }
        break;
      //case '=a__':
      //  s->local_var[0] = s->arg[0];
      //  break;
      case ':q__':
      case '/q__':
        printf("QUIT\n");
        *user = -1;
        break;
      case ':l__':
      case '/l__':
        if (skode_arg_len(s)) {
          printf("patch_load %d\n", (int)skode_arg(s)[0]);
          patch_load((int)skode_arg(s)[0]);
        }
        break;
      case ':t__':
      case '/t__':
        if (skode_arg_len(s)) {
          skode_trace_set(s, (int)skode_arg(s)[0]);
        }
        break;
    }
  } else if (info == DEFER) {
    printf("DEFER %c %g '%s'\n",
      skode_defer_mode(s),
      skode_defer_num(s),
      skode_defer_string(s));
  }
  return 0;
}

#include "linenoise.h"
#define HISTORY_FILE ".skode_history"

int main(int argc, char *arg[]) {
  int user = 0;
  skode_t *s = skode_new(wire_cb, &user);
  linenoiseHistoryLoad(HISTORY_FILE);
  while (1) {
    char *line = NULL;
    line = linenoise("# ");
    if (line == NULL) break;
    linenoiseHistoryAdd(line);
    skode(s, line, wire_cb);
    linenoiseFree(line);
    if (user == -1) {
      printf("must quit\n");
      break;
    }
  }
  linenoiseHistorySave(HISTORY_FILE);
  skode_free(s);
  return 0;
}
