#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <unistd.h>

#include "motor.h"

#define SOKOL_AUDIO_IMPL
#include "sokol_audio.h"

static volatile uint64_t tick_self_count = 0;
static volatile uint64_t tick_engine_count = 0;
static volatile uint64_t tick_mod = 22050;
static volatile uint64_t sample_count = 0;

void tick();
void tick_mod_set(uint64_t);

int debug = 0;
int trace = 0;

#define HISTORY_FILE ".sok1_history"
#define MAIN_SAMPLE_RATE (44100)
#define VOICE_MAX (32)
#define CHANNEL_NUM (2)
#define AFACTOR (0.025)

#define SCREENWIDTH (800)
#define SCREENHEIGHT (480)

#define OSCOPE_LEN (MAIN_SAMPLE_RATE)

// inspired by AMY :)
enum {
  EXWAVESINE,     // 0
  EXWAVESQR,      // 1
  EXWAVESAWDN,    // 2
  EXWAVESAWUP,    // 3
  EXWAVETRI,      // 4
  EXWAVENOISE,    // 5
  EXWAVEUSR0,     // 6
  EXWAVEPCM,      // 7
  EXWAVEUSR1,     // 8
  EXWAVEUSR2,     // 9
  EXWAVEUSR3,     // 10
  EXWAVENONE,     // 11
    
  EXWAVEUSR4,     // 12
  EXWAVEUSR5,     // 13
  EXWAVEUSR6,     // 14
  EXWAVEUSR7,     // 15
  
  AMYWAVE00  = 24,
  AMYWAVE01,
  AMYWAVE02,
  AMYWAVE03,
  AMYWAVE04,

  EXWAVEKRG1 = 32,
  EXWAVEKRG2,
  EXWAVEKRG3,
  EXWAVEKRG4,
  EXWAVEKRG5,
  EXWAVEKRG6,
  EXWAVEKRG7,
  EXWAVEKRG8,
  EXWAVEKRG9,
  EXWAVEKRG10,
  EXWAVEKRG11,
  EXWAVEKRG12,
  EXWAVEKRG13,
  EXWAVEKRG14,
  EXWAVEKRG15,
  EXWAVEKRG16,

  EXWAVEKRG17,
  EXWAVEKRG18,
  EXWAVEKRG19,
  EXWAVEKRG20,
  EXWAVEKRG21,
  EXWAVEKRG22,
  EXWAVEKRG23,
  EXWAVEKRG24,
  EXWAVEKRG25,
  EXWAVEKRG26,
  EXWAVEKRG27,
  EXWAVEKRG28,
  EXWAVEKRG29,
  EXWAVEKRG30,
  EXWAVEKRG31,
  EXWAVEKRG32,

  AMYSAMPLE00 = 100,
  AMYSAMPLE99 = 100+99,

  EXTSAMPLE00 = 200,
  EXTSAMPLE99 = 200 + 99,
  EXWAVEMAX
};

int show_audio(void) {
  if (saudio_isvalid()) {
    printf("# audio backend is running\n");
    printf("# audio sample count %ld\n", sample_count);
    printf("# requested sample rate %d, actual sample rate %d\n",
      (int)MAIN_SAMPLE_RATE,
      saudio_sample_rate());
    printf("# buffer frames %d\n", saudio_buffer_frames());
    printf("# number of channels %d\n", saudio_channels());
  } else {
    printf("# did not start audio backend\n");
    return 1;
  }
  return 0;
}


#include "linenoise.h"

#include <math.h>
#include <pthread.h>
#include <time.h>

#define WT_FREE_LEN (8)
float *wt_free_list[WT_FREE_LEN]; // to keep from crashing the engine, have a place to store free-ed waves
int wt_free_ptr = 0;

float *wt_data[EXWAVEMAX];
int wt_size[EXWAVEMAX];
float wt_rate[EXWAVEMAX];
int wt_one_shot[EXWAVEMAX];
int wt_loop_enabled[EXWAVEMAX];
int wt_loop_start[EXWAVEMAX];
int wt_loop_end[EXWAVEMAX];
int wt_midinote[EXWAVEMAX];
float wt_offsethz[EXWAVEMAX];

void wt_free(void);

double freq[VOICE_MAX];
double note[VOICE_MAX];
float sample[VOICE_MAX];
float samples[VOICE_MAX][OSCOPE_LEN];
float hold[VOICE_MAX];
double amp[VOICE_MAX];
double panl[VOICE_MAX];
double panr[VOICE_MAX];
double pan[VOICE_MAX];
int interp[VOICE_MAX];
int use_adsr[VOICE_MAX];
int fmod_osc[VOICE_MAX];
int pmod_osc[VOICE_MAX];
int amod_osc[VOICE_MAX];
float fmod_depth[VOICE_MAX];
float pmod_depth[VOICE_MAX];
float amod_depth[VOICE_MAX];
int hide[VOICE_MAX];
int decimate[VOICE_MAX];
int decicount[VOICE_MAX];
float decihold[VOICE_MAX];
float decittl[VOICE_MAX];
int quantize[VOICE_MAX];
int direction[VOICE_MAX];
int phasereset[VOICE_MAX];

int wtsel[VOICE_MAX];

int cz[VOICE_MAX];
float czd[VOICE_MAX];

void reset_voice(int i);
void init_voice(void);

typedef struct {
  float phase;            // Current position in table
  float phase_inc;        // Phase increment per host sample
  float *table;           // Pointer to waveform or sample
  int table_size;         // Length of table
  int one_shot;
  int finished;
  int loop_enabled;
  float table_rate;       // Native sample rate of the table
  int loop_start;
  int loop_end;
  int midinote;
  float offsethz;
} osc_t;

osc_t osc[VOICE_MAX];

enum {
  OSCOPE_TRIGGER_BOTH,
  OSCOPE_TRIGGER_RISING,
  OSCOPE_TRIGGER_FALLING,
};

int oscope_trigger = OSCOPE_TRIGGER_RISING;

int oscope_running = 0;
int oscope_len = OSCOPE_LEN;
float oscope_bufferl[OSCOPE_LEN];
float oscope_bufferr[OSCOPE_LEN];
int oscope_buffer_pointer = 0;
float oscope_display_pointer = 0.0;
float oscope_display_inc = 1.0;
float oscope_display_mag = 1.0;
#define OWWIDTH (SCREENWIDTH/4)
#define OWHEIGHT (SCREENHEIGHT/2)
float oscope_wave[OWWIDTH];
int oscope_wave_index = 0;
float oscope_min[OWWIDTH];
float oscope_max[OWWIDTH];
int oscope_wave_len = 0;
int oscope_channel = -1; // -1 means all

float osc_get_phase_inc(int v, float freq) {
  float g = freq;
  if (osc[v].one_shot) g /= osc[v].offsethz;
  float phase_inc = (g * osc[v].table_size) / osc[v].table_rate * (osc[v].table_rate / MAIN_SAMPLE_RATE);
  return phase_inc;
}

void osc_set_freq(int v, float freq) {
  // Compute the frequency in "table samples per system sample"
  // This works even if table_rate ≠ system rate
  float g = freq;
  if (osc[v].one_shot) g /= osc[v].offsethz;
  osc[v].phase_inc = (g * osc[v].table_size) / osc[v].table_rate * (osc[v].table_rate / MAIN_SAMPLE_RATE);
}

float cz_phasor(int n, float p, float d, int table_size) {
  float phase = p / (float)table_size;
  if (d < 0) d = 0;
  if (d > 1) d = 1;
  switch (n) {
    case 1: // saw -> pulse
      if (phase < d) phase *= (0.5 / d);
      else phase = 0.5 + (phase - d) * (0.5 / (1.0 - d));
      break;
    case 2: // square (folded sine)
      if (phase < 0.5) phase *= (0.5 / (0.5 - d * 0.5));
      else phase = 1.0 - (1.0 - phase) * (0.5 / (0.5 - d * 0.5));
      break;
    case 3: // triangle
      if (phase < 0.5) phase *= (0.5 / (0.5 - d * 0.5));
      else phase = 0.5 + (phase - 0.5) * (0.5 / (0.5 - d * 0.5));
      break;
    case 4: // double sine
      phase = fmodf(phase * 2.0, 1.0);
      break;
    case 5: // saw -> triangle
      if (phase < 0.5) phase *= (0.5 / (0.5 - d * 0.5));
      else phase = 0.5 + (phase - 0.5) * (0.5 / (0.5 + d * 0.5));
      break;
    default:
      return p;
  }
  return fmodf(phase * (float)table_size, table_size);
  return phase * (float)table_size;
}

#if 1

float osc_next(int n, float phase_inc) {
    if (osc[n].finished) return 0.0f;

    if (direction[n]) phase_inc *= -1.0;

    // Step phase
    osc[n].phase += phase_inc;

    // NaN check
    if (!isfinite(osc[n].phase)) {
        osc[n].phase = 0.0f;
        osc[n].finished = osc[n].one_shot;
        return 0.0f;
    }

    // Clamp loop boundaries to valid range
    float loop_start = osc[n].loop_enabled ? osc[n].loop_start : 0.0f;
    float loop_end   = osc[n].loop_enabled ? osc[n].loop_end   : (float)osc[n].table_size;
    if (loop_end <= loop_start) { loop_start = 0.0f; loop_end = (float)osc[n].table_size; }

    // Handle forward playback
    if (phase_inc >= 0.0f) {
        // One-shot forward
        if (osc[n].one_shot && osc[n].phase >= loop_end) {
            osc[n].phase = loop_end - 1e-6f;
            osc[n].finished = 1;
        }
        // Loop forward
        else if (osc[n].loop_enabled && osc[n].phase >= loop_end) {
            osc[n].phase = loop_start + fmodf(osc[n].phase - loop_start, loop_end - loop_start);
        }
        // Wrap full table
        else if (!osc[n].loop_enabled && osc[n].phase >= osc[n].table_size) {
            osc[n].phase -= osc[n].table_size;
        }
    }
    // Handle backward playback
    else {
        // One-shot backward
        if (osc[n].one_shot && osc[n].phase < loop_start) {
            osc[n].phase = loop_start;
            osc[n].finished = 1;
        }
        // Loop backward
        else if (osc[n].loop_enabled && osc[n].phase < loop_start) {
            osc[n].phase = loop_end - fmodf(loop_start - osc[n].phase, loop_end - loop_start);
        }
        // Wrap full table
        else if (!osc[n].loop_enabled && osc[n].phase < 0.0f) {
            osc[n].phase += osc[n].table_size;
        }
    }

    float sample;
    if (interp[n]) {
      // Linear interpolation
      int idx;
      if (cz[n]) {
        idx = (int)cz_phasor(cz[n], osc[n].phase, czd[n], osc[n].table_size);
      } else {
        idx = (int)osc[n].phase;
      }
      //
      int next_idx;
      float frac;
      if (phase_inc >= 0.0f) {
          next_idx = (idx + 1) % osc[n].table_size;
          frac = osc[n].phase - (float)idx;
      } else {
          next_idx = (idx == 0) ? osc[n].table_size - 1 : idx - 1;
          frac = (float)idx - osc[n].phase;
      }
      sample = osc[n].table[idx] * (1.0f - frac) + osc[n].table[next_idx] * frac;
    } else {
      int idx;
      //
      if (cz[n]) {
        idx = (int)cz_phasor(cz[n], osc[n].phase, czd[n], osc[n].table_size);
      } else {
        idx = (int)osc[n].phase;
      }
      //
      if (idx >= osc->table_size) idx = osc->table_size - 1;
      if (idx < 0) idx = 0;
      sample = osc->table[idx];
    }

    return sample;
}



