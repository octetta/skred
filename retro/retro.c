#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
/*
k wave ROM organization:

Each ROM contains 4 waves, 8bit unsigned format, with the following data:
2048 samples octave 0
2048 samples octave 1
1024 samples octave 2
1024 samples octave 3
512 samples octave 4
512 samples octave 5
512 samples octave 6
512 samples octave 7

Standard waves:
HN613256P-T70	1-4
HN613256P-T71	5-8
HN613256P-CB4	9-12
HN613256P-CB5	13-16

Expansion waves ("Version E"):
EXP-1		1-4
EXP-3		5-8
EXP-2		9-12
EXP-4		13-16

*/

int16_t kw00[] = {
  #include "HN613256P_T70.w0" //	1-4
};
int16_t kw01[] = {
  #include "HN613256P_T70.w1" //	1-4
  };
int16_t kw02[] = {
  #include "HN613256P_T70.w2" //	1-4
  };
int16_t kw03[] = {
  #include "HN613256P_T70.w3" //	1-4
  };

int16_t kw04[] = {
  #include "HN613256P_T71.w0" //	5-8
  };
int16_t kw05[] = {
  #include "HN613256P_T71.w1" //	5-8
  };
int16_t kw06[] = {
  #include "HN613256P_T71.w2" //	5-8
  };
int16_t kw07[] = {
  #include "HN613256P_T71.w3" //	5-8
  };

int16_t kw08[] = {
  #include "HN613256P_CB4.w0" //	9-12
  };
int16_t kw09[] = {
  #include "HN613256P_CB4.w1" //	9-12
  };
int16_t kw10[] = {
  #include "HN613256P_CB4.w2" //	9-12
  };
int16_t kw11[] = {
  #include "HN613256P_CB4.w3" //	9-12
  };

int16_t kw12[] = {
  #include "HN613256P_CB5.w0" //	13-16
  };
int16_t kw13[] = {
  #include "HN613256P_CB5.w1" //	13-16
  };
int16_t kw14[] = {
  #include "HN613256P_CB5.w2" //	13-16
  };
int16_t kw15[] = {
  #include "HN613256P_CB5.w3" //	13-16
  };

//

int16_t kw16[] = {
  #include "EXP_1.w0" //	1-4
  };
int16_t kw17[] = {
  #include "EXP_1.w1" //	1-4
  };
int16_t kw18[] = {
  #include "EXP_1.w2" //	1-4
  };
int16_t kw19[] = {
  #include "EXP_1.w3" //	1-4
  };

int16_t kw20[] = {
  #include "EXP_2.w0" //	5-8
  };
int16_t kw21[] = {
  #include "EXP_2.w1" //	5-8
  };
int16_t kw22[] = {
  #include "EXP_2.w2" //	5-8
  };
int16_t kw23[] = {
  #include "EXP_2.w3" //	5-8
  };

int16_t kw24[] = {
  #include "EXP_3.w0" //	9-12
  };
int16_t kw25[] = {
  #include "EXP_3.w1" //	9-12
  };
int16_t kw26[] = {
  #include "EXP_3.w2" //	9-12
  };
int16_t kw27[] = {
  #include "EXP_3.w3" //	9-12
  };

int16_t kw28[] = {
  #include "EXP_4.w0" //	13-16
  };
int16_t kw29[] = {
  #include "EXP_4.w1" //	13-16
  };
int16_t kw30[] = {
  #include "EXP_4.w2" //	13-16
  };
int16_t kw31[] = {
  #include "EXP_4.w3" //	13-16
  };
int16_t kw32[] = {
  #include "out.list" //	13-16
  };



//

#define KWAVEMAX (33)

int16_t *kwave[KWAVEMAX];
int kwave_size[KWAVEMAX];
double kwave_freq[KWAVEMAX];
char *kwave_name[KWAVEMAX];

void resize(void);

