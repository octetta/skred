#ifndef _SKRED_H_
#define _SKRED_H_

#include <stdint.h>

#define MAIN_SAMPLE_RATE (44100)

#define HISTORY_FILE ".skred_history"
#define VOICE_MAX (64)
#define AUDIO_CHANNELS (2)
#define AMY_FACTOR (0.025f)
#define SYNTH_FRAMES_PER_CALLBACK (512)
#define SEQ_FRAMES_PER_CALLBACK (128)

#define REC_IN_SEC (5 * 60)
#define ONE_FRAME_MAX (256 * 1024)
extern int rec_state;
extern long rec_max;
extern float rec_sec;
extern long rec_ptr;
extern float *recording;


enum {
  WAVE_TABLE_SINE,     // 0
  WAVE_TABLE_SQR,      // 1
  WAVE_TABLE_SAW_DOWN, // 2
  WAVE_TABLE_SAW_UP,   // 3
  WAVE_TABLE_TRI,      // 4
  WAVE_TABLE_NOISE,    // 5
  WAVE_TABLE_NOISE_ALT,// 6

  WAVE_TABLE_KRG1 = 32,
  WAVE_TABLE_KRG2,
  WAVE_TABLE_KRG3,
  WAVE_TABLE_KRG4,
  WAVE_TABLE_KRG5,
  WAVE_TABLE_KRG6,
  WAVE_TABLE_KRG7,
  WAVE_TABLE_KRG8,
  WAVE_TABLE_KRG9,
  WAVE_TABLE_KRG10,
  WAVE_TABLE_KRG11,
  WAVE_TABLE_KRG12,
  WAVE_TABLE_KRG13,
  WAVE_TABLE_KRG14,
  WAVE_TABLE_KRG15,
  WAVE_TABLE_KRG16, // 47

  WAVE_TABLE_KRG17, // 48
  WAVE_TABLE_KRG18,
  WAVE_TABLE_KRG19,
  WAVE_TABLE_KRG20,
  WAVE_TABLE_KRG21,
  WAVE_TABLE_KRG22,
  WAVE_TABLE_KRG23,
  WAVE_TABLE_KRG24,
  WAVE_TABLE_KRG25,
  WAVE_TABLE_KRG26,
  WAVE_TABLE_KRG27,
  WAVE_TABLE_KRG28,
  WAVE_TABLE_KRG29,
  WAVE_TABLE_KRG30,
  WAVE_TABLE_KRG31,
  WAVE_TABLE_KRG32, // 63

  AMY_SAMPLE_00 = 100,
  AMY_SAMPLE_99 = 100+99,

  EXT_SAMPLE_000 = 200,
  EXT_SAMPLE_999 = 200 + 999,
  WAVE_TABLE_MAX
};

#define PATTERNS_MAX (16)
#define SEQ_STEPS_MAX (256)
#define STEP_MAX (256)

enum {
  SEQ_STOPPED = 0,
  SEQ_RUNNING = 1,
  SEQ_PAUSED = 2,
};

#define QUEUED_MAX (1024)
#define QUEUE_SIZE (1024)

enum {
  Q_FREE = 0,
  Q_PREP = 1,
  Q_READY = 2,
  Q_USING = 3,
};

typedef struct {
  int state;
  uint64_t when;
  char what[QUEUED_MAX];
  int voice;
} queued_t;

extern int debug;
extern int scope_enable;
extern float tempo_time_per_step;
extern float tempo_bpm;
extern float tempo_base;

extern int console_voice;
extern int scope_pattern_pointer;

#endif