#endif

#if 0


float osc_next(int n, float phase_inc) {
    if (osc[n].finished) return 0.0f;
    if (direction[n]) phase_inc *= -1.0;

    // Step phase
    osc[n].phase += phase_inc;

    // Sanity check for NaN
    if (!isfinite(osc[n].phase)) {
        osc[n].phase = 0.0f;
        osc[n].finished = osc[n].one_shot;
        return 0.0f;
    }

    // Determine active boundaries
    float start = osc[n].loop_enabled ? osc[n].loop_start : 0.0f;
    float end   = osc[n].loop_enabled ? osc[n].loop_end   : (float)osc[n].table_size;

    // Prevent invalid boundaries
    if (end <= start) {
        start = 0.0f;
        end = (float)osc[n].table_size;
    }

    // Forward playback
    if (phase_inc >= 0.0f) {
        if (osc[n].phase >= end) {
            if (osc[n].one_shot) {
                osc[n].phase = end - 1e-6f; // clamp
                osc[n].finished = 1;
            } else if (osc[n].loop_enabled) {
                osc[n].phase = start + fmodf(osc[n].phase - start, end - start);
            } else {
                osc[n].phase -= osc[n].table_size; // wrap entire table
            }
        }
    }
    // Backward playback
    else {
        if (osc[n].phase < start) {
            if (osc[n].one_shot) {
                osc[n].phase = start; // clamp
                osc[n].finished = 1;
            } else if (osc[n].loop_enabled) {
                osc[n].phase = end - fmodf(start - osc[n].phase, end - start);
            } else {
                osc[n].phase += osc[n].table_size; // wrap entire table
            }
        }
    }

    // Linear interpolation
    int idx = (int)osc[n].phase;
    int next_idx;
    float frac;
    if (phase_inc >= 0.0f) {
        next_idx = (idx + 1) % osc[n].table_size;
        frac = osc[n].phase - (float)idx;
    } else {
        next_idx = (idx == 0) ? osc[n].table_size - 1 : idx - 1;
        frac = (float)idx - osc[n].phase;
    }

    float sample = osc[n].table[idx] * (1.0f - frac) + osc[n].table[next_idx] * frac;

    return sample;
}



#endif


#if 0

float osc_next(int n, float phase_inc) {
    if (osc[n].finished) return 0.0f;

    // Step phase
    osc[n].phase += phase_inc;

    // Determine active boundaries
    float start = osc[n].loop_enabled ? osc[n].loop_start : 0.0f;
    float end   = osc[n].loop_enabled ? osc[n].loop_end   : (float)osc[n].table_size;

    // Forward playback
    if (phase_inc >= 0.0f) {
        if (osc[n].phase >= end) {
            if (osc[n].one_shot) {
                osc[n].phase = end - 1e-6f; // clamp to end
                osc[n].finished = 1;
            } else if (osc[n].loop_enabled) {
                osc[n].phase = start + fmodf(osc[n].phase - start, end - start);
            } else {
                osc[n].phase -= osc[n].table_size; // wrap entire table
            }
        }
    } else {
    // Backward playback
        if (osc[n].phase < start) {
            if (osc[n].one_shot) {
                osc[n].phase = start;
                osc[n].finished = 1;
            } else if (osc[n].loop_enabled) {
                osc[n].phase = end - fmodf(start - osc[n].phase, end - start);
            } else {
                osc[n].phase += osc[n].table_size; // wrap entire table
            }
        }
    }

    float sample;
    int idx = (int)osc[n].phase;
    // Linear interpolation
      int next_idx;
      if (phase_inc >= 0.0f) {
          next_idx = (idx + 1) % osc[n].table_size;
      } else {
          next_idx = (idx == 0) ? osc[n].table_size - 1 : idx - 1;
      }
      float frac = osc[n].phase - (float)idx;
      sample = osc[n].table[idx] * (1.0f - frac) + osc[n].table[next_idx] * frac;
    return sample;
}


#endif

#if 0

#define OFT float
#define OFM fmodf

float osc_next(int n, float phase_inc) {
  int table_size = osc[n].table_size;
  OFT fphase = OFM(osc[n].phase, table_size);
  
  if (cz[n]) fphase = cz_phasor(cz[n], fphase, czd[n], table_size);
  
  int iphase = fphase;
  
  float sample = osc[n].table[iphase];
  
  if (direction[n]) osc[n].phase -= phase_inc;
  else osc[n].phase += phase_inc;
  
  if (osc[n].one_shot) {
    if (osc[n].loop_enabled) {
      if (direction[n]) {
        if (osc[n].phase < osc[n].loop_start) osc[n].phase = osc[n].loop_end;
      } else {
        if (osc[n].phase >= osc[n].loop_end) osc[n].phase = osc[n].loop_start;
      }
    } else {
      if (direction[n]) {
        if (osc[n].phase < 0) osc[n].finished = 1;
      } else {
        if (osc[n].phase >= table_size) osc[n].finished = 1;
      }
    }
  }
  // osc[n].phase = fmodf(osc[n].phase, table_size);
  return sample;
}
#endif

#if 0
float osc_next(int n, float phase_inc) {
  int table_size = osc[n].table_size;
  float phase = osc[n].phase;
  float sample;
    
  if (osc[n].one_shot) {
    if (direction[n] == 0 && phase > table_size) {
      osc[n].finished = 1;
      return 0;
    }
    if (direction[n] == 1 && phase < 0) {
      osc[n].finished = 1;
      return 0;
    }
  }

  if (cz[n] == 1) {
    float phase_norm = phase / (float)table_size;
    float d = czd[n];
    if (phase_norm < d) {
      phase_norm *= (0.5 / d);
    } else {
      phase_norm = 0.5 + (phase_norm - d) * (0.5 / (1.0 - d));
    }
    phase = phase_norm * (float)table_size;
  }
  osc[n].phase = phase;

  int i = (int)phase % table_size;
    
  if (i < 0) {
    osc[n].phase = table_size - 1;
    sample = osc[n].table[table_size - 1];
    return sample;
  } else if (i >= table_size) {
    if (osc[n].one_shot) {
      return 0;
    }
    osc[n].phase = 0;
    sample = osc[n].table[0];
    return sample;
  }
#if 1
  sample = osc[n].table[i];      
#else
  if (interp[n]) {
    int i_next = (i + 1) % table_size;
    float frac = 0.0;
    frac = osc[n].phase - i;
    sample = osc[n].table[i] + frac * (osc[n].table[i_next] - osc[n].table[i]);
  } else {
    sample = osc[n].table[i];      
  }
#endif

  if (direction[n]) {
    // backwards
    osc[n].phase -= phase_inc;
  } else {
    // forwards
    osc[n].phase += phase_inc;
  }
    
  if (osc[n].one_shot) {
    if (osc[n].loop_enabled) {
      if (direction[n]) {
        // backwards
        if (osc[n].phase <= osc[n].loop_start) {
          osc[n].phase = osc[n].loop_end;
        }
      } else {
        // forwards
        if (osc[n].phase >= osc[n].loop_end) {
          osc[n].phase = osc[n].loop_start;
        }
      }
    } else {
    }
  } else {
    if (osc[n].phase >= table_size) {
      osc[n].phase -= table_size;
    }
  }

  return sample;
}
#endif

void osc_set_wt(int voice, int n) {
  if (wt_data[n] && wt_size[n] && wt_rate[n] > 0.0) {
    wtsel[voice] = n;
    int update_freq = 0;
    if (wt_one_shot[n]) osc[voice].finished = 1;
    if (osc[voice].table_rate != wt_rate[n] || osc[voice].table_size != wt_size[n]) update_freq = 1;
    osc[voice].table_rate = wt_rate[n];
    osc[voice].table_size = wt_size[n];
    osc[voice].table = wt_data[n];
    osc[voice].one_shot = wt_one_shot[n];
    osc[voice].loop_start = wt_loop_start[n];
    osc[voice].loop_enabled = wt_loop_enabled[n];
    osc[voice].loop_end = wt_loop_end[n];
    osc[voice].midinote = wt_midinote[n];
    osc[voice].offsethz = wt_offsethz[n];
    osc[voice].phase = 0;
    if (update_freq) {
      osc_set_freq(voice, freq[voice]);
    }
  }
}

void osc_trigger(int voice) {
  if (1 || osc[voice].one_shot) {
    if (direction[voice]) {
      osc[voice].phase = osc[voice].table_size - 1;
    } else {
      osc[voice].phase = 0;
    }
    osc[voice].finished = 0;
  }
}

typedef enum {
  ADSR_OFF,
  ADSR_ATTACK,
  ADSR_DECAY,
  ADSR_SUSTAIN,
  ADSR_RELEASE
} adsr_state_t;

typedef struct {
  float attack_time;   // Attack duration in seconds
  float decay_time;    // Decay duration in seconds
  float sustain_level; // Sustain level (0.0 to 1.0)
  float release_time;  // Release duration in seconds
  float sample_rate;   // Samples per second (e.g., 44100.0)

  adsr_state_t state;  // Current state
  float value;         // Current envelope output (0.0 to 1.0)
  float time;          // Time elapsed in current stage (seconds)
  float save_sustain_level;
} adsr_t;

adsr_t amp_env[VOICE_MAX];

// Initialize the ADSR envelope
void adsr_init(int n, float attack_time, float decay_time, float sustain_level, float release_time, float sample_rate) {
  amp_env[n].attack_time = attack_time;
  amp_env[n].decay_time = decay_time;
  amp_env[n].sustain_level = sustain_level;
  amp_env[n].release_time = release_time;
  amp_env[n].sample_rate = sample_rate;
  amp_env[n].state = ADSR_OFF;
  amp_env[n].value = 0.0f;
  amp_env[n].time = 0.0f;
}

