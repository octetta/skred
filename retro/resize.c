#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>

#include "korg.h"

#if 0
#include <math.h>
#include <stdint.h>

#define SRC_SIZE 2048
#define TGT_SIZE 4096
#define MAX_H 1024 // Max harmonics for a 2048 wave (Nyquist)

void upscale_dw8000_wave(int8_t* src_2048, int16_t* tgt_4096) {
    double real[MAX_H] = {0};
    double imag[MAX_H] = {0};

    // 1. ANALYSIS: Extract the "DNA" from the 2048 source
    for (int n = 1; n < MAX_H; n++) {
        for (int i = 0; i < SRC_SIZE; i++) {
            double angle = (2.0 * M_PI * n * i) / SRC_SIZE;
            real[n] += src_2048[i] * cos(angle);
            imag[n] += src_2048[i] * sin(angle);
        }
        real[n] /= (SRC_SIZE / 2.0);
        imag[n] /= (SRC_SIZE / 2.0);
    }

    // 2. SYNTHESIS: Render at the new 4096 resolution
    double max_val = 0;
    double temp[TGT_SIZE];

    for (int i = 0; i < TGT_SIZE; i++) {
        temp[i] = 0;
        double angle = (2.0 * M_PI * i) / TGT_SIZE;
        
        for (int n = 1; n < MAX_H; n++) {
            // We use the harmonics we found, but apply them to the new 4096 phase
            temp[i] += (real[n] * cos(n * angle) + imag[n] * sin(n * angle));
        }
        if (fabs(temp[i]) > max_val) max_val = fabs(temp[i]);
    }

    // 3. NORMALIZE: Output to 16-bit
    for (int i = 0; i < TGT_SIZE; i++) {
        tgt_4096[i] = (int16_t)((temp[i] / max_val) * 32767);
    }
}
#endif

#ifdef EXE
int main(int argc, char *argv[]) {
  resize();
  return 0;
}
#endif
