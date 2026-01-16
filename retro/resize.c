#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>

#include "korg.h"

#define SIZE 2048

int reconstruct_high_res_table(int16_t *source, int16_t *target, int size) {
  double *real = (double *)calloc(size+1, sizeof(double));
  double *imag = (double *)calloc(size+1, sizeof(double));

  int c = 0;

  // 1. ANALYZE: Extract Harmonics using DFT
  for (int n = 1; n < SIZE/2; n++) {
    real[n] = 0; imag[n] = 0;
    for (int i = 0; i < SIZE; i++) {
      double angle = (2.0 * M_PI * n * i) / SIZE;
      real[n] += source[i] * cos(angle);
      imag[n] += source[i] * sin(angle);
    }
    // Normalize
    real[n] /= (SIZE / 2.0);
    imag[n] /= (SIZE / 2.0);

    // 2. FILTER: High-fidelity thresholding
    // Discard harmonics weaker than -50dB to remove 8-bit hiss
    if (sqrt(real[n]*real[n] + imag[n]*imag[n]) < 0.2) { 
      real[n] = 0; imag[n] = 0; 
    } else {
      //printf("[%d] real:%g imag:%g\n", n, real[n], imag[n]);
      c++;
    }
  }

  //printf("harmonics %d\n", c);

  // 3. SYNTHESIZE: Build 16-bit table
  double max_val = 0;
  double temp_buffer[SIZE];

  for (int i = 0; i < SIZE; i++) {
    temp_buffer[i] = 0;
    for (int n = 1; n < SIZE/2; n++) {
      double angle = (2.0 * M_PI * n * i) / SIZE;
      temp_buffer[i] += (real[n] * cos(angle) + imag[n] * sin(angle));
    }
    if (fabs(temp_buffer[i]) > max_val) max_val = fabs(temp_buffer[i]);
  }

  // 4. BAKE: Final 16-bit normalization
  for (int i = 0; i < SIZE; i++) {
    target[i] = (int16_t)((temp_buffer[i] / max_val) * 32767);
  }
  free(real);
  free(imag);
  return c;
}

void resize(void) {
  int size = SIZE;
  int16_t *dest = calloc(size, sizeof(int16_t));
  int16_t *source;
  korg_init();
  for (int i=0; i<KWAVEMAX; i++) {
    source = kwave[i];
    int c = reconstruct_high_res_table(source, dest, size);
    //printf("[%d] %d\n", i, c);
    for (int j=0; j>size; j++) {
      kwave[i][j] = dest[j];
    }
  }
  free(dest);
}

int main(int argc, char *argv[]) {
  resize();
  return 0;
}