// Trigger the envelope (start attack)
void adsr_trigger(int n, float a) {
  //amp_env[n].save_sustain_level = amp[n];
  amp_env[n].sustain_level = a;
  amp_env[n].state = ADSR_ATTACK;
  amp_env[n].value = 0.0f;
  amp_env[n].time = 0.0f;
}

// Release the envelope (start release)
void adsr_release(int n) {
  //if (amp_env[n].state != ADSR_OFF) {
    amp_env[n].state = ADSR_RELEASE;
    amp_env[n].time = 0.0f;
  //}
}


// Compute one sample of the envelope
float adsr_step(int n) {
  if (amp_env[n].state == ADSR_OFF) {
    //amp[n] = amp_env[n].save_sustain_level;
    return 0.0f;
  }

  // Increment time (in seconds)
  amp_env[n].time += 1.0f / amp_env[n].sample_rate;
  float t, target;

  switch (amp_env[n].state) {
    case ADSR_ATTACK:
      if (amp_env[n].attack_time <= 0.0f) {
        amp_env[n].value = 1.0f;
        amp_env[n].state = ADSR_DECAY;
        amp_env[n].time = 0.0f;
      } else {
        t = amp_env[n].time / amp_env[n].attack_time;
        if (t >= 1.0f) {
          amp_env[n].value = 1.0f;
          amp_env[n].state = ADSR_DECAY;
          amp_env[n].time = 0.0f;
        } else {
          amp_env[n].value = t; // Linear ramp from 0 to 1
        }
      }
      break;

      case ADSR_DECAY:
        if (amp_env[n].decay_time <= 0.0f) {
          amp_env[n].value = amp_env[n].sustain_level;
          amp_env[n].state = ADSR_SUSTAIN;
          amp_env[n].time = 0.0f;
        } else {
          t = amp_env[n].time / amp_env[n].decay_time;
          if (t >= 1.0f) {
            amp_env[n].value = amp_env[n].sustain_level;
            amp_env[n].state = ADSR_SUSTAIN;
            amp_env[n].time = 0.0f;
          } else {
            // Linear ramp from 1 to sustain_level
            amp_env[n].value = 1.0f - t * (1.0f - amp_env[n].sustain_level);
          }
        }
        break;

      case ADSR_SUSTAIN:
        amp_env[n].value = amp_env[n].sustain_level;
        break;

      case ADSR_RELEASE:
        if (amp_env[n].release_time <= 0.0f) {
          amp_env[n].value = 0.0f;
          amp_env[n].state = ADSR_OFF;
          amp_env[n].time = 0.0f;
        } else {
          t = amp_env[n].time / amp_env[n].release_time;
          if (t >= 1.0f) {
            amp_env[n].value = 0.0f;
            amp_env[n].state = ADSR_OFF;
            amp_env[n].time = 0.0f;
          } else {
            // Linear ramp from current value to 0
            amp_env[n].value = amp_env[n].sustain_level * (1.0f - t);
          }
        }
        break;

      default:
        amp_env[n].value = 0.0f;
        amp_env[n].state = ADSR_OFF;
        amp[n] = amp_env[n].save_sustain_level;
  }

  // Clamp value to [0.0, 1.0]
  if (amp_env[n].value < 0.0f) amp_env[n].value = 0.0f;
  if (amp_env[n].value > 1.0f) amp_env[n].value = 1.0f;

  return amp_env[n].value;
}

float quantize_bits_int(float v, int bits) {
  int levels = (1 << bits) - 1;
  int iv = (int)(v * levels + 0.5);
  return iv * (1.0 / levels);
}

int main_running = 1;
int udp_running = 1;

void show_stats(void) {
  for (int i = 0; i < OWWIDTH; i++) {
    float avg = (oscope_wave[i] / 2.0 + 0.5) * (float)OWHEIGHT/2.0;
    float min = (oscope_min[i] / 2.0 + 0.5) * (float)OWHEIGHT/2.0;
    float max = (oscope_max[i] / 2.0 + 0.5) * (float)OWHEIGHT/2.0;
    printf("# [%d] min:%g avg:%g max:%g\n", i, min, avg, max);
  }
}

float get_oscope_buffer(int *index) {
  if (index == NULL) return 0;
  int i = *index;
  if (i < 0) {
    i = OSCOPE_LEN - 1;
  } else if (i >= OSCOPE_LEN) {
    i = 0;
  }
  float a = (oscope_bufferl[i] + oscope_bufferr[i]) / 2.0;
  *index = i;
  return a;
}

void engine(float *buffer, int num_frames, int num_channels, void *user_data) { // , void *user_data) {
  for (int i = 0; i < num_frames; i++) {
    if ((sample_count % tick_mod) == 0) {
      motor_trigger();
      tick_engine_count++;
    }
    sample_count++;
    float samplel = 0;
    float sampler = 0;
    float f = 0;
    for (int n = 0; n < VOICE_MAX; n++) {
      if (amp[n] == 0) continue;
      //if (osc[n].one_shot && osc[n].finished) continue;
      if (osc[n].finished) continue;
      if (fmod_osc[n] >= 0) {
        int m = fmod_osc[n];
        float g = sample[m] * fmod_depth[n];
        float h = osc_get_phase_inc(n, freq[n] + g);
        f = osc_next(n, h);
      } else {
        f = osc_next(n, osc[n].phase_inc);
      }
      if (decimate[n]) {
        decittl[n] += f;
        sample[n] = decihold[n];
        decicount[n]++;
        if (decicount[n] >= decimate[n]) {
          decicount[n] = 0;
          decihold[n] = decittl[n] / (float)decimate[n];
          decittl[n] = 0;
        }
      } else {
        decihold[n] = f;
        sample[n] = f;
      }
      if (quantize[n]) {
        sample[n] = quantize_bits_int(sample[n], quantize[n]);
      }

      // apply amp to sample
      if (use_adsr[n]) {
        float env = adsr_step(n);
        //sample[n] *= env; // * AFACTOR;
        sample[n] *= (env * amp[n]);
      } else {
        sample[n] *= amp[n];
      }
      if (amod_osc[n] >= 0) {
        int m = amod_osc[n];
        float g = sample[m] * amod_depth[n];
        sample[n] *= g;
      }
      // accumulate samples
      if (hide[n] == 0) {
        if (pmod_osc[n] >= 0) {
          float q = sample[pmod_osc[n]] * pmod_depth[n];
          panl[n] = (1.0 - q) / 2.0;
          panr[n] = (1.0 + q) / 2.0;          
        }
        float left = sample[n] * panl[n];
        float right = sample[n] * panr[n];
        samplel += left;
        sampler += right;
        if (oscope_channel == n) {
          oscope_bufferl[oscope_buffer_pointer] = left;
          oscope_bufferr[oscope_buffer_pointer] = right;  
        }
      }
    }
    // Write to all channels
    buffer[i * num_channels + 0] = samplel;
    buffer[i * num_channels + 1] = sampler;
    if (oscope_channel < 0) {
      oscope_bufferl[oscope_buffer_pointer] = samplel;
      oscope_bufferr[oscope_buffer_pointer] = sampler;      
    }
    oscope_buffer_pointer++;
    if (oscope_buffer_pointer >= oscope_len) oscope_buffer_pointer = 0;
  }
}

void fsleep(double seconds) {
  if (seconds < 0.0f) return; // Invalid input
  struct timespec ts;
  ts.tv_sec = (time_t)seconds; // Whole seconds
  ts.tv_nsec = (long)((seconds - (double)ts.tv_sec) * 1e9); // Fractional part to nanoseconds
  nanosleep(&ts, NULL);
}

void init_wt(void);

void *udp(void *arg);
void *oscope(void *arg);

int current_voice = 0;

#include <dirent.h>

void show_threads(void) {
  DIR* dir;
  struct dirent* entry;
  dir = opendir("/proc/self/task");
  if (dir == NULL) {
    perror("# failed to open /proc/self/task");
    return;
  }

  // Iterate through each thread directory
  while ((entry = readdir(dir)) != NULL) {
    // Skip . and .. directories
    if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
      continue;
    }
    char path[4096], name[4096];
    name[0] = '\0';
    snprintf(path, sizeof(path), "/proc/self/task/%s/comm", entry->d_name);
    FILE* f = fopen(path, "r");
    if (f) {
      if (fgets(name, sizeof(name), f)) {
        int n = strlen(name);
        if (name[n-1] == '\r' || name[n-1] == '\n') {
          name[n-1] = '\0';
        }
      }
      fclose(f);
    }
    printf("# %s %s\n", entry->d_name, name);
  }

  closedir(dir);
}

enum {
  ERR_EXPECTED_INT,
  ERR_EXPECTED_FLOAT,
  ERR_INVALID_VOICE,
  ERR_FREQUENCY_OUT_OF_RANGE,
  ERR_AMPLITUDE_OUT_OF_RANGE,
  ERR_INVALID_WAVE,
  ERR_EMPTY_WAVE,
  ERR_INVALID_INTERPOLATE,
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
  ERR_INVALID_EXTSAMPLE,
  ERR_PARSING_ERROR,
  ERR_INVALID_PATCH,
  ERR_INVALID_MIDI_NOTE,
  //
  ERR_INVALID_AMP,
  ERR_INVALID_PAN,
  ERR_INVALID_QUANT,
  ERR_INVALID_DECI,
  ERR_INVALID_FREQ,
  ERR_INVALID_WAVETABLE,
};

void voice_show(int v, char c) {
  printf("# v%d w%d b%d B%d n%g f%g a%g p%g I%d d%d q%d c%d,%g",
    v,
    wtsel[v],
    direction[v],
    osc[v].loop_enabled,
    note[v],
    freq[v],
    amp[v] / AFACTOR,
    pan[v],
    interp[v],
    decimate[v],
    quantize[v],
    cz[v], czd[v]);
  printf(" A%d,%g", amod_osc[v], amod_depth[v]);
  printf(" F%d,%g", fmod_osc[v], fmod_depth[v]);
  printf(" P%d,%g", pmod_osc[v], pmod_depth[v]);
  printf(" m%d E%g,%g,%g,%g",
    hide[v],
    amp_env[v].attack_time,
    amp_env[v].decay_time,
    amp_env[v].sustain_level,
    amp_env[v].release_time);
  printf(" # %g/%g", osc[v].phase, osc[v].phase_inc);
  if (osc[v].one_shot) {
    printf(" %d/%g", osc[v].midinote, osc[v].offsethz);
  }
  if (c != ' ') {
    printf(" *");
  }
  puts("");
}

