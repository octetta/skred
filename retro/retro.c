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

#define EWAVEMAX (100)

int16_t ew00[] = {
  #include "ew/saw.h"
};
int16_t ew01[] = {
  #include "ew/bell.h"
};
int16_t ew02[] = {
  #include "ew/sine.h"
};
int16_t ew03[] = {
  #include "ew/square.h"
};
int16_t ew04[] = {
  #include "ew/pulse.h"
};
int16_t ew05[] = {
  #include "ew/noise1.h"
};
int16_t ew06[] = {
  #include "ew/noise2.h"
};
int16_t ew07[] = {
  #include "ew/noise3.h"
};
int16_t ew08[] = {
  #include "ew/bass.h"
};
int16_t ew09[] = {
  #include "ew/piano.h"
};
//
int16_t ew10[] = {
  #include "ew/el_pno.h"
};
int16_t ew11[] = {
  #include "ew/voice1.h"
};
int16_t ew12[] = {
  #include "ew/voice2.h"
};
int16_t ew13[] = {
  #include "ew/kick.h"
};
int16_t ew14[] = {
  #include "ew/reed.h"
};
int16_t ew15[] = {
  #include "ew/organ.h"
};
int16_t ew16[] = {
  #include "ew/synth1.h"
};
int16_t ew17[] = {
  #include "ew/synth2.h"
};
int16_t ew18[] = {
  #include "ew/synth3.h"
};
int16_t ew19[] = {
  #include "ew/pulse2.h"
};
//
int16_t ew20[] = {
  #include "ew/sqr2.h"
};
int16_t ew21[] = {
  #include "ew/4octs.h"
};
int16_t ew22[] = {
  #include "ew/prime.h"
};
int16_t ew23[] = {
  #include "ew/bass2.h"
};
int16_t ew24[] = {
  #include "ew/epno2.h"
};
int16_t ew25[] = {
  #include "ew/octave.h"
};
int16_t ew26[] = {
  #include "ew/oct+5.h"
};
int16_t ew27[] = {
  #include "ew/sine.h"
};
int16_t ew28[] = {
  #include "ew/sine.h"
};
int16_t ew29[] = {
  #include "ew/sine.h"
};
//
int16_t ew30[] = {
  #include "ew/sine.h"
};
int16_t ew31[] = {
  #include "ew/sine.h"
};
int16_t ew32[] = {
  #include "ew/triang.h"
};
int16_t ew33[] = {
  #include "ew/reed2.h"
};
int16_t ew34[] = {
  #include "ew/reed3.h"
};
int16_t ew35[] = {
  #include "ew/grit1.h"
};
int16_t ew36[] = {
  #include "ew/grit2.h"
};
int16_t ew37[] = {
  #include "ew/grit3.h"
};
int16_t ew38[] = {
  #include "ew/glint1.h"
};
int16_t ew39[] = {
  #include "ew/glint2.h"
};
//
int16_t ew40[] = {
  #include "ew/breath.h"
};
int16_t ew41[] = {
  #include "ew/voice3.h"
};
int16_t ew42[] = {
  #include "ew/steam.h"
};
int16_t ew43[] = {
  #include "ew/metal.h"
};
int16_t ew44[] = {
  #include "ew/chime.h"
};
int16_t ew45[] = {
  #include "ew/formant1.h"
};
int16_t ew46[] = {
  #include "ew/formant2.h"
};
int16_t ew47[] = {
  #include "ew/formant3.h"
};
int16_t ew48[] = {
  #include "ew/formant4.h"
};
int16_t ew49[] = {
  #include "ew/formant5.h"
};
//
int16_t ew50[] = {
  #include "ew/sine.h"
};
int16_t ew51[] = {
  #include "ew/sine.h"
};
int16_t ew52[] = {
  #include "ew/sine.h"
};
int16_t ew53[] = {
  #include "ew/sine.h"
};
int16_t ew54[] = {
  #include "ew/slap.h"
};
int16_t ew55[] = {
  #include "ew/plink.h"
};
int16_t ew56[] = {
  #include "ew/plunk.h"
};
int16_t ew57[] = {
  #include "ew/click.h"
};
int16_t ew58[] = {
  #include "ew/bowing.h"
};
int16_t ew59[] = {
  #include "ew/sine.h"
};
//
int16_t ew60[] = {
  #include "ew/sine.h"
};
int16_t ew61[] = {
  #include "ew/thump.h"
};
int16_t ew62[] = {
  #include "ew/logdrm.h"
};
int16_t ew63[] = {
  #include "ew/sine.h"
};
int16_t ew64[] = {
  #include "ew/sine.h"
};
int16_t ew65[] = {
  #include "ew/sine.h"
};
int16_t ew66[] = {
  #include "ew/sine.h"
};
int16_t ew67[] = {
  #include "ew/sine.h"
};
int16_t ew68[] = {
  #include "ew/sine.h"
};
int16_t ew69[] = {
  #include "ew/mallet.h"
};
//
int16_t ew70[] = {
  #include "ew/drm1.h"
};
int16_t ew71[] = {
  #include "ew/drm2.h"
};
int16_t ew72[] = {
  #include "ew/drm3.h"
};
int16_t ew73[] = {
  #include "ew/drm4.h"
};
int16_t ew74[] = {
  #include "ew/drm5.h"
};
int16_t ew75[] = {
  #include "ew/sine.h"
};
int16_t ew76[] = {
  #include "ew/sine.h"
};
int16_t ew77[] = {
  #include "ew/sine.h"
};
//

