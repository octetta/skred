/*
Wave ROM organization:

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

void korg_init(void) {
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
  kwave_name[0] = "korg-strings";
  kwave_name[1] = "korg-clarinet";
  kwave_name[2] = "korg-apiano";
  kwave_name[3] = "korg-epiano";
  kwave_name[4] = "korg-epiano-hard";
  kwave_name[5] = "korg-clavi";
  kwave_name[6] = "korg-organ";
  kwave_name[7] = "korg-brass";
  kwave_name[8] = "korg-sax";
  kwave_name[9] = "korg-violin";
  kwave_name[10] = "korg-aguitar";
  kwave_name[11] = "korg-dguitar";
  kwave_name[12] = "korg-ebass";
  kwave_name[13] = "korg-dbass";
  kwave_name[14] = "korg-bell";
  kwave_name[15] = "korg-whistle";
  // exp
  kwave_name[16] = "exp-01";
  kwave_name[17] = "exp-02";
  kwave_name[18] = "exp-03";
  kwave_name[19] = "exp-04";
  kwave_name[20] = "exp-05";
  kwave_name[21] = "exp-06";
  kwave_name[22] = "exp-07";
  kwave_name[23] = "exp-08";
  kwave_name[24] = "exp-09";
  kwave_name[25] = "exp-10";
  kwave_name[26] = "exp-11";
  kwave_name[27] = "exp-12";
  kwave_name[28] = "exp-13";
  kwave_name[29] = "exp-14";
  kwave_name[30] = "exp-15";
  kwave_name[31] = "exp-16";
  kwave_name[32] = "out";

  for (int i=0; i<KWAVEMAX; i++) {
    kwave_size[i] = 2048;
    kwave_freq[i] = 0;
  }
  kwave_size[32] = sizeof(kw32) / sizeof(int16_t);
}