int voice_show_all(int voice) {
  for (int i=0; i<VOICE_MAX; i++) {
    if (amp[i] == 0) continue;
    int t = ' ';
    if (i == voice) t = '*';
    voice_show(i, t);
  }
}

float midi2hz(int f);

pthread_t udp_thread;
pthread_t oscope_thread;

void downsample_block_average_min_max(
  float *source, int source_len, float *dest, int dest_len,
  float *min, float *max) {
  if (dest_len >= source_len) {
    // If dest is same size or larger, just copy
    for (int i = 0; i < dest_len && i < source_len; i++) {
      //puts("####100####\n");
      dest[i] = source[i];
      if (min) min[i] = source[i];
      if (max) max[i] = source[i];
    }
    return;
  }
    
  float block_size = (float)source_len / dest_len;
    
  for (int i = 0; i < dest_len; i++) {
    float start = i * block_size;
    float end = (i + 1) * block_size;
        
    int start_idx = (int)start;
    int end_idx = (int)end;
    if (end_idx >= source_len) end_idx = source_len - 1;
        
    float sum = 0.0;
    int count = 0;
        
    float thismin = source[start_idx];
    float thismax = thismin;
    // Average all values in this block
    for (int j = start_idx; j <= end_idx; j++) {
      sum += source[j];
      count++;
      if (source[j] < thismin) thismin = source[j];
      if (source[j] > thismin) thismax = source[j];
    }
    if (min) min[i] = thismin;
    if (max) max[i] = thismax;
    dest[i] = (count > 0) ? sum / count : 0.0;
  }
}

void downsample_block_average(float *source, int source_len, float *dest, int dest_len) {
  downsample_block_average_min_max(source, source_len, dest, dest_len, NULL, NULL);
}

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
  FUNC_AMOD,
  FUNC_FMOD,
  FUNC_PMOD,
  FUNC_MIDI,
  FUNC_WAVE,
  FUNC_LOOP,
  FUNC_DIR,
  FUNC_INTER,
  FUNC_PAN,
  FUNC_ADSR,
  FUNC_DECI,
  FUNC_QUANT,
  FUNC_RESET,
  // subfunctions
  FUNC_HELP,
  FUNC_SEQSTART,
  FUNC_SEQSTOP,
  FUNC_SEQPAUSE,
  FUNC_SEQRESUME,
  FUNC_QUIT,
  FUNC_STATS0,
  FUNC_STATS1,
  FUNC_TRACE,
  FUNC_DEBUG,
  FUNC_OSCOPE,
  FUNC_LOAD,
  FUNC_SAVE,
  FUNC_WAVEREAD,
  //
  FUNC_SHOWWAVE,
  FUNC_DELAY,
  FUNC_COMMENT,
  FUNC_WHITESPACE,
  FUNC_METRO,
  FUNC_WAVE_DEFAULT,
  FUNC_CZ,
  //
  FUNC_UNKNOWN,
};

char *_func_func_str[FUNC_UNKNOWN+1] = {
  [FUNC_NULL] = "-?-",
  [FUNC_ERR] = "err",
  [FUNC_SYS] = "sys",
  [FUNC_IMM] = "imm",
  [FUNC_VOICE] = "voice",
  [FUNC_FREQ] = "freq",
  [FUNC_AMP] = "amp",
  [FUNC_TRIGGER] = "trigger",
  [FUNC_VELOCITY] = "velocity",
  [FUNC_MUTE] = "mute",
  [FUNC_AMOD] = "amod",
  [FUNC_FMOD] = "fmod",
  [FUNC_PMOD] = "pmod",
  [FUNC_MIDI] = "midi",
  [FUNC_WAVE] = "wave",
  [FUNC_LOOP] = "loop",
  [FUNC_DIR] = "dir",
  [FUNC_INTER] = "inter",
  [FUNC_PAN] = "pan",
  [FUNC_ADSR] = "adsr",
  [FUNC_DECI] = "deci",
  [FUNC_QUANT] = "quant",
  [FUNC_RESET] = "reset",
  [FUNC_UNKNOWN] = "unknown",
  //
  [FUNC_HELP] = "help",
  [FUNC_SEQSTART] = "seq-start",
  [FUNC_SEQSTOP] = "seq-stop",
  [FUNC_SEQPAUSE] = "seq-pause",
  [FUNC_SEQRESUME] = "seq-resume",
  [FUNC_QUIT] = "quit",
  [FUNC_STATS0] = "stats-0",
  [FUNC_STATS1] = "stats-1",
  [FUNC_TRACE] = "trace",
  [FUNC_TRIGGER] = "trigger",
  [FUNC_DEBUG] = "debug",
  [FUNC_OSCOPE] = "oscope",
  [FUNC_LOAD] = "load",
  [FUNC_SAVE] = "save",
  [FUNC_SHOWWAVE] = "show-wave",
  [FUNC_DELAY] = "delay",
  [FUNC_COMMENT] = "comment",
  [FUNC_WHITESPACE] = "white-space",
  [FUNC_METRO] = "metro",
  [FUNC_WAVEREAD] = "wave-read",
  [FUNC_CZ] = "cz",
};

char *func_func_str(int n) {
  if (n >= 0 && n <= FUNC_UNKNOWN) {
    if (_func_func_str[n]) {
      return _func_func_str[n];
    }
  }
  return "no-string";
}

#include "miniwav.h"

void update_oscope_wave(float *table, int size) {
  oscope_wave_len = 0;
  //float *table = osc[voice].table;
  //int size = osc[voice].table_size;
  downsample_block_average_min_max(table, size, oscope_wave, OWWIDTH, oscope_min, oscope_max);
  oscope_wave_len = OWWIDTH;
}

int freq_set(int v, float f);
int freq_midi(int voice, float f);
int amp_set(int v, float f);
int wave_set(int voice, int n);
int wave_reset(int voice, int n);
int wave_default(int voice);
int wave_loop(int voice, int state);
int wave_mute(int voice, int state);
int wave_dir(int voice, int state);
int wave_load(int which, int where);
int wave_interp(int voice, int state);
int wave_deci(int voice, int state);
int wave_quant(int voice, int n);
int voice_set(int voice, int *old_voice);
int voice_trigger(int voice);
int voice_show_all(int voice);
int oscope_start(int sub);
int amod_set(int voice, int osc, float f);
int fmod_set(int voice, int osc, float f);
int pmod_set(int voice, int osc, float f);
int patch_load(int voice, int n, int output);
int pan_set(int voice, float f);
int adsr_set(int voice, float a, float d, float s, float r);
int adsr_velocity(int voice, float f);
int wavetable_show(int n);
int cz_set(int v, int m, float d);

char *ignore = " \t\r\n;";

typedef struct {
  int func;
  int subfunc;
  int next;
  int argc;
  float args[8];
} value_t;

void dump(value_t v) {
  printf("# %s", func_func_str(v.func));
  if (v.subfunc != FUNC_NULL) printf(" %s", func_func_str(v.subfunc));
  printf(" [");
  for (int i=0; i<v.argc; i++) {
    if (i) printf(" ");
    printf("%g", v.args[i]);
  }
  puts("]");
}

void float_to_timespec(double seconds, int64_t *sec, int64_t *nsec) {
    double intpart;
    double frac = modf(seconds, &intpart);

    *sec  = (int64_t)intpart;
    *nsec = (int64_t)llround(frac * 1e9);

    // Normalize in case rounding pushed us to 1 second
    if (*nsec >= 1000000000) {
        *sec += 1;
        *nsec -= 1000000000;
    }
    if (*nsec < 0) {
        *sec -= 1;
        *nsec += 1000000000;
    }
}

void ms_to_timespec(int64_t ms, int64_t *sec, int64_t *ns) {
  if (sec == NULL || ns == NULL) return;
  *sec = ms / 1000;
  *ns = (ms % 1000) * 1000000L;
}

value_t parse_none(int func, int subfunc) {
  value_t v;
  v.func = func;
  v.subfunc = subfunc;
  v.argc = 0;
  v.next = 0;
  if (trace) dump(v);
}

value_t parse(char *ptr, int func, int subfunc, int argc) {
  value_t v;
  v.func = func;
  v.subfunc = subfunc;
  int next[8];
  int limit = argc;
    switch (argc) {
      case 1:
        v.argc = sscanf(ptr, "%g%n", &v.args[0], &next[0]);
        if (v.argc == 1) v.next = next[0];
        break;
      case 2:
        v.argc = sscanf(ptr, "%g%n,%g%n", &v.args[0], &next[0], &v.args[1], &next[1]);
        if (v.argc > 0) v.next = next[v.argc-1];
      case 4:
        v.argc = sscanf(ptr, "%g%n,%g%n,%g%n,%g%n",
          &v.args[0], &next[0],
          &v.args[1], &next[1],
          &v.args[2], &next[2],
          &v.args[3], &next[3]);
        if (v.argc > 0) v.next = next[v.argc-1];
        break;
      default:
        v.argc = 0;
        v.next = 0;
        break;
    }
  if (debug) {
    printf("# argc:%d next:%d", v.argc, v.next);
    puts("");
  }
  if (trace) dump(v);

  return v;
}

