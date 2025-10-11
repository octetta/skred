#ifndef _SYNTH_TYPES_H_
#define _SYNTH_TYPES_H_

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
