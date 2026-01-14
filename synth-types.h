#ifndef _SYNTH_TYPES_H_
#define _SYNTH_TYPES_H_

#define MAIN_SAMPLE_RATE (44100)
#define VOICE_MAX (64)
#define AUDIO_CHANNELS (2)
#define AMY_FACTOR (0.025f)
#define SYNTH_FRAMES_PER_CALLBACK (512)

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

enum {
  FILTER_LOWPASS = 1,
  FILTER_HIGHPASS = 2,
  FILTER_BANDPASS = 3,
  FILTER_NOTCH = 4,
  FILTER_ALL_PASS = 5,
};

// Low-pass filter state structure
typedef struct {
  float x1, x2;  // Input delay line
  float y1, y2;  // Output delay line
  float b0, b1, b2;  // Feedforward coefficients
  float a1, a2;      // Feedback coefficients
    
  // Parameter tracking for coefficient updates
  float last_freq;
  float last_resonance;
  int last_mode;
} mmf_t;

#include <stdint.h> // for uint64_t

typedef struct {
    float a;
    float d;
    float s;
    float r;
    float attack_time;    // attack duration in samples
    float decay_time;     // decay duration in samples
    float sustain_level;     // 0 to 1
    float release_time;   // release duration in samples
    uint64_t sample_start;   // sample count when note is triggered
    uint64_t sample_release; // sample count when note is released
    int is_active;            // envelope state
    float velocity; // multiply envelope by this value
} envelope_t;

#endif
