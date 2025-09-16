#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <math.h>

#define SOKOL_AUDIO_IMPL
#include "sokol_audio.h"

float tick_max = 10.0f;
float tick_inc = 1.0f;
int tick_frames = 0;
static volatile uint64_t sample_count = 0;

void tick(int);
void tick_max_set(float, float);

int debug = 0;
int trace = 0;

int console_voice = 0;

#define HISTORY_FILE ".sok1_history"
#define MAIN_SAMPLE_RATE (44100)
#define VOICE_MAX (32)
#define CHANNEL_NUM (2)
#define AMY_FACTOR (0.025f)

#define UDP_PORT 60440
int udp_port = UDP_PORT;

#define SCREENWIDTH (800)
#define SCREENHEIGHT (480)

#define SCOPE_LEN (MAIN_SAMPLE_RATE)

// inspired by AMY :)
enum {
  WAVE_TABLE_SINE,     // 0
  WAVE_TABLE_SQR,      // 1
  WAVE_TABLE_SAW_DOWN,    // 2
  WAVE_TABLE_SAW_UP,    // 3
  WAVE_TABLE_TRI,      // 4
  WAVE_TABLE_NOISE,    // 5

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
  WAVE_TABLE_KRG16,

  WAVE_TABLE_KRG17,
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
  WAVE_TABLE_KRG32,

  AMY_SAMPLE_00 = 100,
  AMY_SAMPLE_99 = 100+99,

  EXT_SAMPLE_00 = 200,
  EXT_SAMPLE_99 = 200 + 99,
  WAVE_TABLE_MAX
};

int audio_show(void) {
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

#include <pthread.h>
#include <time.h>

#define SAVE_WAVE_LEN (8)
static float *save_wave_list[SAVE_WAVE_LEN]; // to keep from crashing the engine, have a place to store free-ed waves
static int save_wave_ptr = 0;

float *wave_table_data[WAVE_TABLE_MAX];
int wave_size[WAVE_TABLE_MAX];
float wave_rate[WAVE_TABLE_MAX];
int wave_one_shot[WAVE_TABLE_MAX];
int wave_loop_enabled[WAVE_TABLE_MAX];
int wave_loop_start[WAVE_TABLE_MAX];
int wave_loop_end[WAVE_TABLE_MAX];
int wave_midi_note[WAVE_TABLE_MAX];
float wave_offset_hz[WAVE_TABLE_MAX];

void wave_free(void);

float voice_freq[VOICE_MAX];
float voice_note[VOICE_MAX];
float voice_sample[VOICE_MAX];
float samples[VOICE_MAX][SCOPE_LEN];
float voice_amp[VOICE_MAX];
float voice_user_amp[VOICE_MAX];
float voice_pan_left[VOICE_MAX];
float voice_pan_right[VOICE_MAX];
float voice_pan[VOICE_MAX];
int voice_use_amp_envelope[VOICE_MAX];
int voice_freq_mod_osc[VOICE_MAX];
int voice_pan_mod_osc[VOICE_MAX];
int voice_amp_mod_osc[VOICE_MAX];
int voice_cz_mod_osc[VOICE_MAX];
float voice_freq_mod_depth[VOICE_MAX];
float voice_pan_mod_depth[VOICE_MAX];
float voice_amp_mod_depth[VOICE_MAX];
float voice_cz_mod_depth[VOICE_MAX];
int voice_disconnect[VOICE_MAX];
int voice_quantize[VOICE_MAX];
int voice_direction[VOICE_MAX];
int voice_phase_reset[VOICE_MAX];

int voice_wave_table_index[VOICE_MAX];

int voice_cz_mode[VOICE_MAX];
float voice_cz_distortion[VOICE_MAX];

// Low-pass filter state structure
typedef struct {
  float x1, x2;  // Input delay line
  float y1, y2;  // Output delay line
  float b0, b1, b2;  // Feedforward coefficients
  float a1, a2;      // Feedback coefficients
    
  // Parameter tracking for coefficient updates
  float last_freq;
  float last_resonance;
  float last_gain;
  int last_mode;
} mmf_t;

float voice_filter_freq[VOICE_MAX];
float voice_filter_res[VOICE_MAX];
float voice_filter_gain[VOICE_MAX];
int voice_filter_mode[VOICE_MAX];
mmf_t voice_filter[VOICE_MAX];

void mmf_init(int, float, float, float);
void mmf_set_params(int, float, float, float);
float mmf_process(int, float);
int mmf_set_freq(int, float);
int mmf_set_res(int, float);
int mmf_set_gain(int, float);

void voice_reset(int i);
void voice_init(void);

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
  int midi_note;
  float offset_hz;
} osc_t;

osc_t osc[VOICE_MAX];

enum {
  SCOPE_TRIGGER_BOTH,
  SCOPE_TRIGGER_RISING,
  SCOPE_TRIGGER_FALLING,
};

int scope_trigger_mode = SCOPE_TRIGGER_RISING;

int scope_running = 0;
int scope_len = SCOPE_LEN;
float scope_buffer_left[SCOPE_LEN];
float scope_buffer_right[SCOPE_LEN];
int scope_buffer_pointer = 0;
float scope_display_pointer = 0.0f;
float scope_display_inc = 1.0f;
float scope_display_mag = 1.0f;
#define SCOPE_WAVE_WIDTH (SCREENWIDTH/4)
#define SCOPE_WAVE_HEIGHT (SCREENHEIGHT/2)
float scope_wave_data[SCOPE_WAVE_WIDTH];
int scope_wave_index = 0;
float scope_wave_min[SCOPE_WAVE_WIDTH];
float scope_wave_max[SCOPE_WAVE_WIDTH];
int scope_wave_len = 0;
int scope_channel = -1; // -1 means all

float osc_get_phase_inc(int v, float f) {
  float g = f;
  if (osc[v].one_shot) g /= osc[v].offset_hz;
  float phase_inc = (g * (float)osc[v].table_size) / osc[v].table_rate * (osc[v].table_rate / MAIN_SAMPLE_RATE);
  return phase_inc;
}

void osc_set_freq(int v, float f) {
  // Compute the frequency in "table samples per system sample"
  // This works even if table_rate ≠ system rate
  float g = f;
  if (osc[v].one_shot) g /= osc[v].offset_hz;
  osc[v].phase_inc = (g * (float)osc[v].table_size) / osc[v].table_rate * (osc[v].table_rate / MAIN_SAMPLE_RATE);
}

float cz_phasor(int n, float p, float d, int table_size) {
  float phase = p / (float)table_size;
  switch (n) {
    case 1: // 2 :: saw -> pulse
      if (phase < d) phase *= (0.5f / d);
      else phase = 0.5f + (phase - d) * (0.5f / (1.0f - d));
      break;
    case 2: // 3 :: square (folded sine)
      if (phase < 0.5) phase *= 0.5f / (0.5f - d * 0.5f);
      else phase = 1.0f - (1.0f - phase) * (0.5f / (0.5f - d * 0.5f));
      break;
    case 3: // 4 :: triangle
      if (phase < 0.5f) phase *= (0.5f / (0.5f - d * 0.5f));
      else phase = 0.5f + (phase - 0.5f) * (0.5f / (0.5f - d * 0.5f));
      break;
    case 4: // 5 :: double sine
      phase = fmodf(phase * 2.0f, 1.0f);
      break;
    case 5: // 6 :: saw -> triangle
      if (phase < 0.5f) phase *= (0.5f / (0.5f - d * 0.5f));
      else phase = 0.5f + (phase - 0.5f) * (0.5f / (0.5f + d * 0.5f));
      break;
    case 6: // 7 :: resonant 1
      phase = powf(phase, 1.0f + 4.0f * d);
      break;
    case 7: // 8 :: resonant 2
      phase = powf(phase, 1.0f + 8.0f * d);
      break;
    default:
      return p;
  }
  return fmodf(phase * (float)table_size, (float)table_size);
  return phase * (float)table_size;
}


float osc_next(int voice, float phase_inc) {
    if (osc[voice].finished) return 0.0f;
    if (voice_direction[voice]) phase_inc *= -1.0f;

    // Step phase
    osc[voice].phase += phase_inc;

    // NaN check
    if (!isfinite(osc[voice].phase)) {
        osc[voice].phase = 0.0f;
        osc[voice].finished = osc[voice].one_shot;
        return 0.0f;
    }

    int table_size = osc[voice].table_size;

    // Clamp loop boundaries to valid range
    float loop_start = osc[voice].loop_enabled ? (float)osc[voice].loop_start : 0.0f;
    float loop_end   = osc[voice].loop_enabled ? (float)osc[voice].loop_end   : (float)table_size;
    if (loop_end <= loop_start) { loop_start = 0.0f; loop_end = (float)table_size; }

    // Handle forward playback
    if (phase_inc >= 0.0f) {
        // One-shot forward
        if (osc[voice].one_shot && osc[voice].phase >= loop_end) {
            osc[voice].phase = loop_end - 1e-6f;
            osc[voice].finished = 1;
        }
        // Loop forward
        else if (osc[voice].loop_enabled && osc[voice].phase >= loop_end) {
            osc[voice].phase = loop_start + fmodf(osc[voice].phase - loop_start, loop_end - loop_start);
        }
        // Wrap full table
        else if (!osc[voice].loop_enabled && osc[voice].phase >= (float)table_size) {
            osc[voice].phase -= (float)table_size;
        }
    }
    // Handle backward playback
    else {
        // One-shot backward
        if (osc[voice].one_shot && osc[voice].phase < loop_start) {
            osc[voice].phase = loop_start;
            osc[voice].finished = 1;
        }
        // Loop backward
        else if (osc[voice].loop_enabled && osc[voice].phase < loop_start) {
            osc[voice].phase = loop_end - fmodf(loop_start - osc[voice].phase, loop_end - loop_start);
        }
        // Wrap full table
        else if (!osc[voice].loop_enabled && osc[voice].phase < 0.0f) {
            osc[voice].phase += (float)osc[voice].table_size;
        }
    }

    int idx;
    if (voice_cz_mode[voice]) {
      int dv = voice_cz_mod_osc[voice];
      float dm = 1.0f;
      if (dv >= 0) dm = voice_sample[dv] * voice_cz_mod_depth[voice];
      idx = (int)cz_phasor(voice_cz_mode[voice], osc[voice].phase, voice_cz_distortion[voice] + dm, table_size);
      //idx = (int)cz_phasor(cz[voice], osc[voice].phase, czd[voice], table_size);
    } else {
      idx = (int)osc[voice].phase;
    }
    if (idx >= table_size) idx = table_size - 1;
    if (idx < 0) idx = 0;
    return osc[voice].table[idx];
}

