#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>

#include "korg.h"

#define SIZE 2048

int reconstruct_high_res_table(int16_t* source_8bit, int16_t* target_16bit) {
  double real[SIZE/2];
  double imag[SIZE/2];

  int c = 0;

  // 1. ANALYZE: Extract Harmonics using DFT
  for (int n = 1; n < SIZE/2; n++) {
    real[n] = 0; imag[n] = 0;
    for (int i = 0; i < SIZE; i++) {
      double angle = (2.0 * M_PI * n * i) / SIZE;
      real[n] += source_8bit[i] * cos(angle);
      imag[n] += source_8bit[i] * sin(angle);
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
    target_16bit[i] = (int16_t)((temp_buffer[i] / max_val) * 32767);
  }
  return c;
}

int main(int argc, char *argv[]) {
  int16_t dest[SIZE];
  int16_t *source;
  korg_init();
  for (int i=0; i<KWAVEMAX; i++) {
    source = kwave[i];
    int c = reconstruct_high_res_table(source, dest);
    printf("[%d] %d\n", i, c);
  }
  return 0;
}