int wire(char *line, int *this_voice, int output) {
  int len = strlen(line);
  if (len == 0) return 0;

  char *ptr = line;
  char *max = line + len;
  
  int func;
  int subfunc;
  
  value_t v;
  int voice = 0;
  if (this_voice) voice = *this_voice;
  
  int more = 1;
  int status = 0;
  
  int n;
  int r = 0;

  char c;
  
  while (more) {
    if (ptr >= max) break;
    if (*ptr == '\0') break;
    // skip whitespace and semi-colons
    ptr += strspn(ptr, ignore);
    if (debug) printf("# [%ld] '%c' (%d)\n", ptr-line, *ptr, *ptr);
    r = 0;
    switch (*ptr++) {
      case '#':
        return 0;
      case '\0':
        if (output) puts("# NULL!");
        break;
      case ':':
        //puts("# COLON");
        switch (*ptr++) {
          case '\0': return 100;
          case 'q': return -1;
          case 't':
            v = parse_none(FUNC_SYS, FUNC_TRACE);
            c = *ptr;
            if (c == '0' || c == '1') {
              trace = c - '0';
              ptr++;
            } else {
              if (trace) trace = 0; else trace = 1;
            }
            break;
          case 'S':
            v = parse_none(FUNC_SYS, FUNC_STATS0);
            if (output) show_stats();
            break;
          case 's':
            v = parse_none(FUNC_SYS, FUNC_STATS1);
            if (output) {
              show_threads();
              show_audio();
            }
            break;
          case 'd':
            v = parse_none(FUNC_SYS, FUNC_DEBUG);
            c = *ptr;
            if (c == '0' || c == '1') {
              debug = c - '0';
              ptr++;
            } else {
              if (debug) debug = 0; else debug = 1;
            }
            break;
          case 'o':
            v = parse_none(FUNC_SYS, FUNC_OSCOPE);
            oscope_start(*ptr);
            // sub x for oscope_cross = 1
            // sub q for oscope_quit = 0
            // sub 0..VOICE_MAX-1 for oscope_channel = n
            // sub -1 for oscope_channel = -1 (all channels)
            break;
          case 'l':
            // :l# load exp#.patch
            v = parse(ptr, FUNC_SYS, FUNC_LOAD, 1);
            {
              int which;
              if (v.argc == 1) {
                ptr += v.next;
                which = v.args[0];
              } else return ERR_INVALID_PATCH;
              r = patch_load(voice, which, output);
              //if (r != 0) return r;
            }
            break;
          case 'w':
            // :w#,# load wave#.wav into wave slot #
            v = parse(ptr, FUNC_SYS, FUNC_WAVEREAD, 2);
            {
              int which;
              int where;
              if (v.argc == 2) {
                ptr += v.next;
                which = v.args[0];
                where = v.args[1];
              } else if (v.argc == 1) {
                ptr += v.next;
                which = v.args[0];
                where = EXTSAMPLE00;
              } else return ERR_INVALID_EXTSAMPLE;
              r = wave_load(which, where);
            }
            break;
          default: return 999;
        }
        break;
      case '~':
        v = parse(ptr, FUNC_DELAY, FUNC_NULL, 1);
        if (v.argc == 1) {
          ptr += v.next;
          if (v.args[0] >= 0 && v.args[0] <= 15) fsleep(v.args[0]);
        }
        break;
      case 'c':
        v = parse(ptr, FUNC_CZ, FUNC_NULL, 2);
        if (v.argc == 2) {
          ptr += v.next;
          r = cz_set(voice, v.args[0], v.args[1]);
        }
        break;
      case '?':
        v = parse_none(FUNC_HELP, FUNC_NULL);
        if (*ptr == '?') {
          voice_show_all(voice);
          ptr++;
        } else {
          voice_show(voice, '*');
        }
        break;
      case 'a':
        v = parse(ptr, FUNC_AMP, FUNC_NULL, 1);
        if (v.argc == 1) {
          ptr += v.next;
          r = amp_set(voice, v.args[0]);
        } else return ERR_INVALID_AMP;
        break;
      case 'p':
        v = parse(ptr, FUNC_PAN, FUNC_NULL, 1);
        if (v.argc == 1) {
          ptr += v.next;
          r = pan_set(voice, v.args[0]);
        } else return ERR_INVALID_PAN;
        break;
      case 'q':
        v = parse(ptr, FUNC_QUANT, FUNC_NULL, 1);
        if (v.argc == 1) {
          ptr += v.next;
          r = wave_quant(voice, v.args[0]);
        } else return ERR_INVALID_QUANT;
        break;
      case 'd':
        v = parse(ptr, FUNC_DECI, FUNC_NULL, 1);
        if (v.argc == 1) {
          ptr += v.next;
          r = wave_deci(voice, v.args[0]);
        } else return ERR_INVALID_DECI;
        break;
      case 'f':
        v = parse(ptr, FUNC_FREQ, FUNC_NULL, 1);
        if (v.argc == 1) {
          ptr += v.next;
          r = freq_set(voice, v.args[0]);
        } else return ERR_INVALID_FREQ;
        break;
      case 'v':
        v = parse(ptr, FUNC_VOICE, FUNC_NULL, 1);
        if (v.argc == 1) {
          ptr += v.next;
          r = voice_set(v.args[0], &voice);
        } else return ERR_INVALID_VOICE;
        break;
      case 'w':
        v = parse(ptr, FUNC_WAVE, FUNC_NULL, 1);
        if (v.argc == 1) {
          ptr += v.next;
          r = wave_set(voice, v.args[0]);
        } else return ERR_INVALID_WAVE;
        break;
      case 'T':
        v = parse_none(FUNC_TRIGGER, FUNC_NULL);
        voice_trigger(voice);
        break;
      case '+':
        v = parse_none(FUNC_WAVE_DEFAULT, FUNC_NULL);
        wave_default(voice);
        break;
      case 'B':
        v = parse_none(FUNC_LOOP, FUNC_NULL);
        c = *ptr;
        if (c == '0' || c == '1') {
          wave_loop(voice, c == '1');
          ptr++;
        } else wave_loop(voice, -1);
        break;
      case 'W':
        // show wavetable 0..WTMAX
        // wavetable_show(n);
        v = parse(ptr, FUNC_SHOWWAVE, FUNC_NULL, 1);
        if (v.argc == 1) {
          ptr += v.next;
          r = wavetable_show(v.args[0]);
        } else return ERR_INVALID_WAVETABLE;
        break;
      case 'M':
        v = parse(ptr, FUNC_METRO, FUNC_NULL, 1);
        if (v.argc == 1) {
          ptr += v.next;
          tick_mod_set(v.args[0]);
        }
        break;
      case 'm':
        v = parse_none(FUNC_MUTE, FUNC_NULL);
        c = *ptr;
        if (c == '0' || c == '1') {
          wave_mute(voice, c == '1');
          ptr++;
        } else wave_mute(voice, -1);
        break;
      case 'b':
        v = parse_none(FUNC_DIR, FUNC_NULL);
        c = *ptr;
        if (c == '0' || c == '1') {
          wave_dir(voice, c == '1');
          ptr++;
        } else wave_dir(voice, -1);
        break;
      case 'I':
        v = parse_none(FUNC_INTER, FUNC_NULL);
        c = *ptr;
        if (c == '0' || c == '1') {
          wave_interp(voice, c == '1');
          ptr++;
        } else wave_interp(voice, -1);
        break;
      case 'n':
        v = parse(ptr, FUNC_MIDI, FUNC_NULL, 1);
        if (v.argc == 1) {
          ptr += v.next;
          r = freq_midi(voice, v.args[0]);
        } else return ERR_INVALID_MIDI_NOTE;
        break;
      case 'A':
        v = parse(ptr, FUNC_AMOD, FUNC_NULL, 2);
        if (v.argc == 2) {
          ptr += v.next;
          r = amod_set(voice, v.args[0], v.args[1]);
        } else if (v.argc == 1) {
          ptr += v.next;
          r = amod_set(voice, -1, 0);
        } else return ERR_PARSING_ERROR;
        break;
      case 'l':
        v = parse(ptr, FUNC_VELOCITY, FUNC_NULL, 1);
        if (v.argc == 1) {
          ptr += v.next;
          r = adsr_velocity(voice, v.args[0]);
        } else return ERR_PARSING_ERROR;
        break;
      case 'E':
        v = parse(ptr, FUNC_ADSR, FUNC_NULL, 4);
        if (v.argc == 4) {
          ptr += v.next;
          r = adsr_set(voice, v.args[0], v.args[1], v.args[2], v.args[3]);
        } else return ERR_PARSING_ERROR;
        break;
      case 'S':
        v = parse(ptr, FUNC_RESET, FUNC_NULL, 1);
        if (v.argc == 1) {
          ptr += v.next;
          r = wave_reset(voice, v.args[0]);
        } else return ERR_PARSING_ERROR;
        break;
      case 'F':
        v = parse(ptr, FUNC_FMOD, FUNC_NULL, 2);
        if (v.argc == 2) {
          ptr += v.next;
          r = fmod_set(voice, v.args[0], v.args[1]);
        } else if (v.argc == 1) {
          ptr += v.next;
          r = fmod_set(voice, -1, 0);
        } else return ERR_PARSING_ERROR;
        break;
      case 'P':
        v = parse(ptr, FUNC_PMOD, FUNC_NULL, 2);
        if (v.argc == 2) {
          ptr += v.next;
          r = pmod_set(voice, v.args[0], v.args[1]);
        } else if (v.argc == 1) {
          ptr += v.next;
          r = pmod_set(voice, -1, 0);
        } else return ERR_PARSING_ERROR;
        break;
      //
      default:
        if (output) printf("# not sure\n");
        return ERR_UNKNOWN_FUNC;
        more = 0;
        break;
    }
    if (r != 0) break;
    if (ptr >= max) break;
    if (*ptr == '\0') break;
  }
  if (this_voice) *this_voice = voice;
  return r;
}

char my_data[] = "hello";

void tick_mod_set(uint64_t m) {
  tick_mod = m;
}

void tick() {
#if 0
  static uint64_t last = 0;
  static uint64_t diff = 0;
  diff = sample_count - last;
  last = sample_count;
  if (diff >= tick_mod) {
  }
  if (tick_self_count & 1) voice_trigger(20);
  //if (tick_engine_count & 1) voice_trigger(20);
  else voice_trigger(21);
  tick_self_count++;
#else
  int voice = 20;
  wire("v20T", &voice, 0);
#endif
}

