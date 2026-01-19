#include <stdio.h>
#include <stdlib.h>
#include "miniwav.h"
#define COLS (5)
int main(int argc, char *argv[]) {
  wav_t wav;
  int len;
  char *name = "wave11.wav";
  if (argc > 1) {
    name = argv[1];
  }
  float *table = mw_get(name, &len, &wav, -1);
  if (table) {
    // printf("# table:%p len:%d\n", table, len);
    printf("D%d\n", len);
    printf("( ");
    int c = 0;
    for (int i = 0; i < len; i++) {
      printf("%.8f ", table[i]);
      if (c++ >= (COLS-1)) {
        puts(" ");
        c = 0;
      }
    }
    puts(" ) ");
  }
  free(table);
  return 0;
}