void osc_set_wt(int voice, int wave) {
  if (wave_table_data[wave] && wave_size[wave] && wave_rate[wave] > 0.0) {
    voice_wave_table_index[voice] = wave;
    int update_freq = 0;
    if (wave_one_shot[wave]) osc[voice].finished = 1;
    if (
      osc[voice].table_rate != wave_rate[wave] ||
      osc[voice].table_size != wave_size[wave]
      ) update_freq = 1;
    osc[voice].table_rate = wave_rate[wave];
    osc[voice].table_size = wave_size[wave];
    osc[voice].table = wave_table_data[wave];
    osc[voice].one_shot = wave_one_shot[wave];
    osc[voice].loop_start = wave_loop_start[wave];
    osc[voice].loop_enabled = wave_loop_enabled[wave];
    osc[voice].loop_end = wave_loop_end[wave];
    osc[voice].midi_note = wave_midi_note[wave];
    osc[voice].offset_hz = wave_offset_hz[wave];
    // osc[voice].phase = 0;
    if (update_freq) {
      osc_set_freq(voice, voice_freq[voice]);
    }
  }
}

void osc_trigger(int voice) {
  if (1 || osc[voice].one_shot) {
    if (voice_direction[voice]) {
      osc[voice].phase = (float)(osc[voice].table_size - 1);
    } else {
      osc[voice].phase = 0;
    }
    osc[voice].finished = 0;
  }
}

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

envelope_t voice_amp_envelope[VOICE_MAX];

// Initialize the envelope
void envelope_init(int v, float attack_time, float decay_time,
               float sustain_level, float release_time, float ignore) {
    voice_amp_envelope[v].a = attack_time;
    voice_amp_envelope[v].d = decay_time;
    voice_amp_envelope[v].s = sustain_level;
    voice_amp_envelope[v].r = release_time;
    voice_amp_envelope[v].attack_time = attack_time * MAIN_SAMPLE_RATE; // convert seconds to samples
    voice_amp_envelope[v].decay_time = decay_time * MAIN_SAMPLE_RATE;
    voice_amp_envelope[v].sustain_level = fmaxf(0, fminf(1.0f, sustain_level)); // clamp 0 to 1
    voice_amp_envelope[v].release_time = release_time * MAIN_SAMPLE_RATE;
    voice_amp_envelope[v].sample_start = 0;
    voice_amp_envelope[v].sample_release = 0;
    voice_amp_envelope[v].is_active = 0;
}

// Trigger the envelope (note on)
void amp_envelope_trigger(int v, float f) {
    voice_amp_envelope[v].sample_start = sample_count;
    voice_amp_envelope[v].sample_release = 0;
    voice_amp_envelope[v].velocity = f;
    voice_amp_envelope[v].is_active = 1;
}

// Release the envelope (note off)
void amp_envelope_release(int v) {
    if (voice_amp_envelope[v].is_active) {
        voice_amp_envelope[v].sample_release = sample_count;
    }
}

// Get the current amplitude (0 to 1) at given sample count
float amp_envelope_step(int v) {
    if (!voice_amp_envelope[v].is_active) return 0;

    float samples_since_start = (float)(sample_count - voice_amp_envelope[v].sample_start);

    // Attack phase
    if (samples_since_start < voice_amp_envelope[v].attack_time) {
        return samples_since_start / voice_amp_envelope[v].attack_time; // linear ramp up
    }

    // Decay phase
    float decay_start = voice_amp_envelope[v].attack_time;
    if (samples_since_start < decay_start + voice_amp_envelope[v].decay_time) {
        float samples_in_decay = samples_since_start - decay_start;
        float decay_progress = samples_in_decay / voice_amp_envelope[v].decay_time;
        return 1.0f - decay_progress * (1.0f - voice_amp_envelope[v].sustain_level); // linear decay to sustain
    }

    // Sustain phase
    if (voice_amp_envelope[v].sample_release == 0) {
        return voice_amp_envelope[v].sustain_level;
    }

    // Release phase
    float samples_since_release = (float)(sample_count - voice_amp_envelope[v].sample_release);
    if (samples_since_release < voice_amp_envelope[v].release_time) {
        float release_progress = samples_since_release / voice_amp_envelope[v].release_time;
        return voice_amp_envelope[v].sustain_level * (1.0f - release_progress); // linear ramp down
    }

    // Envelope finished
    voice_amp_envelope[v].is_active = 0;
    return 0.0f;
}

float quantize_bits_int(float v, int bits) {
  int levels = (1 << bits) - 1;
  int iv = (int)(v * (float)levels + 0.5);
  return (float)iv * (1.0f / (float)levels);
}

int main_running = 1;
int udp_running = 1;

void show_stats(void) {
  // do something useful
}

float scope_buffer_get(int *index) {
  if (index == NULL) return 0;
  int i = *index;
  if (i < 0) {
    i = SCOPE_LEN - 1;
  } else if (i >= SCOPE_LEN) {
    i = 0;
  }
  float a = (scope_buffer_left[i] + scope_buffer_right[i]) / 2.0f;
  *index = i;
  return a;
}

void engine(float *buffer, int num_frames, int num_channels, void *user_data) { // , void *user_data) {
  tick(num_frames);
  for (int i = 0; i < num_frames; i++) {
    sample_count++;
    float sample_left = 0.0f;
    float sample_right = 0.0f;
    float f = 00.f;
    for (int n = 0; n < VOICE_MAX; n++) {
      if (voice_amp[n] == 0) continue;
      if (osc[n].finished) continue;
      if (voice_freq_mod_osc[n] >= 0) {
        int m = voice_freq_mod_osc[n];
        float g = voice_sample[m] * voice_freq_mod_depth[n];
        float h = osc_get_phase_inc(n, voice_freq[n] + g);
        f = osc_next(n, h);
      } else {
        f = osc_next(n, osc[n].phase_inc);
      }
      voice_sample[n] = f;
      if (voice_quantize[n]) voice_sample[n] = quantize_bits_int(voice_sample[n], voice_quantize[n]);

      // mmf EXPERIMENTAL
      if (voice_filter_mode[n]) voice_sample[n] = mmf_process(n, voice_sample[n]);

      // apply amp to sample
      if (voice_use_amp_envelope[n]) {
        float env = amp_envelope_step(n) * voice_amp_envelope[n].velocity;
        voice_sample[n] *= (env * voice_amp[n]);
      } else {
        voice_sample[n] *= voice_amp[n];
      }
      if (voice_amp_mod_osc[n] >= 0) {
        int m = voice_amp_mod_osc[n];
        float g = voice_sample[m] * voice_amp_mod_depth[n];
        voice_sample[n] *= g;
      }
      // accumulate samples
      if (voice_disconnect[n] == 0) {
        if (voice_pan_mod_osc[n] >= 0) {
          float q = voice_sample[voice_pan_mod_osc[n]] * voice_pan_mod_depth[n];
          voice_pan_left[n] = (1.0f - q) / 2.0f;
          voice_pan_right[n] = (1.0f + q) / 2.0f;
        }
        float left = voice_sample[n] * voice_pan_left[n];
        float right = voice_sample[n] * voice_pan_right[n];
        sample_left += left;
        sample_right += right;
        if (scope_channel == n) {
          scope_buffer_left[scope_buffer_pointer] = left;
          scope_buffer_right[scope_buffer_pointer] = right;
        }
      }
    }
    // Write to all channels
    buffer[i * num_channels + 0] = sample_left;
    buffer[i * num_channels + 1] = sample_right;
    if (scope_channel < 0) {
      scope_buffer_left[scope_buffer_pointer] = sample_left;
      scope_buffer_right[scope_buffer_pointer] = sample_right;
    }
    scope_buffer_pointer++;
    if (scope_buffer_pointer >= scope_len) scope_buffer_pointer = 0;
  }
}

void sleep_float(double seconds) {
  if (seconds < 0.0f) return; // Invalid input
  struct timespec ts;
  ts.tv_sec = (time_t)seconds; // Whole seconds
  ts.tv_nsec = (long)((seconds - (double)ts.tv_sec) * 1e9); // Fractional part to nanoseconds
  nanosleep(&ts, NULL);
}

void wave_table_init(void);

void *udp_main(void *arg);
void *scope_main(void *arg);

int current_voice = 0;

#include <dirent.h>

