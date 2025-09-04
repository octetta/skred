#include <stdio.h>
#include <stdlib.h>
#include <math.h>
int main(int argc, char *argv[]) {
  float f = 10;
  int d = 10;
  if (argc > 1) {
    f = atof(argv[1]);
    d = atoi(argv[1]);
  }
  printf("  int %d %d\n", -d % 2048, d % 2048);
  printf("float %g %g\n", fmodf(f,2048.1), fmodf(f,2048.1));
}