int main(int argc, char *argv[]) {
  if (argc > 1) {
    for (int i=1; i<argc; i++) {
      if (argv[i][0] == '-') {
        switch (argv[i][1]) {
          case 'd': debug = 1; break;
        }
      }
    }
  }
  show_threads();
  linenoiseHistoryLoad(HISTORY_FILE);
  init_wt();
  init_voice();
  
  // Initialize Sokol Audio
  saudio_setup(&(saudio_desc){
    // .stream_cb = stream_cb,
    .stream_userdata_cb = engine,
    .user_data = &my_data,
    .sample_rate = MAIN_SAMPLE_RATE,
    .num_channels = CHANNEL_NUM, // Stereo
    // .user_data = &my_data, // todo
  });

  if (show_audio() != 0) return 1;

  pthread_setname_np(pthread_self(), "skred-main");

  pthread_create(&udp_thread, NULL, udp, NULL);
  pthread_detach(udp_thread);

  int ms = 250;
  motor_init(ms, tick);
  
  while (main_running) {
    char *line = linenoise("# ");
    if (line == NULL) {
      main_running = 0;
      break;
    }
    if (strlen(line) == 0) continue;
    linenoiseHistoryAdd(line);
    int n = wire(line, &current_voice, 1);
    if (n < 0) break; // request to stop or error
    if (n > 0) {
      char *s = "\0";
      switch (n) {
        case ERR_EXPECTED_INT: s = "expected int"; break;
        case ERR_EXPECTED_FLOAT: s = "expected float"; break;
        case ERR_INVALID_VOICE: s = "invalid voice"; break;
        case ERR_FREQUENCY_OUT_OF_RANGE: s = "frequency out of range"; break;
        case ERR_AMPLITUDE_OUT_OF_RANGE: s = "amplitude out of range"; break;
        case ERR_INVALID_WAVE: s = "invalid wave"; break;
        case ERR_EMPTY_WAVE: s = "empty wave"; break;
        case ERR_INVALID_INTERPOLATE: s = "invalid interpolate type"; break;
        case ERR_INVALID_DIRECTION: s = "invalid wave direction"; break;
        case ERR_INVALID_LOOPING: s = "invalid wave looping flag"; break;
        case ERR_PAN_OUT_OF_RANGE: s = "pan out of range"; break;
        case ERR_INVALID_DELAY: s = "invalid delay"; break;
        case ERR_INVALID_MODULATOR: s = "invalid modulator"; break;
        case ERR_UNKNOWN_FUNC: s = "unknown func"; break;
        case ERR_UNKNOWN_SYS: s = "unknown sys"; break;
        case ERR_INVALID_TRACE: s = "invalid trace"; break;
        case ERR_INVALID_DEBUG: s = "invalid debug"; break;
        case ERR_INVALID_MUTE: s = "invalid mute"; break;
        case ERR_INVALID_EXTSAMPLE: s = "invalid external sample"; break;
        case ERR_PARSING_ERROR: s = "parsing error"; break;
        case ERR_INVALID_PATCH: s = "invalid patch"; break;
        case ERR_INVALID_MIDI_NOTE: s = "invalid midi note"; break;
        //
        case ERR_INVALID_AMP: s = "invalid amp"; break;
        case ERR_INVALID_PAN: s = "invalid pan"; break;
        case ERR_INVALID_QUANT: s = "invalid quant"; break;
        case ERR_INVALID_DECI: s = "invalid deci"; break;
        case ERR_INVALID_FREQ: s = "invalid freq"; break;
        case ERR_INVALID_WAVETABLE: s = "invalid wave table"; break;
        default: s = "unknown return"; break;
      }
      printf("# %s ERR:%d\n", s, n);
    }
    linenoiseFree(line);
  }
  linenoiseHistorySave(HISTORY_FILE);

  // Cleanup
  saudio_shutdown();
  
  udp_running = 0;
  motor_fini();
  oscope_running = 0;

  sleep(1); // make sure we don't crash the callback b/c thread timing and wt_data
  
  wt_free();
  
  show_threads();
  
  return 0;
}

#define SIZE_SINE (4096)
#define SIZE_SQR (4096)
#define SIZE_SAWDN (4096)
#define SIZE_SAWUP (4096)
#define SIZE_TRI (4096)
#include "retro/korg.h"

#include "amysamples.h"

void generate_moog_squarewave(int sample_count, float *waveform) {
    if (!waveform) return;

    // Square wave parameters
    float period = sample_count; // One cycle over all samples
    float half_period = period / 2.0f;

    // Ringing parameters
    float ring_amplitude = 0.3f; // Ringing amplitude (adjustable, < 1 to stay in bounds)
    //float ring_freq = 20.0f * 2.0f * M_PI / sample_count; // Ringing frequency (fast oscillations)
    float ring_freq = 40.0f * 2.0f * M_PI / sample_count; // Ringing frequency (fast oscillations)
    //float decay_rate = 5.0f / sample_count; // Decay rate for ringing
    float decay_rate = 40.0f / sample_count; // Decay rate for ringing

    // Generate waveform
    for (int i = 0; i < sample_count; i++) {
        float t = (float)i;
        // Base square wave: +1 for first half, -1 for second half
        float square = (t < half_period) ? 1.0f : -1.0f;

        // Add ringing at transitions (at t=0 and t=half_period)
        float ringing = 0.0f;
        if (t < half_period) {
            // Ringing at t=0 (start of +1)
            ringing = ring_amplitude * sin(ring_freq * t) * exp(-decay_rate * t);
        } else {
            // Ringing at t=half_period (start of -1)
            ringing = ring_amplitude * sin(ring_freq * (t - half_period)) * 
                     exp(-decay_rate * (t - half_period));
        }

        // Combine square wave and ringing
        waveform[i] = square + ringing;

        // Normalize to [-1, 1] (clip if necessary)
        if (waveform[i] > 1.0f) waveform[i] = 1.0f;
        if (waveform[i] < -1.0f) waveform[i] = -1.0f;
    }
}

void normalize_preserve_zero(float *data, int length) {
  if (length == 0) return;
    
  // Find the maximum absolute value
  float max_abs = 0.0;
  for (int i = 0; i < length; i++) {
    float abs_val = fabs(data[i]);
    if (abs_val > max_abs) {
      max_abs = abs_val;
    }
  }
    
  // Avoid division by zero
  if (max_abs == 0.0) {
    return;  // All values are zero, nothing to normalize
  }
    
  // Scale all values by the same factor
  float scale_factor = 1.0 / max_abs;
  for (int i = 0; i < length; i++) {
    data[i] *= scale_factor;
  }
}

void init_wt(void) {
  float *table;
  float f;
  float d;

  for (int i = 0 ; i < EXWAVEMAX; i++) {
    wt_data[i] = NULL;
    wt_size[i] = 0;
  }

  printf("# make sine wave (%d)\n", EXWAVESINE);
  table = (float *)malloc(SIZE_SINE * sizeof(float));
  for (int i=0; i<SIZE_SINE; i++) {
    float t = (i * 2.0 * M_PI) / (float)SIZE_SINE;
    table[i] = sin(t);
  }
  wt_data[EXWAVESINE] = table;
  wt_size[EXWAVESINE] = SIZE_SINE;
  wt_rate[EXWAVESINE] = MAIN_SAMPLE_RATE;
  wt_one_shot[EXWAVESINE] = 0;
  wt_loop_start[EXWAVESINE] = 0;
  wt_loop_end[EXWAVESINE] = SIZE_SINE-1;

  printf("# make square wave (%d)\n", EXWAVESQR);
  table = (float *)malloc(SIZE_SQR * sizeof(float));
  for (int i=0; i<SIZE_SQR; i++) {
    if (i < SIZE_SQR/2) table[i] = -1;
    else table[i] = 1;
  }
  wt_data[EXWAVESQR] = table;
  wt_size[EXWAVESQR] = SIZE_SQR;
  wt_rate[EXWAVESQR] = MAIN_SAMPLE_RATE;
  wt_one_shot[EXWAVESQR] = 0;
  wt_loop_start[EXWAVESQR] = 0;
  wt_loop_end[EXWAVESQR] = SIZE_SQR-1;

  printf("# make saw-down wave (%d)\n", EXWAVESAWDN);
  table = (float *)malloc(SIZE_SAWDN * sizeof(float));
  f = -1.0;
  d = 2.0 / (float)SIZE_SAWDN;
  for (int i=0; i<SIZE_SAWDN; i++) {
    table[i] = f;
    f += d;
  }
  wt_data[EXWAVESAWDN] = table;
  wt_size[EXWAVESAWDN] = SIZE_SAWDN;
  wt_rate[EXWAVESAWDN] = MAIN_SAMPLE_RATE;
  wt_one_shot[EXWAVESAWDN] = 0;
  wt_loop_start[EXWAVESAWDN] = 0;
  wt_loop_end[EXWAVESAWDN] = SIZE_SAWDN-1;

  printf("# make saw-up wave (%d)\n", EXWAVESAWUP);
  table = (float *)malloc(SIZE_SAWUP * sizeof(float));
  f = 1.0;
  d = 2.0 / (float)SIZE_SAWUP;
  for (int i=0; i<SIZE_SAWUP; i++) {
    table[i] = f;
    f -= d;
  }
  wt_data[EXWAVESAWUP] = table;
  wt_size[EXWAVESAWUP] = SIZE_SAWUP;
  wt_rate[EXWAVESAWUP] = MAIN_SAMPLE_RATE;
  wt_one_shot[EXWAVESAWUP] = 0;
  wt_loop_start[EXWAVESAWUP] = 0;
  wt_loop_end[EXWAVESAWUP] = SIZE_SAWUP-1;

  printf("# make triangle wave (%d)\n", EXWAVETRI);
  //FILE *out = fopen("tri.dat", "w+");
  table = (float *)malloc(SIZE_TRI * sizeof(float));
  f = -1.0;
  d = 4.0 / (float)SIZE_TRI;
  for (int i=0; i<SIZE_TRI; i++) {
    table[i] = f;
    //fprintf(out, "%g\n", f);
    if (i < (SIZE_TRI / 2)) {
      f += d;
    } else {
      f -= d;
    }
  }
  //fclose(out);
  
  wt_data[EXWAVETRI] = table;
  wt_size[EXWAVETRI] = SIZE_TRI;
  wt_rate[EXWAVETRI] = MAIN_SAMPLE_RATE;
  wt_one_shot[EXWAVETRI] = 0;
  wt_loop_start[EXWAVETRI] = 0;
  wt_loop_end[EXWAVETRI] = SIZE_TRI-1;

  printf("# load retro waves (%d to %d)\n", EXWAVEKRG1, EXWAVEKRG32-1);

  korg_init();

  for (int i = EXWAVEKRG1; i < EXWAVEKRG32; i++) {
    int k = i - EXWAVEKRG1;
    int s = kwave_size[k];
    table = malloc(s * sizeof(float));
    for (int j = 0 ; j < s; j++) {
      table[j] = (float)kwave[k][j] / (float)32767;
    }
    wt_data[i] = table;
    wt_size[i] = s;
    wt_rate[i] = MAIN_SAMPLE_RATE;
    wt_one_shot[i] = 0;
    wt_loop_start[i] = 0;
    wt_loop_end[i] = s-1;
  }

  #define PROGMEM
  #include "notamy/impulse_lutset_fxpt.h"

  FILE *out = NULL;

  printf("# load AMY waves (%d to %d)\n", AMYWAVE00, AMYWAVE04);
  out = fopen("amyimpulse.dat", "w+");
  table = (float *)malloc(1024 * sizeof(float));
  for (int i = 0; i < 1024; i++) {
    float g = (float)impulse_fxpt_lutable_0[i] / 32767.0;
    table[i] = g;
    fprintf(out, "%g\n", g);
  }
  fclose(out);
  wt_data[AMYWAVE00] = table;
  wt_size[AMYWAVE00] = 1024;
  wt_rate[AMYWAVE00] = MAIN_SAMPLE_RATE;
  wt_one_shot[AMYWAVE00] = 0;
  
  printf("# generate moog-like squarewave at %d\n", AMYWAVE01);
#define GENSIZE (2048)
  table = (float *)malloc(GENSIZE * sizeof(float));
  generate_moog_squarewave(GENSIZE, table);
  out = fopen("moog_squarewave.dat", "w+");
  for (int i = 0; i < GENSIZE; i++) {
    fprintf(out, "%g\n", table[i]);
  }
  fclose(out);
  wt_data[AMYWAVE01] = table;
  wt_size[AMYWAVE01] = GENSIZE;
  wt_rate[AMYWAVE01] = MAIN_SAMPLE_RATE;
  wt_one_shot[AMYWAVE01] = 0;
  
  // load AMY samples
  int j = AMYSAMPLE99;
  for (int i = 0; i < PCM_SAMPLES; i++) {
    j = i + AMYSAMPLE00;
    if (j > AMYSAMPLE99-1) {
      printf("# too many PCM samples... exit early\n");
      break;
    }
    if (debug) printf("[%d] offset:%d length:%d loopstart:%d loopend:%d midinote:%d offsethz:%g\n",
      j, pcm_map[i].offset, pcm_map[i].length, pcm_map[i].loopstart, pcm_map[i].loopend,
      pcm_map[i].midinote, midi2hz(pcm_map[i].midinote));
    table = malloc(pcm_map[i].length * sizeof(float));
    for (int k = 0; k < pcm_map[i].length; k++) {
      table[k] = (float)pcm[pcm_map[i].offset + k] / 32767.0;
    }
    normalize_preserve_zero(table, pcm_map[i].length);
    wt_data[j] = table;
    wt_size[j] = pcm_map[i].length;
    wt_rate[j] = PCM_AMY_SAMPLE_RATE;
    wt_one_shot[j] = 1;
    wt_loop_enabled[j] = 0;
    wt_loop_start[j] = pcm_map[i].loopstart;
    wt_loop_end[j] = pcm_map[i].loopend;
    wt_midinote[j] = pcm_map[i].midinote;
    wt_offsethz[j] = midi2hz(pcm_map[i].midinote);
  }
  printf("# load AMY samples (%d to %d)\n", AMYSAMPLE00, j);
}