int16_t *ewave[EWAVEMAX];
int ewave_size[EWAVEMAX];
double ewave_freq[KWAVEMAX];
char *ewave_name[KWAVEMAX];


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

  int n = 0;
  ewave[n++] = ew00;
  ewave[n++] = ew01;
  ewave[n++] = ew02;
  ewave[n++] = ew03;
  ewave[n++] = ew04;
  ewave[n++] = ew05;
  ewave[n++] = ew06;
  ewave[n++] = ew07;
  ewave[n++] = ew08;
  ewave[n++] = ew09;
  //
  ewave[n++] = ew10;
  ewave[n++] = ew11;
  ewave[n++] = ew12;
  ewave[n++] = ew13;
  ewave[n++] = ew14;
  ewave[n++] = ew15;
  ewave[n++] = ew16;
  ewave[n++] = ew17;
  ewave[n++] = ew18;
  ewave[n++] = ew19;
  //
  ewave[n++] = ew20;
  ewave[n++] = ew21;
  ewave[n++] = ew22;
  ewave[n++] = ew23;
  ewave[n++] = ew24;
  ewave[n++] = ew25;
  ewave[n++] = ew26;
  ewave[n++] = ew27;
  ewave[n++] = ew28;
  ewave[n++] = ew29;
  //
  ewave[n++] = ew30;
  ewave[n++] = ew31;
  ewave[n++] = ew32;
  ewave[n++] = ew33;
  ewave[n++] = ew34;
  ewave[n++] = ew35;
  ewave[n++] = ew36;
  ewave[n++] = ew37;
  ewave[n++] = ew38;
  ewave[n++] = ew39;
  //
  ewave[n++] = ew40;
  ewave[n++] = ew41;
  ewave[n++] = ew42;
  ewave[n++] = ew43;
  ewave[n++] = ew44;
  ewave[n++] = ew45;
  ewave[n++] = ew46;
  ewave[n++] = ew47;
  ewave[n++] = ew48;
  ewave[n++] = ew49;
  //
  ewave[n++] = ew50;
  ewave[n++] = ew51;
  ewave[n++] = ew52;
  ewave[n++] = ew53;
  ewave[n++] = ew54;
  ewave[n++] = ew55;
  ewave[n++] = ew56;
  ewave[n++] = ew57;
  ewave[n++] = ew58;
  ewave[n++] = ew59;
  //
  ewave[n++] = ew60;
  ewave[n++] = ew61;
  ewave[n++] = ew62;
  ewave[n++] = ew63;
  ewave[n++] = ew64;
  ewave[n++] = ew65;
  ewave[n++] = ew66;
  ewave[n++] = ew67;
  ewave[n++] = ew68;
  ewave[n++] = ew69;
  //
  ewave[n++] = ew70;
  ewave[n++] = ew71;
  ewave[n++] = ew72;
  ewave[n++] = ew73;
  ewave[n++] = ew74;
  ewave[n++] = ew75;
  ewave[n++] = ew76;
  ewave[n++] = ew77;
  //
  n=0;
  ewave_size[n++] = sizeof(ew00) / sizeof(int16_t);
  ewave_size[n++] = sizeof(ew01) / sizeof(int16_t);
  ewave_size[n++] = sizeof(ew02) / sizeof(int16_t);
  ewave_size[n++] = sizeof(ew03) / sizeof(int16_t);
  ewave_size[n++] = sizeof(ew04) / sizeof(int16_t);
  ewave_size[n++] = sizeof(ew05) / sizeof(int16_t);
  ewave_size[n++] = sizeof(ew06) / sizeof(int16_t);
  ewave_size[n++] = sizeof(ew07) / sizeof(int16_t);
  ewave_size[n++] = sizeof(ew08) / sizeof(int16_t);
  ewave_size[n++] = sizeof(ew09) / sizeof(int16_t);
  //
  ewave_size[n++] = sizeof(ew10) / sizeof(int16_t);
  ewave_size[n++] = sizeof(ew11) / sizeof(int16_t);
  ewave_size[n++] = sizeof(ew12) / sizeof(int16_t);
  ewave_size[n++] = sizeof(ew13) / sizeof(int16_t);
  ewave_size[n++] = sizeof(ew14) / sizeof(int16_t);
  ewave_size[n++] = sizeof(ew15) / sizeof(int16_t);
  ewave_size[n++] = sizeof(ew16) / sizeof(int16_t);
  ewave_size[n++] = sizeof(ew17) / sizeof(int16_t);
  ewave_size[n++] = sizeof(ew18) / sizeof(int16_t);
  ewave_size[n++] = sizeof(ew19) / sizeof(int16_t);
  //
  ewave_size[n++] = sizeof(ew20) / sizeof(int16_t);
  ewave_size[n++] = sizeof(ew21) / sizeof(int16_t);
  ewave_size[n++] = sizeof(ew22) / sizeof(int16_t);
  ewave_size[n++] = sizeof(ew23) / sizeof(int16_t);
  ewave_size[n++] = sizeof(ew24) / sizeof(int16_t);
  ewave_size[n++] = sizeof(ew25) / sizeof(int16_t);
  ewave_size[n++] = sizeof(ew26) / sizeof(int16_t);
  ewave_size[n++] = sizeof(ew27) / sizeof(int16_t);
  ewave_size[n++] = sizeof(ew28) / sizeof(int16_t);
  ewave_size[n++] = sizeof(ew29) / sizeof(int16_t);
  //
  ewave_size[n++] = sizeof(ew30) / sizeof(int16_t);
  ewave_size[n++] = sizeof(ew31) / sizeof(int16_t);
  ewave_size[n++] = sizeof(ew32) / sizeof(int16_t);
  ewave_size[n++] = sizeof(ew33) / sizeof(int16_t);
  ewave_size[n++] = sizeof(ew34) / sizeof(int16_t);
  ewave_size[n++] = sizeof(ew35) / sizeof(int16_t);
  ewave_size[n++] = sizeof(ew36) / sizeof(int16_t);
  ewave_size[n++] = sizeof(ew37) / sizeof(int16_t);
  ewave_size[n++] = sizeof(ew38) / sizeof(int16_t);
  ewave_size[n++] = sizeof(ew39) / sizeof(int16_t);
  //
  ewave_size[n++] = sizeof(ew40) / sizeof(int16_t);
  ewave_size[n++] = sizeof(ew41) / sizeof(int16_t);
  ewave_size[n++] = sizeof(ew42) / sizeof(int16_t);
  ewave_size[n++] = sizeof(ew43) / sizeof(int16_t);
  ewave_size[n++] = sizeof(ew44) / sizeof(int16_t);
  ewave_size[n++] = sizeof(ew45) / sizeof(int16_t);
  ewave_size[n++] = sizeof(ew46) / sizeof(int16_t);
  ewave_size[n++] = sizeof(ew47) / sizeof(int16_t);
  ewave_size[n++] = sizeof(ew48) / sizeof(int16_t);
  ewave_size[n++] = sizeof(ew49) / sizeof(int16_t);
  //
  ewave_size[n++] = sizeof(ew50) / sizeof(int16_t);
  ewave_size[n++] = sizeof(ew51) / sizeof(int16_t);
  ewave_size[n++] = sizeof(ew52) / sizeof(int16_t);
  ewave_size[n++] = sizeof(ew53) / sizeof(int16_t);
  ewave_size[n++] = sizeof(ew54) / sizeof(int16_t);
  ewave_size[n++] = sizeof(ew55) / sizeof(int16_t);
  ewave_size[n++] = sizeof(ew56) / sizeof(int16_t);
  ewave_size[n++] = sizeof(ew57) / sizeof(int16_t);
  ewave_size[n++] = sizeof(ew58) / sizeof(int16_t);
  ewave_size[n++] = sizeof(ew59) / sizeof(int16_t);
  //
  ewave_size[n++] = sizeof(ew60) / sizeof(int16_t);
  ewave_size[n++] = sizeof(ew61) / sizeof(int16_t);
  ewave_size[n++] = sizeof(ew62) / sizeof(int16_t);
  ewave_size[n++] = sizeof(ew63) / sizeof(int16_t);
  ewave_size[n++] = sizeof(ew64) / sizeof(int16_t);
  ewave_size[n++] = sizeof(ew65) / sizeof(int16_t);
  ewave_size[n++] = sizeof(ew66) / sizeof(int16_t);
  ewave_size[n++] = sizeof(ew67) / sizeof(int16_t);
  ewave_size[n++] = sizeof(ew68) / sizeof(int16_t);
  ewave_size[n++] = sizeof(ew69) / sizeof(int16_t);
  //
  ewave_size[n++] = sizeof(ew70) / sizeof(int16_t);
  ewave_size[n++] = sizeof(ew71) / sizeof(int16_t);
  ewave_size[n++] = sizeof(ew72) / sizeof(int16_t);
  ewave_size[n++] = sizeof(ew73) / sizeof(int16_t);
  ewave_size[n++] = sizeof(ew74) / sizeof(int16_t);
  ewave_size[n++] = sizeof(ew75) / sizeof(int16_t);
  ewave_size[n++] = sizeof(ew76) / sizeof(int16_t);
  ewave_size[n++] = sizeof(ew77) / sizeof(int16_t);
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
