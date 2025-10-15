#ifndef _WIRE_H_
#define _WIRE_H_

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include <dirent.h>
#include <sys/types.h>

#define VOICE_STACK_LEN (8)

typedef struct {
  float s[VOICE_STACK_LEN];
  int ptr;
} voice_stack_t;

typedef struct {
  int func;
  int sub_func;
  int next;
  int argc;
  float args[8];
} value_t;

#define WIRE_SCRATCH_MAX (1024)

typedef struct {
  int voice;
  voice_stack_t stack;
  int state;
  char scratch[WIRE_SCRATCH_MAX];
  int scratch_pointer;
  float *data;
  int data_max;
  int data_pointer;
  int data_len;
  char data_acc[64];
  int data_acc_ptr;
  char queued[QUEUED_MAX];
  int queued_pointer;
  int last_func;
  int last_sub_func;
  int pattern;
} wire_t;

enum {
  FUNC_NULL,
  FUNC_ERR,
  FUNC_SYS,
  FUNC_IMM,
  //
  FUNC_VOICE,
  FUNC_FREQ,
  FUNC_AMP,
  FUNC_TRIGGER,
  FUNC_VELOCITY,
  FUNC_MUTE,
  FUNC_AMP_MOD,
  FUNC_CZ_MOD,
  FUNC_FREQ_MOD,
  FUNC_PAN_MOD,
  FUNC_MIDI,
  FUNC_WAVE,
  FUNC_LOOP,
  FUNC_DIR,
  FUNC_INTER,
  FUNC_PAN,
  FUNC_ENVELOPE,
  FUNC_QUANT,
  FUNC_HOLD,
  FUNC_RESET,
  FUNC_FILTER_MODE,
  FUNC_FILTER_FREQ,
  FUNC_FILTER_RES,
  FUNC_COPY,
  FUNC_SMOOTHER,
  FUNC_GLISSANDO,
  FUNC_DATA,
  // subfunctions
  FUNC_HELP,
  FUNC_QUIT,
  FUNC_STATS0,
  FUNC_STATS1,
  FUNC_TRACE,
  FUNC_DEBUG,
  FUNC_SCOPE,
  FUNC_LOAD,
  FUNC_SAVE,
  FUNC_WAVE_READ,
  //
  FUNC_WAVE_SHOW,
  FUNC_DELAY,
  FUNC_COMMENT,
  FUNC_WHITESPACE,
  FUNC_METRO,
  FUNC_SEQ,
  FUNC_MAIN_SEQ,
  FUNC_STEP,
  FUNC_STEP_MUTE,
  FUNC_STEP_UNMUTE,
  FUNC_PATTERN,
  FUNC_WAVE_DEFAULT,
  FUNC_CZ,
  FUNC_VOLUME_SET,
  //
  FUNC_UNKNOWN,
};

enum {
  ERR_EXPECTED_INT,
  ERR_EXPECTED_FLOAT,
  ERR_INVALID_VOICE,
  ERR_FREQUENCY_OUT_OF_RANGE,
  ERR_AMPLITUDE_OUT_OF_RANGE,
  ERR_INVALID_WAVE,
  ERR_EMPTY_WAVE,
  ERR_INVALID_DIRECTION,
  ERR_INVALID_LOOPING,
  ERR_PAN_OUT_OF_RANGE,
  ERR_INVALID_DELAY,
  ERR_INVALID_MODULATOR,
  ERR_UNKNOWN_FUNC,
  ERR_UNKNOWN_SYS,
  ERR_INVALID_TRACE,
  ERR_INVALID_DEBUG,
  ERR_INVALID_MUTE,
  ERR_INVALID_EXT_SAMPLE,
  ERR_PARSING,
  ERR_INVALID_PATCH,
  ERR_INVALID_MIDI_NOTE,
  ERR_INVALID_MOD,
  //
  ERR_INVALID_AMP,
  ERR_INVALID_FILTER_MODE,
  ERR_INVALID_FILTER_RES,
  ERR_INVALID_PAN,
  ERR_INVALID_QUANT,
  ERR_INVALID_FREQ,
  ERR_INVALID_WAVETABLE,
  // add new stuff before here...
  ERR_UNKNOWN,
};

enum {
  W_PROTOCOL = 0,
  W_SCRATCH,
  W_DATA,
  W_DATA_0,
  W_DATA_1,
  W_DATA_2,
  W_DATA_3,
  W_DATA_4,
};

int wire(char *line, wire_t *w, int output);
void show_threads(void);
void system_show(void);
int audio_show(void);
int patch_load(int voice, int n, int output);
int wavetable_show(int n);
int audio_show(void);
char *wire_err_str(int n);

#define WIRE() { .voice = 0, .state = W_PROTOCOL, .last_func = FUNC_NULL, \
  .pattern = 0, .data = NULL, .data_max = 0 }

#endif