void wt_free(void) {
  for (int i = 0; i < EXWAVEMAX; i++) {
    if (wt_data[i]) {
      if (debug) printf("[%d] freeing...\n", i);
      free(wt_data[i]);
      wt_size[i] = 0;
    }
  }
}

void reset_voice(int i) {
  sample[i] = 0;
  hold[i] = 0;
  amp[i] = 0;
  pan[i] = 0;
  panl[i] = 0.5;
  panr[i] = 0.5;
  interp[i] = 0;
  use_adsr[i] = 0;
  amod_osc[i] = -1;
  fmod_osc[i] = -1;
  pmod_osc[i] = -1;
  hide[i] = 0;
  decimate[i] = 0;
  decicount[i] = 0;
  decittl[i] = 0.0;
  quantize[i] = 0;
  direction[i] = 0;
  adsr_init(i, 1.1f, 0.2f, 0.7f, 0.5f, 44100.0f);
  freq[i] = 440.0;
  osc_set_wt(i, EXWAVESINE);
}

void init_voice(void) {
  for (int i=0; i<VOICE_MAX; i++) {
    reset_voice(i);
  }
}


#define UDP_PORT 60440

#include <arpa/inet.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <ifaddrs.h>

struct sockaddr_in serve;

int udp_open(int port) {
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    int opt = 1;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(int));
    bzero(&serve, sizeof(serve));
    serve.sin_family = AF_INET;
    serve.sin_addr.s_addr = htonl(INADDR_ANY);
    serve.sin_port = htons(port);
    if (bind(sock, (struct sockaddr *)&serve, sizeof(serve)) >= 0) {
        return sock;
    }
    return -1;
}

#define PORT 60440

void *udp(void *arg) {
  int sock = udp_open(PORT);
  if (sock < 0) {
    puts("# udp thread cannot run");
    return NULL;
  }
  pthread_setname_np(pthread_self(), "skred-udp");
  struct timeval tv;
  tv.tv_sec = 1;
  tv.tv_usec = 0;
  setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (struct timeval *)&tv, sizeof(struct timeval));
  int voice = 0;
  struct sockaddr_in client;
  unsigned int client_len = sizeof(client);
  char line[1024];
  int output = 0;
  while (udp_running) {
    int n = recvfrom(sock, line, sizeof(line), 0, (struct sockaddr *)&client, &client_len);
    if (n > 0) {
      line[n] = '\0';
      int r = wire(line, &voice, output);
    } else {
      if (errno = EAGAIN) continue;
      printf("# recvfrom = %d ; errno = %d\n", n, errno);
      perror("# recvfrom");
    }
  }
  if (debug) printf("# udp stopping\n");
  return NULL;
}

int find_starting_point(int buffer_snap, int sw) {
  int j;
  j = buffer_snap - sw;
  float t0 = (oscope_bufferl[j] + oscope_bufferr[j]) / 2.0;
  int i = j-1;
  int c = 1;
  while (c < OSCOPE_LEN) {
    if (i < 0) i = OSCOPE_LEN-1;
    float t1 = (oscope_bufferl[i] + oscope_bufferr[i]) / 2.0;
    if (t0 == 0.0 && t1 == 0.0) {
      // zero
    } else if (t0 < 0 && t1 > 0) break;
    i--;
    c++;
    t0 = t1;
  }
  return i;
}

#define MAG_X_INC (0.05)

#define CONFIG_FILE ".skred_window"

void *oscope(void *arg) {
  pthread_setname_np(pthread_self(), "skred-oscope");
#ifndef USE_RAYLIB
  while (oscope_running) {
    sleep(1);
  }
#else
#include "raylib.h"
#include "rlgl.h"
  //
  Vector2 position;
  FILE *file = fopen(CONFIG_FILE, "r");
  const int screenWidth = SCREENWIDTH;
  const int screenHeight = SCREENHEIGHT;
  float mag_x = 1.0;
  float sw = (float)screenWidth;
  if (file != NULL) {
    /*
    x y oscope_display_mag mag_x
    */
    int r;
    r = fscanf(file, "%f %f %f %f",
      &position.x, &position.y, &oscope_display_mag, &mag_x);
    if (position.x < 0) position.x = 0;
    if (position.y < 0) position.y = 0;
    if (oscope_display_mag <= 0) oscope_display_mag = 1;
    // if (mag_x <= 0) mag_x = 1;
    fclose(file);
  } else {
    printf("# %s read fopen fail\n", CONFIG_FILE);
  }
  //
  SetTraceLogLevel(LOG_NONE);
  InitWindow(screenWidth, screenHeight, "skred-oscope");
  //
  SetWindowPosition((int)position.x, (int)position.y);
  //
  Vector2 dot = { (float)screenWidth/2, (float)screenHeight/2 };
  SetTargetFPS(12);
  float sh = (float)screenHeight;
  float h0 = screenHeight / 2.0;
  char osd[1024] = "?";
  int osd_dirty = 1;
  float y = h0;
  float a = 1.0;
  int show_l = 1;
  int show_r = 1;
  Color color_left = {0, 255, 255, 128};
  Color color_right = {255, 255, 0, 128};
  Color tGreen = {0, 255, 0, 128};
  Color green0 = {0, 255, 0, 128};
  Color green1 = {0, 128, 0, 128};
  while (oscope_running && !WindowShouldClose()) {
    if (mag_x <= 0) mag_x = MAG_X_INC;
    sw = (float)screenWidth / mag_x;
    if (sw >= (OSCOPE_LEN/2)) sw = (float)(OSCOPE_LEN/2);
    int shifted = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
    if (IsKeyDown(KEY_ONE)) {
      if (show_l == 1) show_l = 0; else show_l = 1;
    }
    if (IsKeyDown(KEY_TWO)) {
      if (show_r == 1) show_r = 0; else show_r = 1;
    }
    if (IsKeyDown(KEY_A)) {
      if (shifted) {
        oscope_display_mag -= 0.1;
        a -= 0.1;
      } else {
        oscope_display_mag += 0.1;
        a += 0.1;
      }
      osd_dirty++;
    }
    if (IsKeyDown(KEY_RIGHT)) {
      //oscope_display_inc += 0.1;
      mag_x += MAG_X_INC;
      osd_dirty++;
    }
    if (IsKeyDown(KEY_LEFT)) {
      //oscope_display_inc -= 0.1;
      mag_x -= MAG_X_INC;
      osd_dirty++;
    }
    if (IsKeyDown(KEY_UP)) {
      y += 1.0;
      osd_dirty++;
    }
    if (IsKeyDown(KEY_DOWN)) {
      y -= 1.0;
      osd_dirty++;
    }
    if (osd_dirty) {
      sprintf(osd, "mag_x:%g sw:%g y:%g a:%g mag:%g ch:%d",
        mag_x, sw, y, a, oscope_display_mag, oscope_channel);
    }
    int buffer_snap = oscope_buffer_pointer;
    BeginDrawing();
    ClearBackground(BLACK);
    for (int i = 0; i < oscope_wave_len; i++) {
      float avg = (oscope_wave[i] / 2.0 + 0.5) * (float)OWHEIGHT/2.0;
      float min = (oscope_min[i] / 2.0 + 0.5) * (float)OWHEIGHT/2.0;
      float max = (oscope_max[i] / 2.0 + 0.5) * (float)OWHEIGHT/2.0;
      dot.x = i;
      DrawLine(i, max, i, OWHEIGHT/4.0, green1);
      DrawLine(i, min, i, OWHEIGHT/4.0, green1);
      //DrawLine(i, avg, i, OWHEIGHT/4.0, green1);
      dot.y = max; DrawCircleV(dot, 1, green0);
      //dot.y = avg; DrawCircleV(dot, 1, green0);
      dot.y = min; DrawCircleV(dot, 1, green0);
    }
    // show a wave table
    if (oscope_wave_len) {
      char s[32];
      sprintf(s, "%d", oscope_wave_index);
      DrawText(s, 10, 10, 20, YELLOW);
    }
    // find starting point for display
    //int start = find_starting_point(buffer_snap, OSCOPE_LEN/2);
    int start = find_starting_point(buffer_snap, screenWidth);
    int actual = 0;
    float xoffset = (float)SCREENWIDTH / 8.0;
    //DrawLine(xoffset * oscope_display_inc, -sh, xoffset * oscope_display_inc, sh, WHITE);
    start -= xoffset;
    DrawText(osd, 10, SCREENHEIGHT-20, 20, GREEN);
    rlPushMatrix();
    rlTranslatef(0.0, y, 0.0); // x,y,z
    rlScalef(mag_x,oscope_display_mag,1.0);
      DrawLine(0, 0, sw, 0, DARKGREEN);
      if (start >= oscope_len) start = 0;
      actual = start;
      for (float i = 0.0; i < sw; i++) {
        dot.x = i;
#if 1
        if (i == start) DrawLine(dot.x, -sh, dot.x, sh, PURPLE);
        if (actual == 0) DrawLine(dot.x, -sh, dot.x, sh, YELLOW);
        if (actual == buffer_snap) DrawLine(dot.x, -sh, dot.x, sh, BLUE);
        if (actual >= (OSCOPE_LEN-1)) {
          DrawLine(dot.x, -sh, dot.x, sh, RED);
          actual = 0;
        }
#endif
        if (show_l) {
          dot.y = oscope_bufferl[actual] * h0 * oscope_display_mag;
          DrawCircleV(dot, 1, color_right);
        }
        if (show_r) {
          dot.y = oscope_bufferr[actual] * h0 * oscope_display_mag;
          DrawCircleV(dot, 1, color_left);
        }
        actual++;
      }
    rlPopMatrix();
    char *bell = " ";
    char *ding = " ";
    if ((sample_count % tick_mod) == 0) bell = "!";
    if (tick_self_count != tick_engine_count) ding = "*";
    sprintf(osd, "%s %s %ld %ld %ld %ld",
      bell,
      ding,
      tick_self_count,
      tick_engine_count,
      tick_mod,
      sample_count);
    DrawText(osd, SCREENWIDTH/5, SCREENHEIGHT/3, 40, BLUE);
    EndDrawing();
  }
  //
  file = fopen(CONFIG_FILE, "w");
  if (file != NULL) {
    Vector2 position = GetWindowPosition();
    fprintf(file, "%g %g %g %g", position.x, position.y, oscope_display_mag, mag_x);
    fclose(file);
  } else {
    printf("# %s write fopen fail\n", CONFIG_FILE);
  }
  //
  CloseWindow();
  oscope_running = 0;
#endif
  if (debug) printf("# oscope stopping\n");
  return NULL;
}