void show_threads(void) {
  DIR* dir = opendir("/proc/self/task");
  struct dirent* entry;
  if (dir == NULL) {
    perror("# failed to open /proc/self/task");
    return;
  }

  // Iterate through each thread directory
  while ((entry = readdir(dir)) != NULL) {
    if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;
    char path[4096], name[4096];
    name[0] = '\0';
    snprintf(path, sizeof(path), "/proc/self/task/%s/comm", entry->d_name);
    FILE* f = fopen(path, "r");
    if (f) {
      if (fgets(name, sizeof(name), f)) {
        unsigned long n = strlen(name);
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
  ERR_PARSING_ERROR,
  ERR_INVALID_PATCH,
  ERR_INVALID_MIDI_NOTE,
  ERR_INVALID_MOD,
  //
  ERR_INVALID_AMP,
  ERR_INVALID_FILTER_MODE,
  ERR_INVALID_FILTER_RES,
  ERR_INVALID_FILTER_GAIN,
  ERR_INVALID_PAN,
  ERR_INVALID_QUANT,
  ERR_INVALID_FREQ,
  ERR_INVALID_WAVETABLE,
  // add new stuff before here...
  ERR_UNKNOWN,
};

char *all_err_str[ERR_UNKNOWN+1] = {
  [ERR_EXPECTED_INT] = "expected int",
  [ERR_EXPECTED_FLOAT] = "expected float",
  [ERR_INVALID_VOICE] = "invalid voice",
  [ERR_FREQUENCY_OUT_OF_RANGE] = "frequency out of range",
  [ERR_AMPLITUDE_OUT_OF_RANGE] = "amplitude out-of-range",
  [ERR_INVALID_WAVE] = "invalid wave",
  [ERR_EMPTY_WAVE] = "empty wave",
  [ERR_INVALID_DIRECTION] = "invalid direction",
  [ERR_INVALID_LOOPING] = "invalid looping",
  [ERR_PAN_OUT_OF_RANGE] = "pan out-of-range",
  [ERR_INVALID_DELAY] = "invalid delay",
  [ERR_INVALID_MODULATOR] = "invalid modulator",
  [ERR_UNKNOWN_FUNC] = "unknown func",
  [ERR_UNKNOWN_SYS] = "unknown sys",
  [ERR_INVALID_TRACE] = "invalid trace",
  [ERR_INVALID_DEBUG] = "invalid debug",
  [ERR_INVALID_MUTE] = "invalid mute",
  [ERR_INVALID_EXT_SAMPLE] = "invalid external sample",
  [ERR_PARSING_ERROR] = "parsing error",
  [ERR_INVALID_PATCH] = "invalid patch",
  [ERR_INVALID_MIDI_NOTE] = "invalid midi note",
  [ERR_INVALID_MOD] = "invalid mod",
  //
  [ERR_INVALID_AMP] = "invalid amp",
  [ERR_INVALID_PAN] = "invalid pan",
  [ERR_INVALID_QUANT] = "invalid quant",
  [ERR_INVALID_FREQ] = "invalid freq",
  [ERR_INVALID_WAVETABLE] = "invalid wave table",
  // add new stuff before here...
  [ERR_UNKNOWN] = "x",
};

char *err_str(int n) {
  if (n >= 0 && n <= ERR_UNKNOWN) {
    if (all_err_str[n]) {
      return all_err_str[n];
    }
  }
  return "no-string";
}

#define VOICE_STACK_LEN (8)
typedef struct {
  float s[VOICE_STACK_LEN];
  int ptr;
} voice_stack_t;

void voice_push(voice_stack_t *s, float n) {
  s->ptr++;
  if (s->ptr >= VOICE_STACK_LEN) s->ptr = 0;
  s->s[s->ptr] = n;
}

float voice_pop(voice_stack_t *s) {
  float n = s->s[s->ptr];
  s->ptr--;
  if (s->ptr < 0) s->ptr = VOICE_STACK_LEN-1;
  return n;
}

char *voice_format(int v, char *out) {
  if (out == NULL) return "(NULL)";
  char *ptr = out;
  int n = sprintf(ptr, "v%d w%d b%d B%d n%g f%g a%g p%g c%d,%g J%d K%g Q%g G%g",
    v,
    voice_wave_table_index[v],
    voice_direction[v],
    osc[v].loop_enabled,
    voice_note[v],
    voice_freq[v],
    voice_user_amp[v],
    voice_pan[v],
    voice_cz_mode[v], voice_cz_distortion[v],
    voice_filter_mode[v], voice_filter_freq[v], voice_filter_res[v], voice_filter_gain[v]);
  ptr += n;
  if (0) {
    n = sprintf(ptr, " q%d", voice_quantize[v]);
    ptr += n;
  }
  if (voice_amp_mod_osc[v] >= 0 && voice_amp_mod_depth[v] > 0) {
    n = sprintf(ptr, " A%d,%g", voice_amp_mod_osc[v], voice_amp_mod_depth[v]);
    ptr += n;
  }
  if (voice_cz_mod_osc[v] >= 0 && voice_cz_mod_depth[v] > 0) {
    n = sprintf(ptr, " C%d,%g", voice_cz_mod_osc[v], voice_cz_mod_depth[v]);
    ptr += n;
  }
  if (voice_freq_mod_osc[v] >= 0 && voice_freq_mod_depth[v] > 0) {
    n = sprintf(ptr, " F%d,%g", voice_freq_mod_osc[v], voice_freq_mod_depth[v]);
    ptr += n;
  }
  if (voice_pan_mod_osc[v] >= 0 && voice_pan_mod_depth[v] > 0) {
    n = sprintf(ptr, " P%d,%g", voice_pan_mod_osc[v], voice_pan_mod_depth[v]);
    ptr += n;
  }
  n = sprintf(ptr, " m%d E%g,%g,%g,%g",
    voice_disconnect[v],
    voice_amp_envelope[v].a,
    voice_amp_envelope[v].d,
    voice_amp_envelope[v].s,
    voice_amp_envelope[v].r);
    ptr += n;
  if (0) {
    n = sprintf(ptr, " # %g/%g", osc[v].phase, osc[v].phase_inc);
    ptr += n;
  }
  if (0 && osc[v].one_shot) {
    n = sprintf(ptr, " %d/%g", osc[v].midi_note, osc[v].offset_hz);
    ptr += n;
  }
  return out;
}

void voice_show(int v, char c) {
  char s[1024];
  char e[8] = "";
  if (c != ' ') sprintf(e, " # *");
  voice_format(v, s);
  printf("# %s%s\n", s, e);
}

int voice_show_all(int voice) {
  for (int i=0; i<VOICE_MAX; i++) {
    if (voice_amp[i] == 0) continue;
    char t = ' ';
    if (i == voice) t = '*';
    voice_show(i, t);
  }
  return 0;
}

float midi2hz(float f);

pthread_t udp_thread_handle;
pthread_t scope_thread_handle;

void downsample_block_average_min_max(
  const float *source, int source_len, float *dest, int dest_len,
  float *min, float *max) {
  if (dest_len >= source_len) {
    // If dest is same size or larger, just copy
    for (int i = 0; i < dest_len && i < source_len; i++) {
      dest[i] = source[i];
      if (min) min[i] = source[i];
      if (max) max[i] = source[i];
    }
    return;
  }

  float block_size = (float)source_len / (float)dest_len;

  for (int i = 0; i < dest_len; i++) {
    float start = (float)i * block_size;
    float end = (float)(i + 1) * block_size;

    int start_idx = (int)start;
    int end_idx = (int)end;
    if (end_idx >= source_len) end_idx = source_len - 1;

    float sum = 0;
    int count = 0;

    float this_min = source[start_idx];
    float this_max = this_min;
    // Average all values in this block
    for (int j = start_idx; j <= end_idx; j++) {
      sum += source[j];
      count++;
      if (source[j] < this_min) this_min = source[j];
      if (source[j] > this_min) this_max = source[j];
    }
    if (min) min[i] = this_min;
    if (max) max[i] = this_max;
    dest[i] = (count > 0) ? sum / (float)count : 0;
  }
}

void downsample_block_average(const float *source, int source_len, float *dest, int dest_len) {
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
  FUNC_RESET,
  FUNC_FILTER_MODE,
  FUNC_FILTER_FREQ,
  FUNC_FILTER_RES,
  FUNC_FILTER_GAIN,
  FUNC_COPY,
  // subfunctions
  FUNC_HELP,
  FUNC_SEQ_START,
  FUNC_SEQ_STOP,
  FUNC_SEQ_PAUSE,
  FUNC_SEQ_RESUME,
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
  FUNC_WAVE_DEFAULT,
  FUNC_CZ,
  //
  FUNC_UNKNOWN,
};

char *display_func_func_str[FUNC_UNKNOWN+1] = {
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
  [FUNC_AMP_MOD] = "amp-mod",
  [FUNC_CZ_MOD] = "cz-mod",
  [FUNC_FREQ_MOD] = "freq-mod",
  [FUNC_PAN_MOD] = "pan-mod",
  [FUNC_MIDI] = "midi",
  [FUNC_WAVE] = "wave",
  [FUNC_LOOP] = "loop",
  [FUNC_DIR] = "dir",
  [FUNC_INTER] = "inter",
  [FUNC_PAN] = "pan",
  [FUNC_ENVELOPE] = "envelope",
  [FUNC_QUANT] = "quant",
  [FUNC_RESET] = "reset",
  [FUNC_FILTER_MODE] = "filter-mode",
  [FUNC_FILTER_FREQ] = "filter-freq",
  [FUNC_FILTER_GAIN] = "filter-gain",
  [FUNC_FILTER_RES] = "filter-q",
  [FUNC_COPY] = "copy-voice",
  [FUNC_UNKNOWN] = "unknown",
  //
  [FUNC_HELP] = "help",
  [FUNC_SEQ_START] = "seq-start",
  [FUNC_SEQ_STOP] = "seq-stop",
  [FUNC_SEQ_PAUSE] = "seq-pause",
  [FUNC_SEQ_RESUME] = "seq-resume",
  [FUNC_QUIT] = "quit",
  [FUNC_STATS0] = "stats-0",
  [FUNC_STATS1] = "stats-1",
  [FUNC_TRACE] = "trace",
  [FUNC_DEBUG] = "debug",
  [FUNC_SCOPE] = "scope",
  [FUNC_LOAD] = "load",
  [FUNC_SAVE] = "save",
  [FUNC_WAVE_SHOW] = "show-wave",
  [FUNC_DELAY] = "delay",
  [FUNC_COMMENT] = "comment",
  [FUNC_WHITESPACE] = "white-space",
  [FUNC_METRO] = "metro",
  [FUNC_WAVE_READ] = "wave-read",
  [FUNC_CZ] = "cz",
};

char *func_func_str(int n) {
  if (n >= 0 && n <= FUNC_UNKNOWN) {
    if (display_func_func_str[n]) {
      return display_func_func_str[n];
    }
  }
  return "no-string";
}

#include "miniwav.h"

void scope_wave_update(const float *table, int size) {
  scope_wave_len = 0;
  //float *table = osc[voice].table;
  //int size = osc[voice].table_size;
  downsample_block_average_min_max(table, size, scope_wave_data, SCOPE_WAVE_WIDTH, scope_wave_min, scope_wave_max);
  scope_wave_len = SCOPE_WAVE_WIDTH;
}

int freq_set(int v, float f);
int freq_midi(int voice, float f);
int amp_set(int v, float f);
int wave_set(int voice, int wave);
int wave_reset(int voice, int n);
int wave_default(int voice);
int wave_loop(int voice, int state);
int wave_mute(int voice, int state);
int wave_dir(int voice, int state);
int wave_load(int which, int where);
int wave_quant(int voice, int n);
int voice_set(int n, int *old_voice);
int voice_copy(int v, int n);
int voice_trigger(int voice);
int scope_start(int sub);
int amp_mod_set(int voice, int o, float f);
int freq_mod_set(int voice, int o, float f);
int pan_mod_set(int voice, int o, float f);
int patch_load(int voice, int n, int output);
int pan_set(int voice, float f);
int envelope_set(int voice, float a, float d, float s, float r);
int envelope_velocity(int voice, float f);
int wavetable_show(int n);
int cz_set(int v, int n, float f);
int cmod_set(int voice, int o, float f);

char *ignore = " \t\r\n;";

typedef struct {
  int func;
  int sub_func;
  int next;
  int argc;
  float args[8];
} value_t;

void dump(value_t v) {
  printf("# %s", func_func_str(v.func));
  if (v.sub_func != FUNC_NULL) printf(" %s", func_func_str(v.sub_func));
  printf(" [");
  for (int i=0; i<v.argc; i++) {
    if (i) printf(" ");
    printf("%g", v.args[i]);
  }
  puts("]");
}

void float_to_timespec(double seconds, int64_t *sec, int64_t *nano_sec) {
    double int_part;
    double frac = modf(seconds, &int_part);

    *sec  = (int64_t)int_part;
    *nano_sec = (int64_t)llround(frac * 1e9);

    // Normalize in case rounding pushed us to 1 second
    if (*nano_sec >= 1000000000) {
        *sec += 1;
        *nano_sec -= 1000000000;
    }
    if (*nano_sec < 0) {
        *sec -= 1;
        *nano_sec += 1000000000;
    }
}

void ms_to_timespec(int64_t ms, int64_t *sec, int64_t *ns) {
  if (sec == NULL || ns == NULL) return;
  *sec = ms / 1000;
  *ns = (ms % 1000) * 1000000L;
}

value_t parse_none(int func, int sub_func) {
  value_t v;
  v.func = func;
  v.sub_func = sub_func;
  v.argc = 0;
  v.next = 0;
  if (trace) dump(v);
  return v;
}

value_t parse(const char *ptr, int func, int sub_func, int argc) {
  value_t v;
  v.func = func;
  v.sub_func = sub_func;
  v.next = 0;
  int next[8];
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

int wire(char *line, int *this_voice, voice_stack_t *vs, int output) {
  size_t len = strlen(line);
  if (len == 0) return 0;

  char *ptr = line;
  char *max = line + len;

  value_t v;
  int voice = 0;
  if (this_voice) voice = *this_voice;

  int more = 1;
  int r = 0;

  char c;

  while (more) {
    if (ptr >= max) break;
    if (*ptr == '\0') break;
    // skip whitespace and semicolons
    ptr += strspn(ptr, ignore);
    if (debug) printf("# [%ld] '%c' (%d)\n", ptr-line, *ptr, *ptr);
    r = 0;
    switch (*ptr++) {
      case '[':
        voice_push(vs, (float)voice);
        continue;
      case ']':
        voice = (int)voice_pop(vs);
        continue;
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
              audio_show();
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
            v = parse_none(FUNC_SYS, FUNC_SCOPE);
            scope_start(*ptr);
            // sub x for scope_cross = 1
            // sub q for scope_quit = 0
            // sub 0..VOICE_MAX-1 for scope_channel = n
            // sub -1 for scope_channel = -1 (all channels)
            break;
          case 'l':
            // :l# load exp#.patch
            v = parse(ptr, FUNC_SYS, FUNC_LOAD, 1);
            {
              int which;
              if (v.argc == 1) {
                ptr += v.next;
                which = (int)v.args[0];
              } else return ERR_INVALID_PATCH;
              r = patch_load(voice, which, output);
              //if (r != 0) return r;
            }
            break;
          case 'w':
            // :w#,# load wave#.wav into wave slot #
            v = parse(ptr, FUNC_SYS, FUNC_WAVE_READ, 2);
            {
              int which;
              int where;
              if (v.argc == 2) {
                ptr += v.next;
                which = (int)v.args[0];
                where = (int)v.args[1];
              } else if (v.argc == 1) {
                ptr += v.next;
                which = (int)v.args[0];
                where = EXT_SAMPLE_00;
              } else return ERR_INVALID_EXT_SAMPLE;
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
          if (v.args[0] >= 0 && v.args[0] <= 15) sleep_float(v.args[0]);
        }
        break;
      case 'c':
        v = parse(ptr, FUNC_CZ, FUNC_NULL, 2);
        if (v.argc == 2) {
          ptr += v.next;
          r = cz_set(voice, (int)v.args[0], v.args[1]);
        }
        break;
      case 'C':
        v = parse(ptr, FUNC_CZ, FUNC_NULL, 2);
        if (v.argc == 2) {
          ptr += v.next;
          r = cmod_set(voice, (int)v.args[0], v.args[1]);
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
          r = wave_quant(voice, (int)v.args[0]);
        } else return ERR_INVALID_QUANT;
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
          r = voice_set((int)v.args[0], &voice);
          if (output) {
            console_voice = voice;
          }
        } else return ERR_INVALID_VOICE;
        break;
      case '>':
        v = parse(ptr, FUNC_COPY, FUNC_NULL, 1);
        if (v.argc == 1) {
          ptr += v.next;
          r = voice_copy(voice, (int)v.args[0]);
        } else return ERR_INVALID_VOICE;
        break;
      case 'w':
        v = parse(ptr, FUNC_WAVE, FUNC_NULL, 1);
        if (v.argc == 1) {
          ptr += v.next;
          r = wave_set(voice, (int)v.args[0]);
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
        v = parse(ptr, FUNC_WAVE_SHOW, FUNC_NULL, 1);
        if (v.argc == 1) {
          ptr += v.next;
          r = wavetable_show((int)v.args[0]);
        } else return ERR_INVALID_WAVETABLE;
        break;
      case 'M':
        v = parse(ptr, FUNC_METRO, FUNC_NULL, 2);
        if (v.argc == 2) {
          ptr += v.next;
          tick_max_set(v.args[0], v.args[1]);
        } else return ERR_INVALID_MOD;
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
      case 'n':
        v = parse(ptr, FUNC_MIDI, FUNC_NULL, 1);
        if (v.argc == 1) {
          ptr += v.next;
          r = freq_midi(voice, v.args[0]);
        } else return ERR_INVALID_MIDI_NOTE;
        break;
      case 'A':
        v = parse(ptr, FUNC_AMP_MOD, FUNC_NULL, 2);
        if (v.argc == 2) {
          ptr += v.next;
          r = amp_mod_set(voice, (int)v.args[0], v.args[1]);
        } else if (v.argc == 1) {
          ptr += v.next;
          r = amp_mod_set(voice, -1, 0);
        } else return ERR_PARSING_ERROR;
        break;
      case 'J':
        v = parse(ptr, FUNC_FILTER_MODE, FUNC_NULL, 2);
        if (v.argc == 1) {
          ptr += v.next;
          voice_filter_mode[voice] = (int)v.args[0];
          mmf_set_params(voice,
            voice_filter_freq[voice],
            voice_filter_res[voice],
            voice_filter_gain[voice]);
        } else return ERR_PARSING_ERROR;
        break;
      case 'K':
        v = parse(ptr, FUNC_FILTER_FREQ, FUNC_NULL, 2);
        if (v.argc == 1) {
          ptr += v.next;
          if (v.args[0] > 0) {
            mmf_set_freq(voice, v.args[0]);
            r = 0;
          }
        } else return ERR_PARSING_ERROR;
        break;
      case 'Q':
        v = parse(ptr, FUNC_FILTER_RES, FUNC_NULL, 2);
        if (v.argc == 1) {
          ptr += v.next;
          if (v.args[0] > 0) {
            mmf_set_res(voice, v.args[0]);
            r = 0;
          }
        } else return ERR_PARSING_ERROR;
        break;
      case 'G':
        v = parse(ptr, FUNC_FILTER_GAIN, FUNC_NULL, 2);
        if (v.argc == 1) {
          ptr += v.next;
          if (v.args[0] > 0) {
            mmf_set_gain(voice, v.args[0]);
            r = 0;
          }
        } else return ERR_PARSING_ERROR;
        break;
      case 'l':
        v = parse(ptr, FUNC_VELOCITY, FUNC_NULL, 1);
        if (v.argc == 1) {
          ptr += v.next;
          r = envelope_velocity(voice, v.args[0]);
        } else return ERR_PARSING_ERROR;
        break;
      case 'E':
        v = parse(ptr, FUNC_ENVELOPE, FUNC_NULL, 4);
        if (v.argc == 4) {
          ptr += v.next;
          r = envelope_set(voice, v.args[0], v.args[1], v.args[2], v.args[3]);
        } else return ERR_PARSING_ERROR;
        break;
      case 'S':
        v = parse(ptr, FUNC_RESET, FUNC_NULL, 1);
        if (v.argc == 1) {
          ptr += v.next;
          r = wave_reset(voice, (int)v.args[0]);
        } else return ERR_PARSING_ERROR;
        break;
      case 'F':
        v = parse(ptr, FUNC_FREQ_MOD, FUNC_NULL, 2);
        if (v.argc == 2) {
          ptr += v.next;
          r = freq_mod_set(voice, (int)v.args[0], v.args[1]);
        } else if (v.argc == 1) {
          ptr += v.next;
          r = freq_mod_set(voice, -1, 0);
        } else return ERR_PARSING_ERROR;
        break;
      case 'P':
        v = parse(ptr, FUNC_PAN_MOD, FUNC_NULL, 2);
        if (v.argc == 2) {
          ptr += v.next;
          r = pan_mod_set(voice, (int)v.args[0], v.args[1]);
        } else if (v.argc == 1) {
          ptr += v.next;
          r = pan_mod_set(voice, -1, 0);
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

void tick_max_set(float m, float n) {
  tick_max = m;
  tick_inc = n;
}

void tick(int frames) {
  tick_frames = frames;
  static float i = 0;
  static int j = 0;
  if (i == 0) {
    if (j&1) voice_trigger(21);
    else voice_trigger(20);
    j++;
  }
  i += tick_inc;
  if (i >= tick_max) i = 0;
  //int voice = 20;
  //wire("v20T", &voice, NULL, 0);
}

int main(int argc, char *argv[]) {
  int load_patch_number = -1;
  if (argc > 1) {
    for (int i=1; i<argc; i++) {
      if (argv[i][0] == '-') {
        switch (argv[i][1]) {
          case 'd': debug = 1; break;
          case 'p': if ((i+i) < argc) { udp_port = (int)strtol(argv[i+1], NULL, 10); i++; } break;
          case 'l': if ((i+i) < argc) { load_patch_number = (int)strtol(argv[i+1], NULL, 10); i++; } break;
          default:
            printf("# unknown switch '%s'\n", argv[i]);
            exit(1);
            break;
        }
      }
    }
  }
  show_threads();
  linenoiseHistoryLoad(HISTORY_FILE);
  wave_table_init();
  voice_init();

  // Initialize Sokol Audio
  saudio_setup(&(saudio_desc){
    // .stream_cb = stream_cb,
    .stream_userdata_cb = engine,
    .user_data = &my_data,
    .sample_rate = MAIN_SAMPLE_RATE,
    .num_channels = CHANNEL_NUM, // Stereo
    .buffer_frames = 512,
    //.packet_frames = 256,
    //.num_packets = 1,
    // .user_data = &my_data, // todo
  });

  if (audio_show() != 0) return 1;

  pthread_setname_np(pthread_self(), "skred-main");

  pthread_create(&udp_thread_handle, NULL, udp_main, NULL);
  pthread_detach(udp_thread_handle);

  voice_stack_t vs;
  vs.ptr = 0;

  if (load_patch_number >= 0) patch_load(0, load_patch_number, 0);

  while (main_running) {
    char *line = linenoise("# ");
    if (line == NULL) {
      main_running = 0;
      break;
    }
    if (strlen(line) == 0) continue;
    linenoiseHistoryAdd(line);
    int n = wire(line, &current_voice, &vs, 1);
    if (n < 0) break; // request to stop or error
    if (n > 0) {
      char *s = err_str(n);
      printf("# %s ERR:%d\n", s, n);
    }
    linenoiseFree(line);
  }
  linenoiseHistorySave(HISTORY_FILE);

  // Cleanup
  saudio_shutdown();

  udp_running = 0;
  scope_running = 0;

  sleep(1); // make sure we don't crash the callback b/c thread timing and wave_data

  wave_free();

  show_threads();

  return 0;
}

#define SIZE_SINE (4096)
#include "retro/korg.h"

#include "amysamples.h"

void normalize_preserve_zero(float *data, int length) {
  if (length == 0) return;

  // Find the maximum absolute value
  float max_abs = 0.0f;
  for (int i = 0; i < length; i++) {
    float abs_val = fabsf(data[i]);
    if (abs_val > max_abs) {
      max_abs = abs_val;
    }
  }

  // Avoid division by zero
  if (max_abs == 0.0) {
    return;  // All values are zero, nothing to normalize
  }

  // Scale all values by the same factor
  float scale_factor = 1.0f / max_abs;
  for (int i = 0; i < length; i++) {
    data[i] *= scale_factor;
  }
}

//
#include <stdint.h>

typedef struct {
    uint64_t state;
} AudioRNG;

// Initialize the RNG with a seed
void audio_rng_init(AudioRNG* rng, uint64_t seed) {
    rng->state = seed ? seed : 1; // Ensure non-zero seed
}

// Generate next random number (full 64-bit range)
uint64_t audio_rng_next(AudioRNG* rng) {
    // High-quality LCG parameters (Knuth's MMIX)
    rng->state = rng->state * 6364136223846793005ULL + 1442695040888963407ULL;
    return rng->state;
}

// Generate random float in range [-1.0, 1.0] for audio
float audio_rng_float(AudioRNG* rng) {
    uint64_t raw = audio_rng_next(rng);
    // Use upper 32 bits for better quality
    uint32_t val = (uint32_t)(raw >> 32);
    // Convert to signed float [-1.0, 1.0]
    return (float)((int32_t)val) / 2147483648.0f;
}

// Generate random float in range [0.0, 1.0]
float audio_rng_uniform(AudioRNG* rng) {
    uint64_t raw = audio_rng_next(rng);
    uint32_t val = (uint32_t)(raw >> 32);
    return (float)val / 4294967296.0f;
}

// Generate random float in custom range [min, max]
float audio_rng_range(AudioRNG* rng, float min, float max) {
    return min + audio_rng_uniform(rng) * (max - min);
}

// Example usage for white noise generation
void generate_white_noise(AudioRNG* rng, float* buffer, int n) {
    for (int i = 0; i < n; i++) {
        buffer[i] = audio_rng_float(rng);
    }
}

// Example usage for pink noise (simple approximation)
typedef struct {
    AudioRNG rng;
    float b0, b1, b2, b3, b4, b5, b6;
} PinkNoiseGen;

void pink_noise_init(PinkNoiseGen* gen, uint64_t seed) {
    audio_rng_init(&gen->rng, seed);
    gen->b0 = gen->b1 = gen->b2 = gen->b3 = gen->b4 = gen->b5 = gen->b6 = 0.0f;
}

float pink_noise_sample(PinkNoiseGen* gen) {
    float white = audio_rng_float(&gen->rng);
    gen->b0 = 0.99886f * gen->b0 + white * 0.0555179f;
    gen->b1 = 0.99332f * gen->b1 + white * 0.0750759f;
    gen->b2 = 0.96900f * gen->b2 + white * 0.1538520f;
    gen->b3 = 0.86650f * gen->b3 + white * 0.3104856f;
    gen->b4 = 0.55000f * gen->b4 + white * 0.5329522f;
    gen->b5 = -0.7616f * gen->b5 - white * 0.0168980f;
    float pink = gen->b0 + gen->b1 + gen->b2 + gen->b3 + gen->b4 + gen->b5 + gen->b6 + white * 0.5362f;
    gen->b6 = white * 0.115926f;
    return pink * 0.11f; // Scale to reasonable amplitude
}
//

void wave_table_init(void) {
  float *table;

  for (int i = 0 ; i < WAVE_TABLE_MAX; i++) {
    wave_table_data[i] = NULL;
    wave_size[i] = 0;
  }

  AudioRNG pink;
  audio_rng_init(&pink, 1);
  for (int w = WAVE_TABLE_SINE; w <= WAVE_TABLE_NOISE; w++) {
    int size = SIZE_SINE;
    char *name = "?";
    switch (w) {
      case WAVE_TABLE_SINE:  name = "sine"; break;
      case WAVE_TABLE_SQR:   name = "square"; break;
      case WAVE_TABLE_SAW_DOWN: name = "saw-down"; break;
      case WAVE_TABLE_SAW_UP: name = "saw-up"; break;
      case WAVE_TABLE_TRI:   name = "triangle"; break;
      case WAVE_TABLE_NOISE: name = "noise"; break;
      default: name = "?"; break;
    }
    printf("# make w%d %s\n", w, name);
    wave_table_data[w] = (float *)malloc(size * sizeof(float));
    wave_size[w] = size;
    wave_rate[w] = MAIN_SAMPLE_RATE;
    wave_one_shot[w] = 0;
    wave_loop_start[w] = 0;
    wave_loop_end[w] = size-1;
    int off = 0;
    float phase = 0;
    float delta = 1.0f / (float)size;
    while (phase < 1.0f) {
      float sine = sinf(2.0f * (float) M_PI * phase);
      float f;
      switch (w) {
        case WAVE_TABLE_SINE: f = sine; break;
        case WAVE_TABLE_SQR: f = (phase < 0.5) ? 1.0f : -1.0f; break;
        case WAVE_TABLE_SAW_DOWN: f = 2.0f * phase - 1.0f; break;
        case WAVE_TABLE_SAW_UP: f = 1.0f - 2.0f * phase; break;
        case WAVE_TABLE_TRI: f = (phase < 0.5f) ? (4.0f * phase - 1.0f) : (3.0f - 4.0f * phase); break;
        case WAVE_TABLE_NOISE: f = audio_rng_float(&pink); break;
        default: f = 0; break;
      }
      wave_table_data[w][off++] = f;
      phase += delta;
    }
  }

  printf("# load retro waves (%d to %d)\n", WAVE_TABLE_KRG1, WAVE_TABLE_KRG32-1);

  korg_init();

  for (int i = WAVE_TABLE_KRG1; i < WAVE_TABLE_KRG32; i++) {
    int k = i - WAVE_TABLE_KRG1;
    int s = kwave_size[k];
    table = malloc(s * sizeof(float));
    for (int j = 0 ; j < s; j++) {
      table[j] = (float)kwave[k][j] / (float)32767;
    }
    wave_table_data[i] = table;
    wave_size[i] = s;
    wave_rate[i] = MAIN_SAMPLE_RATE;
    wave_one_shot[i] = 0;
    wave_loop_start[i] = 0;
    wave_loop_end[i] = s-1;
  }

  // load AMY samples
  int j = AMY_SAMPLE_99;
  for (int i = 0; i < PCM_SAMPLES; i++) {
    j = i + AMY_SAMPLE_00;
    if (j > AMY_SAMPLE_99-1) {
      printf("# too many PCM samples... exit early\n");
      break;
    }
    if (debug) printf("[%d] offset:%d length:%d loopstart:%d loopend:%d midi_note:%d offset_hz:%g\n",
      j, pcm_map[i].offset, pcm_map[i].length, pcm_map[i].loopstart, pcm_map[i].loopend,
      pcm_map[i].midinote, midi2hz(pcm_map[i].midinote));
    table = malloc(pcm_map[i].length * sizeof(float));
    for (int k = 0; k < pcm_map[i].length; k++) {
      table[k] = (float)pcm[pcm_map[i].offset + k] / 32767.0f;
    }
    normalize_preserve_zero(table, (int)pcm_map[i].length);
    wave_table_data[j] = table;
    wave_size[j] = (int)pcm_map[i].length;
    wave_rate[j] = PCM_AMY_SAMPLE_RATE;
    wave_one_shot[j] = 1;
    wave_loop_enabled[j] = 0;
    wave_loop_start[j] = (int)pcm_map[i].loopstart;
    wave_loop_end[j] = (int)pcm_map[i].loopend;
    wave_midi_note[j] = (int)pcm_map[i].midinote;
    wave_offset_hz[j] = midi2hz((float)pcm_map[i].midinote);
  }
  printf("# load AMY samples (%d to %d)\n", AMY_SAMPLE_00, j);
}

void wave_free(void) {
  for (int i = 0; i < WAVE_TABLE_MAX; i++) {
    if (wave_table_data[i]) {
      if (debug) printf("[%d] freeing...\n", i);
      free(wave_table_data[i]);
      wave_size[i] = 0;
    }
  }
}

void voice_reset(int i) {
  voice_sample[i] = 0;
  voice_amp[i] = 0;
  voice_pan[i] = 0;
  voice_pan_left[i] = 0.5f;
  voice_pan_right[i] = 0.5f;
  voice_use_amp_envelope[i] = 0;
  voice_amp_mod_osc[i] = -1;
  voice_freq_mod_osc[i] = -1;
  voice_pan_mod_osc[i] = -1;
  voice_disconnect[i] = 0;
  voice_quantize[i] = 0;
  voice_direction[i] = 0;
  envelope_init(i, 1.1f, 0.2f, 0.7f, 0.5f, 44100.0f);
  voice_freq[i] = 440.0f;
  osc_set_wt(i, WAVE_TABLE_SINE);
  mmf_init(i, 8000.0f, 0.707f, 1.0f);
  voice_filter_mode[i] = 0;
}

void voice_init(void) {
  for (int i=0; i<VOICE_MAX; i++) {
    voice_reset(i);
  }
}

#include <arpa/inet.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>

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

void *udp_main(void *arg) {
  int sock = udp_open(udp_port);
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
  voice_stack_t vs;
  vs.ptr = 0;
  fd_set readfds;
  struct timeval timeout;
  while (udp_running) {
    FD_ZERO(&readfds);
    FD_SET(sock, &readfds);
    timeout.tv_sec = 1;
    timeout.tv_usec = 0;
    int ready = select(sock+1, &readfds, NULL, NULL, &timeout);
    if (ready > 0 && FD_ISSET(sock, &readfds)) {
      ssize_t n = recvfrom(sock, line, sizeof(line), 0, (struct sockaddr *)&client, &client_len);
      if (n > 0) {
        line[n] = '\0';
        // printf("# from %d\n", ntohs(client.sin_port)); // port
        wire(line, &voice, &vs, 0);
      } else {
        if (errno == EAGAIN) continue;
      }
    } else if (ready == 0) {
      // timeout
    } else {
      perror("# select");
    }
  }
  if (debug) printf("# udp stopping\n");
  return NULL;
}

int find_starting_point(int buffer_snap, int sw) {
  int j = buffer_snap - sw;
  float t0 = (scope_buffer_left[j] + scope_buffer_right[j]) / 2.0f;
  int i = j-1;
  int c = 1;
  while (c < SCOPE_LEN) {
    if (i < 0) i = SCOPE_LEN-1;
    float t1 = (scope_buffer_left[i] + scope_buffer_right[i]) / 2.0f;
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

void *scope_main(void *arg) {
  pthread_setname_np(pthread_self(), "skred-scope");
#ifndef USE_RAYLIB
  while (scope_running) {
    sleep(1);
  }
#else
  int tsize = 10;
#include "raylib.h"
#include "rlgl.h"
  //
  Vector2 position_in;
  FILE *file = fopen(CONFIG_FILE, "r");
  const int screenWidth = SCREENWIDTH;
  const int screenHeight = SCREENHEIGHT;
  float mag_x = 1.0f;
  float sw = (float)screenWidth;
  if (file != NULL) {
    /*
     file contents
    x y scope_display_mag mag_x
    */
    fscanf(file, "%f %f %f %f", &position_in.x, &position_in.y, &scope_display_mag, &mag_x);
    if (position_in.x < 0) position_in.x = 0;
    if (position_in.y < 0) position_in.y = 0;
    if (scope_display_mag <= 0) scope_display_mag = 1;
    // if (mag_x <= 0) mag_x = 1;
    fclose(file);
  } else {
    printf("# %s read fopen fail\n", CONFIG_FILE);
  }
  //
  SetTraceLogLevel(LOG_NONE);
  InitWindow(screenWidth, screenHeight, "skred-scope");
  //
  SetWindowPosition((int)position_in.x, (int)position_in.y);
  //
  Vector2 dot = { (float)screenWidth/2, (float)screenHeight/2 };
  SetTargetFPS(12);
  float sh = (float)screenHeight;
  float h0 = (float)screenHeight / 2.0f;
  char osd[1024] = "?";
  int osd_dirty = 1;
  float y = h0;
  float a = 1.0f;
  int show_l = 1;
  int show_r = 1;
  Color color_left = {0, 255, 255, 128};
  Color color_right = {255, 255, 0, 128};
  Color green0 = {0, 255, 0, 128};
  Color green1 = {0, 128, 0, 128};
  while (scope_running && !WindowShouldClose()) {
    if (mag_x <= 0) mag_x = MAG_X_INC;
    sw = (float)screenWidth / mag_x;
    float mw = (float)SCOPE_LEN / 2.0f;
    if (sw >= mw) sw = mw;
    int shifted = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
    if (IsKeyDown(KEY_ONE)) {
      if (show_l == 1) show_l = 0; else show_l = 1;
    }
    if (IsKeyDown(KEY_TWO)) {
      if (show_r == 1) show_r = 0; else show_r = 1;
    }
    if (IsKeyDown(KEY_T)) {
      if (shifted) {
        tsize -= 1;
        if (tsize < 10) tsize = 10;
      } else {
        tsize += 1;
        if (tsize > 20) tsize = 20;
      }
    }
    if (IsKeyDown(KEY_A)) {
      if (shifted) {
        scope_display_mag -= 0.1f;
        a -= 0.1f;
      } else {
        scope_display_mag += 0.1f;
        a += 0.1f;
      }
      osd_dirty++;
    }
    if (IsKeyDown(KEY_RIGHT)) {
      mag_x += (float)MAG_X_INC;
      osd_dirty++;
    }
    if (IsKeyDown(KEY_LEFT)) {
      mag_x -= (float)MAG_X_INC;
      osd_dirty++;
    }
    if (IsKeyDown(KEY_UP)) {
      y += 1.0f;
      osd_dirty++;
    }
    if (IsKeyDown(KEY_DOWN)) {
      y -= 1.0f;
      osd_dirty++;
    }
    if (osd_dirty) {
      sprintf(osd, "mag_x:%g sw:%g y:%g a:%g mag:%g ch:%d",
        mag_x, sw, y, a, scope_display_mag, scope_channel);
    }
    int buffer_snap = scope_buffer_pointer;
    BeginDrawing();
    ClearBackground(BLACK);
    for (int i = 0; i < scope_wave_len; i++) {
      int mh = SCOPE_WAVE_HEIGHT / 2;
      float min = (scope_wave_min[i] / 2.0f + 0.5f) * (float)mh;
      float max = (scope_wave_max[i] / 2.0f + 0.5f) * (float)mh;
      int qh = SCOPE_WAVE_HEIGHT / 4;
      DrawLine(i, (int)max, i, qh, green1);
      DrawLine(i, (int)min, i, qh, green1);
      dot.x = (float)i;
      dot.y = max; DrawCircleV(dot, 1, green0);
      dot.y = min; DrawCircleV(dot, 1, green0);
    }
    // show a wave table
    if (scope_wave_len) {
      char s[32];
      sprintf(s, "%d", scope_wave_index);
      DrawText(s, 10, 10, 20, YELLOW);
    }
    // find starting point for display
    int start = find_starting_point(buffer_snap, screenWidth);
    int actual = 0;
    float x_offset = (float)SCREENWIDTH / 8.0f;
    start -= (int)x_offset;
    DrawText(osd, 10, SCREENHEIGHT-20, 20, GREEN);
    rlPushMatrix();
    rlTranslatef(0.0f, y, 0.0f); // x,y,z
    rlScalef(mag_x,scope_display_mag,1.0f);
      DrawLine(0, 0, (int)sw, 0, DARKGREEN);
      if (start >= scope_len) start = 0;
      actual = start;
      for (int i = 0; i < (int)sw; i++) {
        if (actual == 0) DrawLine(i, (int)-sh, i, (int)sh, YELLOW);
        if (actual == buffer_snap) DrawLine(i, (int)-sh, i, (int)sh, BLUE);
        if (actual >= (SCOPE_LEN-1)) {
          DrawLine(i, (int)-sh, i, (int)sh, RED);
          actual = 0;
        }
        dot.x = (float)i;
        if (show_l) {
          dot.y = scope_buffer_left[actual] * h0 * scope_display_mag;
          DrawCircleV(dot, 1, color_right);
        }
        if (show_r) {
          dot.y = scope_buffer_right[actual] * h0 * scope_display_mag;
          DrawCircleV(dot, 1, color_left);
        }
        actual++;
      }
    rlPopMatrix();
    sprintf(osd, "M%g,%g %d:%ld", tick_max, tick_inc, tick_frames, sample_count);
    DrawText(osd, SCREENWIDTH-250, SCREENHEIGHT-20, 20, BLUE);
    voice_format(console_voice, osd);
    DrawText(osd, 10, SCREENHEIGHT-40, tsize, YELLOW);
    EndDrawing();
  }
  //
  file = fopen(CONFIG_FILE, "w");
  if (file != NULL) {
    Vector2 position_out = GetWindowPosition();
    fprintf(file, "%g %g %g %g", position_out.x, position_out.y, scope_display_mag, mag_x);
    fclose(file);
  } else {
    printf("# %s write fopen fail\n", CONFIG_FILE);
  }
  //
  CloseWindow();
  scope_running = 0;
#endif
  if (debug) printf("# scope stopping\n");
  return NULL;
}

float midi2hz(float f) {
  float g = 440.0f * powf(2.0f, (f - 69.0f) / 12.0f);
  return g;
}

int freq_set(int voice, float f) {
  if (f > 0 && f < (double)MAIN_SAMPLE_RATE) {
    voice_freq[voice] = f;
    osc_set_freq(voice, f);
    return 0;
  }
  return ERR_FREQUENCY_OUT_OF_RANGE;
}

int amp_set(int voice, float f) {
  if (f >= 0) {
    voice_use_amp_envelope[voice] = 0;
    voice_amp[voice] = f * AMY_FACTOR;
    voice_user_amp[voice] = f;
  } else return ERR_AMPLITUDE_OUT_OF_RANGE;
  return 0;
}

int wave_set(int voice, int wave) {
  if (wave >= 0 && wave < WAVE_TABLE_MAX) {
    scope_wave_index = wave;
    osc_set_wt(voice, wave);
    scope_wave_update(osc[voice].table, osc[voice].table_size);
  } else return ERR_INVALID_WAVE;
  return 0;
}

int wave_reset(int voice, int n) {
  if (n >= 0 && n < VOICE_MAX) {
    voice_reset(n);
  } else {
    voice_init();
  }
  return 0;
}

int wave_default(int voice) {
  float g = midi2hz((float)osc[voice].midi_note);
  voice_freq[voice] = g;
  voice_note[voice] = (float)osc[voice].midi_note;
  osc_set_freq(voice, g);
  scope_wave_update(osc[voice].table, osc[voice].table_size);
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
    if (voice_disconnect[voice] == 0) state = 1;
    else state = 0;
  }
  voice_disconnect[voice] = state;
  return 0;
}

int wave_dir(int voice, int state) {
  if (state < 0) {
    if (voice_direction[voice] == 0) state = 1;
    else state = 0;
  }
  voice_direction[voice] = state;
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

int scope_start(int sub) {
  if (scope_running == 0) {
    scope_running = 1;
    pthread_create(&scope_thread_handle, NULL, scope_main, NULL);
    pthread_detach(scope_thread_handle);
  }
  return 0;
}

int wave_load(int which, int where) {
  if (where < EXT_SAMPLE_00 || where >= EXT_SAMPLE_99) return ERR_INVALID_EXT_SAMPLE;
  char name[1024];
  sprintf(name, "wave%d.wav", which);
  wav_t wav;
  int len;
  float *table = mw_get(name, &len, &wav);
  if (table == NULL) {
    printf("# can not read %s\n", name);
    return ERR_INVALID_EXT_SAMPLE;
  } else {
    if (wave_table_data[where]) {
      if (save_wave_ptr >= SAVE_WAVE_LEN) {
        save_wave_ptr = 0;
      }
      if (save_wave_list[save_wave_ptr]) {
        printf("# freeing old wave %d\n", save_wave_ptr);
        free(save_wave_list[save_wave_ptr]);
      }
      save_wave_list[save_wave_ptr++] = wave_table_data[where];
    }
    wave_table_data[where] = table;
    wave_size[where] = len;
    wave_rate[where] = (float)wav.SamplesRate;
    wave_one_shot[where] = 1;
    wave_loop_enabled[where] = 0;
    wave_loop_start[where] = 1;
    wave_loop_end[where] = len;
    wave_midi_note[where] = 69;
    wave_offset_hz[where] = (float)len / (float)wav.SamplesRate * 440.0f;
    printf("# read %d frames from %s to %d (ch:%d sr:%d)\n",
      len, name, where, wav.Channels, wav.SamplesRate);
  }
  return 0;
}

int amp_mod_set(int voice, int o, float f) {
  if (voice < 0 && voice >= VOICE_MAX) return ERR_INVALID_VOICE;
  if (o < 0 && o >= VOICE_MAX) return ERR_INVALID_VOICE;
  voice_amp_mod_osc[voice] = o;
  voice_amp_mod_depth[voice] = f;
  return 0;
}

int freq_mod_set(int voice, int o, float f) {
  if (voice < 0 && voice >= VOICE_MAX) return ERR_INVALID_VOICE;
  if (o < 0 && o >= VOICE_MAX) return ERR_INVALID_VOICE;
  voice_freq_mod_osc[voice] = o;
  voice_freq_mod_depth[voice] = f;
  return 0;
}

int pan_mod_set(int voice, int o, float f) {
  if (voice < 0 && voice >= VOICE_MAX) return ERR_INVALID_VOICE;
  if (o < 0 && o >= VOICE_MAX) return ERR_INVALID_VOICE;
  voice_pan_mod_osc[voice] = o;
  voice_pan_mod_depth[voice] = f;
  return 0;
}

int patch_load(int voice, int n, int output) {
  char file[1024];
  sprintf(file, "exp%d.patch", n);
  FILE *in = fopen(file, "r");
  int r = 0;
  voice_stack_t vs;
  vs.ptr = 0;
  if (in) {
    char line[1024];
    while (fgets(line, sizeof(line), in) != NULL) {
      size_t len = strlen(line);
      if (len > 0 && line[len-1] == '\n') line[len-1] = '\0';
      if (output) printf("# %s\n", line);
      int temp_voice = voice;
      r = wire(line, &temp_voice, &vs, trace);
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

int wave_quant(int voice, int n) {
  voice_quantize[voice] = n;
  return 0;
}

int pan_set(int voice, float f) {
  if (f >= -1.0f && f <= 1.0f) {
    voice_pan[voice] = f;
    voice_pan_left[voice] = (1.0f - f) / 2.0f;
    voice_pan_right[voice] = (1.0f + f) / 2.0f;
  } else {
    return ERR_PAN_OUT_OF_RANGE;
  }
  return 0;
}

int envelope_set(int voice, float a, float d, float s, float r) {
  envelope_init(voice, a, d, s, r, MAIN_SAMPLE_RATE);
  return 0;
}

int envelope_velocity(int voice, float f) {
  if (f == 0) {
    amp_envelope_release(voice);
  } else {
    voice_use_amp_envelope[voice] = 1;
    if (osc[voice].one_shot) {
      osc_trigger(voice);
    }
    amp_envelope_trigger(voice, f);
  }
  return 0;
}

int wavetable_show(int n) {
  if (n >= 0 && n < WAVE_TABLE_MAX && wave_table_data[n] && wave_size[n]) {
    scope_wave_index = n;
    float *table = wave_table_data[n];
    int size = wave_size[n];
    int crossing = 0;
    int zero = 0;
    float ttl = 0;
    float min = table[0];
    float max = table[0];
    for (int i = 1; i < size; i++) {
      if (table[i] < min) min = table[i];
      if (table[i] > max) max = table[i];
      ttl += table[i];
      if (table[i-1] == 0.0 || table[i] == 0.0) {
        // Prevent ambiguity with multiple zeroes
        zero++;
      } else if ((table[i-1] > 0 && table[i] < 0) || (table[i-1] < 0 && table[i] > 0)) {
        // Check for sign change
        crossing++;
      }
    }
    printf("# w%d size:%d", n, size);
    printf(" +hz:%g midi:%d", wave_offset_hz[n], wave_midi_note[n]);
    puts("");
    downsample_block_average_min_max(table, size, scope_wave_data, SCOPE_WAVE_WIDTH, scope_wave_min, scope_wave_max);
    scope_wave_len = SCOPE_WAVE_WIDTH;
  }
  return 0;
}

int cz_set(int v, int n, float f) {
  voice_cz_mode[v] = n;
  voice_cz_distortion[v] = f;
  return 0;
}

int cmod_set(int voice, int o, float f) {
  voice_cz_mod_osc[voice] = o;
  voice_cz_mod_depth[voice] = f;
  return 0;
}

// Initialize the filter with frequency and resonance
// freq: cutoff frequency in Hz
// resonance: resonance factor (0.1 to 10.0, where 0.707 is no resonance)
// sample_rate: audio sample rate in Hz
void mmf_init(int n, float f, float resonance, float gain) {
    // Clear delay lines
    voice_filter[n].x1 = voice_filter[n].x2 = 0.0f;
    voice_filter[n].y1 = voice_filter[n].y2 = 0.0f;

    // Store parameters
    voice_filter[n].last_freq = -1.0f;  // Force coefficient calculation
    voice_filter[n].last_resonance = -1.0f;
    voice_filter[n].last_gain = -999.0f;
    voice_filter[n].last_mode = -1;

    voice_filter_freq[n] = f;
    voice_filter_res[n] = resonance;
    voice_filter_gain[n] = gain;

    // Calculate initial coefficients
    mmf_set_params(n, f, resonance, gain);
}

enum {
  FILTER_LOWPASS = 1,
  FILTER_HIGHPASS = 2,
  FILTER_BANDPASS = 3,
  FILTER_NOTCH = 4,
  FILTER_ALL_PASS = 5,
  FILTER_PEAKING = 6,
  FILTER_LOW_SHELF = 7,
  FILTER_HIGH_SHELF = 8,
};


// Set parameters - only recalculates coefficients if values changed
void mmf_set_params(int n, float f, float resonance, float gain) {
    // Only recalculate if parameters changed
    if (
      f == voice_filter[n].last_freq &&
      resonance == voice_filter[n].last_resonance &&
      gain == voice_filter[n].last_gain &&
      voice_filter_mode[n] == voice_filter[n].last_mode) {
        return;  // No work needed!
    }

    voice_filter[n].last_freq = f;
    voice_filter[n].last_resonance = resonance;
    voice_filter[n].last_gain = gain;
    voice_filter[n].last_mode = voice_filter_mode[n];

    // Calculate filter coefficients (expensive operations only done here)
    float omega = 2.0f * (float)M_PI * f / (float)MAIN_SAMPLE_RATE;
    float sin_omega = sinf(omega);
    float cos_omega = cosf(omega);
    float alpha = sin_omega / (2.0f * resonance);

    float a0, b0, b1, b2, a1, a2;

    switch (voice_filter_mode[n]) {
      case 0:
        return;
      default:
      case FILTER_LOWPASS:
          b0 = (1.0f - cos_omega) / 2.0f;
          b1 = 1.0f - cos_omega;
          b2 = (1.0f - cos_omega) / 2.0f;
          a0 = 1.0f + alpha;
          a1 = -2.0f * cos_omega;
          a2 = 1.0f - alpha;
          break;
      case FILTER_HIGHPASS:
          b0 = (1.0f + cos_omega) / 2.0f;
          b1 = -(1.0f + cos_omega);
          b2 = (1.0f + cos_omega) / 2.0f;
          a0 = 1.0f + alpha;
          a1 = -2.0f * cos_omega;
          a2 = 1.0f - alpha;
          break;

      case FILTER_BANDPASS:
          b0 = alpha;
          b1 = 0.0f;
          b2 = -alpha;
          a0 = 1.0f + alpha;
          a1 = -2.0f * cos_omega;
          a2 = 1.0f - alpha;
          break;

      case FILTER_NOTCH:
          b0 = 1.0f;
          b1 = -2.0f * cos_omega;
          b2 = 1.0f;
          a0 = 1.0f + alpha;
          a1 = -2.0f * cos_omega;
          a2 = 1.0f - alpha;
          break;

      case FILTER_ALL_PASS:
          b0 = 1.0f - alpha;
          b1 = -2.0f * cos_omega;
          b2 = 1.0f + alpha;
          a0 = 1.0f + alpha;
          a1 = -2.0f * cos_omega;
          a2 = 1.0f - alpha;
          break;

      case FILTER_PEAKING: {
          float A = powf(10.0f, gain / 40.0f);  // Convert dB to linear
          b0 = 1.0f + alpha * A;
          b1 = -2.0f * cos_omega;
          b2 = 1.0f - alpha * A;
          a0 = 1.0f + alpha / A;
          a1 = -2.0f * cos_omega;
          a2 = 1.0f - alpha / A;
          break;
      }

      case FILTER_LOW_SHELF: {
          float A = powf(10.0f, gain / 40.0f);
          float beta = sqrtf(A) / resonance;
          b0 = A * ((A + 1.0f) - (A - 1.0f) * cos_omega + beta * sin_omega);
          b1 = 2.0f * A * ((A - 1.0f) - (A + 1.0f) * cos_omega);
          b2 = A * ((A + 1.0f) - (A - 1.0f) * cos_omega - beta * sin_omega);
          a0 = (A + 1.0f) + (A - 1.0f) * cos_omega + beta * sin_omega;
          a1 = -2.0f * ((A - 1.0f) + (A + 1.0f) * cos_omega);
          a2 = (A + 1.0f) + (A - 1.0f) * cos_omega - beta * sin_omega;
          break;
      }

      case FILTER_HIGH_SHELF: {
          float A = powf(10.0f, gain / 40.0f);
          float beta = sqrtf(A) / resonance;
          b0 = A * ((A + 1.0f) + (A - 1.0f) * cos_omega + beta * sin_omega);
          b1 = -2.0f * A * ((A - 1.0f) + (A + 1.0f) * cos_omega);
          b2 = A * ((A + 1.0f) + (A - 1.0f) * cos_omega - beta * sin_omega);
          a0 = (A + 1.0f) - (A - 1.0f) * cos_omega + beta * sin_omega;
          a1 = 2.0f * ((A - 1.0f) - (A + 1.0f) * cos_omega);
          a2 = (A + 1.0f) - (A - 1.0f) * cos_omega - beta * sin_omega;
          break;
        }
    }

    // Normalize coefficients
    voice_filter[n].b0 = b0 / a0;
    voice_filter[n].b1 = b1 / a0;
    voice_filter[n].b2 = b2 / a0;
    voice_filter[n].a1 = a1 / a0;
    voice_filter[n].a2 = a2 / a0;

    voice_filter_freq[n] = f;
    voice_filter_res[n] = resonance;
    voice_filter_gain[n] = gain;
}


int mmf_set_freq(int n, float f) {
  mmf_set_params(n, f, voice_filter_res[n], voice_filter_gain[n]);
  return 0;
}

int mmf_set_res(int n, float res) {
  mmf_set_params(n, voice_filter_freq[n], res, voice_filter_gain[n]);
  return 0;
}

int mmf_set_gain(int n, float gain) {
  mmf_set_params(n, voice_filter_freq[n], voice_filter_res[n], voice_filter_gain[n]);
  return 0;
}

int voice_copy(int v, int n) {
  wave_set(n, voice_wave_table_index[v]);
  amp_set(n, voice_user_amp[v]);
  freq_set(n, voice_freq[v]);
  pan_set(n, voice_pan[v]);
  amp_mod_set(n, voice_amp_mod_osc[v], voice_amp_mod_depth[v]);
  freq_mod_set(n, voice_freq_mod_osc[v], voice_freq_mod_depth[v]);
  pan_mod_set(n, voice_pan_mod_osc[v], voice_pan_mod_depth[v]);
  //wave_loop(n,
  //wave_dir(n,
  //wave_quant(n,
  envelope_set(n, voice_amp_envelope[v].a, voice_amp_envelope[v].d, voice_amp_envelope[v].s, voice_amp_envelope[v].r);
  cz_set(n, voice_cz_mode[v], voice_cz_distortion[v]);
  cmod_set(n, voice_cz_mod_osc[v], voice_cz_mod_depth[v]);
  voice_filter_mode[n] = voice_filter_mode[v];
  mmf_init(n, voice_filter_freq[v], voice_filter_res[v], voice_filter_gain[v]);
  return 0;
}

// Process a single sample through the filter - VERY FAST
// Only multiplication and addition, no transcendental functions
float mmf_process(int n, float input) {
    // Calculate output using Direct Form II - only 5 multiplies, 4 adds
    float output = voice_filter[n].b0 * input +
                  voice_filter[n].b1 * voice_filter[n].x1 +
                  voice_filter[n].b2 * voice_filter[n].x2 -
                  voice_filter[n].a1 * voice_filter[n].y1 -
                  voice_filter[n].a2 * voice_filter[n].y2;
    
    // Update delay lines
    voice_filter[n].x2 = voice_filter[n].x1;
    voice_filter[n].x1 = input;
    voice_filter[n].y2 = voice_filter[n].y1;
    voice_filter[n].y1 = output;
    
    return output;
}

#define FREQ_TABLE_SIZE 128
#define RES_TABLE_SIZE 32

typedef struct {
    float freq_table[FREQ_TABLE_SIZE];
    float res_table[RES_TABLE_SIZE];
    float coeff_table[FREQ_TABLE_SIZE][RES_TABLE_SIZE][5]; // b0,b1,b2,a1,a2
} mmf_pre_t;

mmf_pre_t mmf;

// Initialize lookup table (call once at startup)
void mmf_init_table(void) {
    for (int f = 0; f < FREQ_TABLE_SIZE; f++) {
        float freq = 20.0f * powf(1000.0f, (float)f / (FREQ_TABLE_SIZE-1)); // 20Hz to 20kHz
        mmf.freq_table[f] = freq;
        
        for (int r = 0; r < RES_TABLE_SIZE; r++) {
            float resonance = 0.1f + (10.0f - 0.1f) * (float)r / (RES_TABLE_SIZE-1);
            mmf.res_table[r] = resonance;
            
            // Calculate coefficients
            float omega = 2.0f * (float)M_PI * freq / MAIN_SAMPLE_RATE;
            float sin_omega = sinf(omega);
            float cos_omega = cosf(omega);
            float alpha = sin_omega / (2.0f * resonance);
            float a0 = 1.0f + alpha;
            
            mmf.coeff_table[f][r][0] = (1.0f - cos_omega) / (2.0f * a0); // b0
            mmf.coeff_table[f][r][1] = (1.0f - cos_omega) / a0;          // b1
            mmf.coeff_table[f][r][2] = (1.0f - cos_omega) / (2.0f * a0); // b2
            mmf.coeff_table[f][r][3] = -2.0f * cos_omega / a0;           // a1
            mmf.coeff_table[f][r][4] = (1.0f - alpha) / a0;              // a2
        }
    }
}

/* Example usage for real-time audio:

void audio_callback(float* input_buffer, float* output_buffer, int num_samples) {
    static LPFilter filter;
    static int initialized = 0;
    
    if (!initialized) {
        lpf_init(&filter, 1000.0f, 0.707f, 44100.0f);
        initialized = 1;
    }
    
    // Update filter parameters only if they changed
    // This is cheap - only does expensive math when needed
    lpf_set_params(&filter, current_cutoff_freq, current_resonance);
    
    // Process each sample - this is VERY fast
    for (int i = 0; i < num_samples; i++) {
        output_buffer[i] = lpf_process(&filter, input_buffer[i]);
    }
}
*/
