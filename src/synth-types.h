#ifndef _SYNTH_TYPES_H_
#define _SYNTH_TYPES_H_

#define MAIN_SAMPLE_RATE (44100)
#define VOICE_MAX (64)
#define AUDIO_CHANNELS (2)
#define AMY_FACTOR (0.025f)
#define SYNTH_FRAMES_PER_CALLBACK (512)

#define NEG_60_DB (-60.0f)
#define NEG_60_DB_AS_LINEAR (0.001)
#define SILENT NEG_60_DB

enum {
  WAVE_TABLE_SINE,     // 0
  WAVE_TABLE_SQR,      // 1
  WAVE_TABLE_SAW_DOWN, // 2
  WAVE_TABLE_SAW_UP,   // 3
  WAVE_TABLE_TRI,      // 4
  WAVE_TABLE_NOISE,    // 5
  WAVE_TABLE_NOISE_ALT,// 6

  WAVE_TABLE_KRG1 = 10, // was 32
  WAVE_TABLE_KRG2,  //11
  WAVE_TABLE_KRG3,  //12
  WAVE_TABLE_KRG4,  //13
  WAVE_TABLE_KRG5,  //14
  WAVE_TABLE_KRG6,  //15
  WAVE_TABLE_KRG7,  //16
  WAVE_TABLE_KRG8,  //17
  WAVE_TABLE_KRG9,  //18
  WAVE_TABLE_KRG10, //19
  WAVE_TABLE_KRG11, //20
  WAVE_TABLE_KRG12, //21
  WAVE_TABLE_KRG13, //22
  WAVE_TABLE_KRG14, //23
  WAVE_TABLE_KRG15, //24
  WAVE_TABLE_KRG16, // 25, // was 47

  WAVE_TABLE_KRG17, // 26, // was 48
  WAVE_TABLE_KRG18, // 27
  WAVE_TABLE_KRG19, // 28
  WAVE_TABLE_KRG20, // 29
  WAVE_TABLE_KRG21, // 30
  WAVE_TABLE_KRG22, // 31
  WAVE_TABLE_KRG23, // 32
  WAVE_TABLE_KRG24, // 33
  WAVE_TABLE_KRG25, // 34
  WAVE_TABLE_KRG26, // 35
  WAVE_TABLE_KRG27, // 36
  WAVE_TABLE_KRG28, // 37
  WAVE_TABLE_KRG29, // 38
  WAVE_TABLE_KRG30, // 39
  WAVE_TABLE_KRG31, // 40
  WAVE_TABLE_KRG32, // 41, // was 63

  EW_00 = 50,
  EW_01,
  EW_02,
  EW_03,
  EW_04,
  EW_05,
  EW_99 = 50+99,

  AMY_SAMPLE_00 = 200,    // was 100
  AMY_SAMPLE_99 = 200+99, // was 199

  EXT_SAMPLE_000 = 300,        // was 200
  EXT_SAMPLE_999 = 300 + 999,  // 999
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
    float amplitude_at_release;
    float current_amplitude;
    float amplitude_at_trigger; // The "floor" for the current attack
} envelope_t;

#define WAVE_NAME_MAX (16+1)
typedef char wave_name_t[WAVE_NAME_MAX];

#endif