float midi2hz(int f) {
  float g = 440.0 * pow(2.0, (f - 69.0) / 12.0);
  return g;
}

int freq_set(int voice, float f) {
  if (f > 0 && f < (double)MAIN_SAMPLE_RATE) {
    freq[voice] = f;
    osc_set_freq(voice, f);
    return 0;
  }
  return ERR_FREQUENCY_OUT_OF_RANGE;
}

int amp_set(int voice, float f) {
  if (f >= 0) {
    use_adsr[voice] = 0;
    amp[voice] = f * AFACTOR;
  } else return ERR_AMPLITUDE_OUT_OF_RANGE;
  return 0;
}

int wave_set(int voice, int n) {
  if (n >= 0 && n < EXWAVEMAX && wt_data[n] && wt_size[n]) {
    oscope_wave_index = n;
    osc_set_wt(voice, n);
    update_oscope_wave(osc[voice].table, osc[voice].table_size);
  } else return ERR_INVALID_WAVE;
  return 0;
}

int wave_reset(int voice, int n) {
  if (n >= 0 && n < VOICE_MAX) {
    reset_voice(n);
  } else {
    init_voice();
  }
  return 0;
}

int wave_default(int voice) {
  float g = midi2hz(osc[voice].midinote);
  freq[voice] = g;
  note[voice] = osc[voice].midinote;
  osc_set_freq(voice, g);
  update_oscope_wave(osc[voice].table, osc[voice].table_size);
  return 0;
}

int wave_loop(int voice, int state) {
  if (state < 0) {
    if (osc[voice].loop_enabled == 0) state = 1;
    else state = 0;
  }
  osc[voice].loop_enabled = state;
  return 0;
}

int wave_mute(int voice, int state) {
  if (state < 0) {
    if (hide[voice] == 0) state = 1;
    else state = 0;
  }
  hide[voice] = state;
  return 0;
}

int wave_dir(int voice, int state) {
  if (state < 0) {
    if (direction[voice] == 0) state = 1;
    else state = 0;
  }
  direction[voice] = state;
  return 0;
}

int wave_interp(int voice, int state) {
  if (state < 0) {
    if (interp[voice] == 0) state = 1;
    else state = 0;
  }
  interp[voice] = state;
  return 0;
}

int voice_set(int n, int *old_voice) {
  if (n >= 0 && n < VOICE_MAX) {
    if (old_voice) *old_voice = n;
    return 0;
  }
  return ERR_INVALID_VOICE;
}

int voice_trigger(int voice) {
  osc_trigger(voice);
  return 0;
}

int oscope_start(int sub) {
  if (oscope_running == 0) {
    oscope_running = 1;
    pthread_create(&oscope_thread, NULL, oscope, NULL);
    pthread_detach(oscope_thread);
  }
  return 0;
}

int wave_load(int which, int where) {
  if (where < EXTSAMPLE00 || where >= EXTSAMPLE99) return ERR_INVALID_EXTSAMPLE;
  char name[1024];
  sprintf(name, "wave%d.wav", which);
  wav_t wav;
  int len;
  float *table = mw_get(name, &len, &wav);
  if (table == NULL) {
    printf("# can not read %s\n", name);
    return ERR_INVALID_EXTSAMPLE;
  } else {
    if (wt_data[where]) {
      if (wt_free_ptr >= WT_FREE_LEN) {
        wt_free_ptr = 0;
      }
      if (wt_free_list[wt_free_ptr]) {
        printf("# freeing old wave %d\n", wt_free_ptr);
        free(wt_free_list[wt_free_ptr]);
      }
      wt_free_list[wt_free_ptr++] = wt_data[where];
    }
    wt_data[where] = table;
    wt_size[where] = len;
    wt_rate[where] = wav.SamplesRate;
    wt_one_shot[where] = 1;
    wt_loop_enabled[where] = 0;
    wt_loop_start[where] = 1;
    wt_loop_end[where] = len;
    wt_midinote[where] = 69;
    wt_offsethz[where] = (float)len / (float)wav.SamplesRate * 440.0;
    printf("# read %d frames from %s to %d (ch:%d sr:%d)\n",
      len, name, where, wav.Channels, wav.SamplesRate);
  }
  return 0;
}

int amod_set(int voice, int osc, float f) {
  if (voice < 0 && voice >= VOICE_MAX) return ERR_INVALID_VOICE;
  if (osc < 0 && osc >= VOICE_MAX) return ERR_INVALID_VOICE;
  amod_osc[voice] = osc;
  amod_depth[voice] = f;
  return 0;
}

int fmod_set(int voice, int osc, float f) {
  if (voice < 0 && voice >= VOICE_MAX) return ERR_INVALID_VOICE;
  if (osc < 0 && osc >= VOICE_MAX) return ERR_INVALID_VOICE;
  fmod_osc[voice] = osc;
  fmod_depth[voice] = f;
  return 0;
}

int pmod_set(int voice, int osc, float f) {
  if (voice < 0 && voice >= VOICE_MAX) return ERR_INVALID_VOICE;
  if (osc < 0 && osc >= VOICE_MAX) return ERR_INVALID_VOICE;
  pmod_osc[voice] = osc;
  pmod_depth[voice] = f;
  return 0;
}

int patch_load(int voice, int n, int output) {
  char file[1024];
  sprintf(file, "exp%d.patch", n);
  FILE *in = fopen(file, "r");
  int r = 0;
  if (in) {
    char line[1024];
    while (fgets(line, sizeof(line), in) != NULL) {
      size_t len = strlen(line);
      if (len > 0 && line[len-1] == '\n') line[len-1] = '\0';
      if (output) printf("# %s\n", line);
      int temp_voice = voice;
      r = wire(line, &temp_voice, trace);
      if (r != 0) {
        if (output) printf("# error in patch\n");
        break;
      }
    }
    fclose(in);
  }
  //hide[voice] = 0;
  return r;
}

int freq_midi(int voice, float f) {
  if (f >= 0.0 && f <= 127.0) {
    float g = midi2hz(f);
    return freq_set(voice, g);
  }
  return ERR_INVALID_MIDI_NOTE;
}

int wave_deci(int voice, int n) {
  decimate[voice] = n;
  return 0;
}

int wave_quant(int voice, int n) {
  quantize[voice] = n;
  return 0;
}

int pan_set(int voice, float f) {
  if (f >= -1.0 && f <= 1.0) {
    pan[voice] = f;
    panl[voice] = (1.0 - f) / 2.0;
    panr[voice] = (1.0 + f) / 2.0;          
  } else {
    return ERR_PAN_OUT_OF_RANGE;
  }
  return 0;
}

int adsr_set(int voice, float a, float d, float s, float r) {
  amp_env[voice].attack_time = a;
  amp_env[voice].decay_time = d;
  amp_env[voice].sustain_level = s * AFACTOR;
  amp_env[voice].release_time = r;
  return 0;
}

int adsr_velocity(int voice, float f) {
  if (f == 0) {
    adsr_release(voice);
  } else {
    use_adsr[voice] = 1;
    amp_env[voice].sustain_level = f;
    amp_env[voice].state = ADSR_ATTACK;
    amp_env[voice].value = amp[voice];
    amp_env[voice].time = 0.0f;
    osc_trigger(voice);
    //adsr_trigger(voice, f);
  }
  return 0;
}

int wavetable_show(int n) {
  if (n >= 0 && n < EXWAVEMAX && wt_data[n] && wt_size[n]) {
    oscope_wave_index = n;
    float *table = wt_data[n];
    int size = wt_size[n];
    int crossing = 0;
    int zero = 0;
    float min, max, ttl = 0, avg;
    min = table[0];
    max = table[0];
    for (int i = 1; i < size; i++) {
      if (table[i] < min) min = table[i];
      if (table[i] > max) max = table[i];
      ttl += table[i];
      if (table[i-1] == 0.0 || table[i] == 0.0) {
        // Prevent abiguity with multiple zeroes
        zero++;
      } else if ((table[i-1] > 0 && table[i] < 0) || (table[i-1] < 0 && table[i] > 0)) {
        // Check for sign change
        crossing++;
      }
    }
    avg = ttl / (float)size;
    printf("# w%d size:%d", n, size);
    //printf(" min:%g max:%g ttl:%g avg:%g 0:%d x:%d", min, max, ttl, avg, zero, crossing);
    printf(" +hz:%g midi:%d", wt_offsethz[n], wt_midinote[n]);
    puts("");
    downsample_block_average_min_max(table, size, oscope_wave, OWWIDTH, oscope_min, oscope_max);
    oscope_wave_len = OWWIDTH;
  }
  return 0;
}

int cz_set(int v, int n, float f) {
  cz[v] = n;
  czd[v] = f;
  return 0;
}