void retro_init(void) {
  static int first = 1;
  if (first == 0) return;
  printf("retro_init()\n");
  first = 0;
  kwave[0] = kw00;
  kwave[1] = kw01;
  kwave[2] = kw02;
  kwave[3] = kw03;
  kwave[4] = kw04;
  kwave[5] = kw05;
  kwave[6] = kw06;
  kwave[7] = kw07;
  kwave[8] = kw08;
  kwave[9] = kw09;
  kwave[10] = kw10;
  kwave[11] = kw11;
  kwave[12] = kw12;
  kwave[13] = kw13;
  kwave[14] = kw14;
  kwave[15] = kw15;
  //
  kwave[16] = kw16;
  kwave[17] = kw17;
  kwave[18] = kw18;
  kwave[19] = kw19;
  kwave[20] = kw20;
  kwave[21] = kw21;
  kwave[22] = kw22;
  kwave[23] = kw23;
  kwave[24] = kw24;
  kwave[25] = kw25;
  kwave[26] = kw26;
  kwave[27] = kw27;
  kwave[28] = kw28;
  kwave[29] = kw29;
  kwave[30] = kw30;
  kwave[31] = kw31;
  kwave[32] = kw32;
  //
  kwave_name[0] = "krg-strings";
  kwave_name[1] = "krg-clarinet";
  kwave_name[2] = "krg-apiano";
  kwave_name[3] = "krg-epiano";
  kwave_name[4] = "krg-epiano-hard";
  kwave_name[5] = "krg-clavi";
  kwave_name[6] = "krg-organ";
  kwave_name[7] = "krg-brass";
  kwave_name[8] = "krg-sax";
  kwave_name[9] = "krg-violin";
  kwave_name[10] = "krg-aguitar";
  kwave_name[11] = "krg-dguitar";
  kwave_name[12] = "krg-ebass";
  kwave_name[13] = "krg-dbass";
  kwave_name[14] = "krg-bell";
  kwave_name[15] = "krg-whistle";
  // exp
  kwave_name[16] = "exp-1-aco-01";
  kwave_name[17] = "exp-1-aco-02";
  kwave_name[18] = "exp-1-aco-03";
  kwave_name[19] = "exp-1-aco-04";
  
  kwave_name[20] = "exp-2-per-05";
  kwave_name[21] = "exp-2-per-06";
  kwave_name[22] = "exp-2-per-07";
  kwave_name[23] = "exp-2-per-08";
  
  kwave_name[24] = "exp-3-for-09";
  kwave_name[25] = "exp-3-for-10";
  kwave_name[26] = "exp-3-for-11";
  kwave_name[27] = "exp-3-for-12";
  
  kwave_name[28] = "exp-4-ppg-13";
  kwave_name[29] = "exp-4-ppg-14";
  kwave_name[30] = "exp-4-ppg-15";
  kwave_name[31] = "exp-4-ppg-16";
  kwave_name[32] = "out";

  for (int i=0; i<KWAVEMAX; i++) {
    kwave_size[i] = 2048;
    kwave_freq[i] = 0;
  }
  kwave_size[32] = sizeof(kw32) / sizeof(int16_t);
}

int reconstruct_high_res_table(int16_t *source, int16_t *target, int size) {
  double *real = (double *)calloc(size+1, sizeof(double));
  double *imag = (double *)calloc(size+1, sizeof(double));

  int c = 0;

  // 1. ANALYZE: Extract Harmonics using DFT
  for (int n = 1; n < size/2; n++) {
    real[n] = 0; imag[n] = 0;
    for (int i = 0; i < size; i++) {
      double angle = (2.0 * M_PI * n * i) / size;
      real[n] += source[i] * cos(angle);
      imag[n] += source[i] * sin(angle);
    }
    // Normalize
    real[n] /= (size / 2.0);
    imag[n] /= (size / 2.0);

    // 2. FILTER: High-fidelity thresholding
    // Discard harmonics weaker than -50dB to remove 8-bit hiss
    if (sqrt(real[n]*real[n] + imag[n]*imag[n]) < 0.2) { 
      real[n] = 0; imag[n] = 0; 
    } else {
#ifdef EXE
      //printf("[%d] real:%g imag:%g\n", n, real[n], imag[n]);
#endif
      c++;
    }
  }

#ifdef EXE
  printf("harmonics %d\n", c);
#endif

  // 3. SYNTHESIZE: Build 16-bit table
  double max_val = 0;
  double *temp_buffer = (double *)calloc(size+1, sizeof(double));

  for (int i = 0; i < size; i++) {
    temp_buffer[i] = 0;
    for (int n = 1; n < size/2; n++) {
      double angle = (2.0 * M_PI * n * i) / size;
      temp_buffer[i] += (real[n] * cos(angle) + imag[n] * sin(angle));
    }
    if (fabs(temp_buffer[i]) > max_val) max_val = fabs(temp_buffer[i]);
  }

  // 4. BAKE: Final 16-bit normalization
  for (int i = 0; i < size; i++) {
    target[i] = (int16_t)((temp_buffer[i] / max_val) * 32767);
  }
  free(real);
  free(imag);
  free(temp_buffer);
  return c;
}

void resize(void) {
  printf("resize()\n");
  int size = 2048;
  int16_t *dest = calloc(size, sizeof(int16_t));
  int16_t *source;
  retro_init();
  for (int i=0; i<KWAVEMAX; i++) {
    source = kwave[i];
    int c = reconstruct_high_res_table(source, dest, size);
    c = c;
    //printf("[%d] %d\n", i, c);
    for (int j=0; j>size; j++) {
      kwave[i][j] = dest[j];
    }
  }
  free(dest);
}
