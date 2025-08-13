#include <stdio.h>

#define SOKOL_AUDIO_IMPL
#include "sokol_audio.h"

#define HISTORY_FILE ".sok1_history"
#define MAIN_SAMPLE_RATE (44100)
#define WT_MAX (99)
#define VOICE_MAX (16)
#define CHANNEL_NUM (2)
#define AFACTOR (0.025)

int show_audio(void) {
  if (saudio_isvalid()) {
    printf("# audio backend is running\n");
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
#include <time.h>

float *wt_data[WT_MAX] = {NULL};
int wt_size[WT_MAX] = {0};
float wt_rate[WT_MAX] = {0};

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
    
    EXWAVEKRG1,     // 16
    EXWAVEKRG2,     // 17
    EXWAVEKRG3,     // 18
    EXWAVEKRG4,     // 19
    EXWAVEKRG5,     // 10
    EXWAVEKRG6,     // 21
    EXWAVEKRG7,     // 22
    EXWAVEKRG8,     // 23
    EXWAVEKRG9,     // 24
    EXWAVEKRG10,    // 25
    EXWAVEKRG11,    // 26
    EXWAVEKRG12,    // 27
    EXWAVEKRG13,    // 28
    EXWAVEKRG14,    // 29
    EXWAVEKRG15,    // 30
    EXWAVEKRG16,    // 31

    EXWAVEKRG17,     // 32
    EXWAVEKRG18,     // 33
    EXWAVEKRG19,     // 34
    EXWAVEKRG20,     // 35
    EXWAVEKRG21,     // 36
    EXWAVEKRG22,     // 37
    EXWAVEKRG23,     // 38
    EXWAVEKRG24,     // 39
    EXWAVEKRG25,     // 40
    EXWAVEKRG26,    // 41
    EXWAVEKRG27,    // 42
    EXWAVEKRG28,    // 43
    EXWAVEKRG29,    // 44
    EXWAVEKRG30,    // 45
    EXWAVEKRG31,    // 46
    EXWAVEKRG32,    // 47

    EXWAVEOUT,      // 48

    EXWAVMAX
};


void wt_free(void);

double freq[VOICE_MAX];
double note[VOICE_MAX];
//double phase[VOICE_MAX];
float sample[VOICE_MAX];
float hold[VOICE_MAX];
double amp[VOICE_MAX];
double panl[VOICE_MAX];
double panr[VOICE_MAX];
double pan[VOICE_MAX];
int interp[VOICE_MAX];
int use_adsr[VOICE_MAX];
int fmod_osc[VOICE_MAX];
int pmod_osc[VOICE_MAX];
float fmod_depth[VOICE_MAX];
float pmod_depth[VOICE_MAX];
int hide[VOICE_MAX];
int decimate[VOICE_MAX];
int quantize[VOICE_MAX];

int wtsel[VOICE_MAX];

void init_voice(void);

typedef struct {
    float *table;           // Pointer to waveform or sample
    int table_size;         // Length of table
    float table_rate;       // Native sample rate of the table
    float phase;            // Current position in table
    float phase_inc;        // Phase increment per host sample
} osc_t;

osc_t osc[VOICE_MAX];

float osc_get_phase_inc(int v, float freq) {
    float phase_inc = (freq * osc[v].table_size) / osc[v].table_rate * (osc[v].table_rate / MAIN_SAMPLE_RATE);
    return phase_inc;
}

void osc_set_freq(int v, float freq, float system_sample_rate) {
    // Compute the frequency in "table samples per system sample"
    // This works even if table_rate ≠ system rate
    osc[v].phase_inc = (freq * osc[v].table_size) / osc[v].table_rate * (osc[v].table_rate / system_sample_rate);
}

float osc_next(int n, float phase_inc) {
    int table_size = osc[n].table_size;
    int i = (int)osc[n].phase % table_size;
    if (i < 0 || i >= table_size) {
      // printf("!! index %d\n", i);
      osc[n].phase = 0;
      return 0;
    }
    float sample;
    if (interp[n]) {
      int i_next = (i + 1) % table_size;
      float frac = 0.0;
      frac = osc[n].phase - i;
      sample = osc[n].table[i] + frac * (osc[n].table[i_next] - osc[n].table[i]);
    } else {
      sample = osc[n].table[i];      
    }

    osc[n].phase += phase_inc;
    if (osc[n].phase > table_size)
        osc[n].phase -= table_size;

    return sample;
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
    amp_env[n].sustain_level = a;
    amp_env[n].state = ADSR_ATTACK;
    amp_env[n].value = 0.0f;
    amp_env[n].time = 0.0f;
}

// Release the envelope (start release)
void adsr_release(int n) {
    if (amp_env[n].state != ADSR_OFF) {
        amp_env[n].state = ADSR_RELEASE;
        amp_env[n].time = 0.0f;
    }
}


// Compute one sample of the envelope
float adsr_step(int n) {
    if (amp_env[n].state == ADSR_OFF) {
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
int sample_count = 0;


// Stream callback function
#if 0
void stream_cb(float *buffer, int num_frames, int num_channels) { // , void *user_data) {
#else
void stream_userdata_cb(float *buffer, int num_frames, int num_channels, void *user_data) { // , void *user_data) {
#endif
  for (int i = 0; i < num_frames; i++) {
    sample_count++;
    float samplel = 0;
    float sampler = 0;
    float f = 0;
    for (int n = 0; n < VOICE_MAX; n++) {
      if (amp[n] == 0) continue;
      if (fmod_osc[n] >= 0) {
        int m = fmod_osc[n];
        float g = sample[m] * fmod_depth[n];
        float h = osc_get_phase_inc(n, freq[n] + g);
        f = osc_next(n, h);
      } else {
        f = osc_next(n, osc[n].phase_inc);
      }
      if (decimate[n] > 1) {
        if ((sample_count % decimate[n]) == 1) sample[n] = f;
      } else {
        sample[n] = f;
      }
      if (quantize[n]) {
        sample[n] = quantize_bits_int(sample[n], quantize[n]);
      }

      // apply amp to sample
      // sample[n] *= amp[n];
      if (use_adsr[n]) {
        float amod = adsr_step(n);
        sample[n] *= amod; // * AFACTOR;
      } else {
        sample[n] *= amp[n];
      }
      // accumulate samples
      if (hide[n] == 0) {
        if (pmod_osc[n] >= 0) {
          float q = sample[pmod_osc[n]] * pmod_depth[n];
          panl[n] = (1.0 - q) / 2.0;
          panr[n] = (1.0 + q) / 2.0;          
        }
        samplel += sample[n] * panl[n];
        sampler += sample[n] * panr[n];
      }
    }
    // Write to all channels
    buffer[i * num_channels + 0] = samplel;
    buffer[i * num_channels + 1] = sampler;
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

int running = 1;

int current_voice = 0;

#include <dirent.h>

void show_threads(void) {
  DIR* dir;
  struct dirent* entry;
  // Open /proc/self/task to list threads
  dir = opendir("/proc/self/task");
  if (dir == NULL) {
    perror("Failed to open /proc/self/task");
    return;
  }

  // Iterate through each thread directory
  while ((entry = readdir(dir)) != NULL) {
    // Skip . and .. directories
    if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
      continue;
    }
    char *thread_name = NULL;
    printf("%s\t%s\n", entry->d_name, thread_name ? thread_name : "Unknown");
    free(thread_name);
  }

  closedir(dir);
}

long parse_int(char *str, int *valid, int *next) {
  long val;
  char *endptr;
  val = strtol(str, &endptr, 10);
  if (endptr == str) {
    if (valid) *valid = 0;
    if (next) *next = 0;
    return 0;
  }
  if (valid) *valid = 1;
  if (next) *next = endptr - str + 1;
  return val;
}

double parse_double(char *str, int *valid, int *next) {
  double val;
  char *endptr;
  val = strtod(str, &endptr);
  if (endptr == str) {
    if (valid) *valid = 0;
    if (next) *next = 0;
    return 0;
  }
  if (valid) *valid = 1;
  if (next) *next = endptr - str + 1;
  return val;
}

enum {
  ERR_EXPECTED_INT,
  ERR_EXPECTED_FLOAT,
  ERR_INVALID_VOICE,
  ERR_INVALID_COLON,
  ERR_FREQUENCY_OUT_OF_RANGE,
  ERR_AMPLITUDE_OUT_OF_RANGE,
  ERR_INVALID_WAVE,
  ERR_EMPTY_WAVE,
  ERR_INVALID_INTERPOLATE,
  ERR_PAN_OUT_OF_RANGE,
  ERR_INVALID_DELAY,
  ERR_INVALID_MODULATOR,
};

void show_voice(int v, char c) {
  printf("# %cv%d w%d n%g f%g a%g p%g I%d d%d q%d",
    c,
    v,
    wtsel[v],
    note[v],
    freq[v],
    amp[v] / AFACTOR,
    pan[v],
    interp[v],
    decimate[v],
    quantize[v]);
  printf(" F%d,%g P%d,%g m%d A%g,%g,%g,%g",
    fmod_osc[v],
    fmod_depth[v],
    pmod_osc[v],
    pmod_depth[v],
    hide[v],
    amp_env[v].attack_time,
    amp_env[v].decay_time,
    amp_env[v].sustain_level,
    amp_env[v].release_time);
  printf(" ## %g/%g", osc[v].phase, osc[v].phase_inc);
  puts("");
}

int wire(char *line, int *this_voice, int output) {
  int p = 0;
  int more = 1;
  int r = 0;
  int next;
  int valid;
  int n, na[8];
  double f, fa[8];
  int voice = 0;
  if (this_voice) voice = *this_voice;
  while (more) {
    valid = 1;
    char c = line[p++];
    char t = '\0';
    if (c == '\0') {
      more = 0;
      break;
    }
    printf("# %c / %d / %x [%d]\n", c, c, c, p);
    if (c == '#') break;
    if (c == ' ' || c == '\t' || c == ';' || c == '\n' || c == '\r') continue;
    if (c == 'v') {
      n = parse_int(&line[p], &valid, &next);
      if (!valid) {
        more = 0;
        r = ERR_EXPECTED_INT;
      } else {
        p += next-1;
        if (n >= 0 && n < VOICE_MAX) voice = n;
        else {
          more = 0;
          r = ERR_INVALID_VOICE;
        }
      }
      continue;
    }
    if (c == ':') {
      t = line[p++];
      switch (t) {
        case 'q': r = -1; more = 0; break;
        case 's': show_threads(); show_audio(); break;
        default:
          r = 1; more = 0; break;
      }
      continue;
    }
    if (c == 'f') {
      f = parse_double(&line[p], &valid, &next);
      if (!valid) {
        more = 0;
        r = ERR_EXPECTED_FLOAT;
      } else {
        p += next-1;
        if (f > 0 && f < (double)MAIN_SAMPLE_RATE) {
          freq[voice] = f;
          osc_set_freq(voice, f, MAIN_SAMPLE_RATE);
        } else {
          more = 0;
          r = ERR_FREQUENCY_OUT_OF_RANGE;
        }
      }
      continue;
    }
    if (c == 'a') {
      f = parse_double(&line[p], &valid, &next);
      if (!valid) {
        more = 0;
        r = ERR_EXPECTED_FLOAT;
      } else {
        p += next-1;
        if (f >= 0) {
          use_adsr[voice] = 0;
          amp[voice] = f * AFACTOR;
        } else {
          more = 0;
          r = ERR_AMPLITUDE_OUT_OF_RANGE;
        }
      }
      continue;
    }
    if (c == 'l') {
      f = parse_double(&line[p], &valid, &next);
      if (!valid) {
        more = 0;
        r = ERR_EXPECTED_FLOAT;
      } else {
        p += next-1;
        if (f == 0) {
          adsr_release(voice);
        } else {
          use_adsr[voice] = 1;
          amp[voice] = f * AFACTOR;
          adsr_trigger(voice, f);
        }
      }
      continue;
    }
    if (c == 'm') {
      n = parse_int(&line[p], &valid, &next);
      if (!valid) {
        more = 0;
        r = ERR_EXPECTED_INT;
      } else {
        p += next-1;
        if (n == 0) {
          hide[voice] = 0;
        } else {
          hide[voice] = 1;
        }
      }
      continue;
    }
    if (c == 'P') {
      n = parse_int(&line[p], &valid, &next);
      if (!valid) {
        more = 0;
        r = ERR_EXPECTED_INT;
      } else {
        if (n < 0) {
        } else if (n >= 0 && n < VOICE_MAX) {
          pmod_osc[voice] = n;
        } else {
          pmod_osc[voice] = -1;
        }
        char peek = line[p+1];
        if (peek == ',') {
          p++;
          p++;
          f = parse_double(&line[p], &valid, &next);
          if (!valid) {
            more = 0;
            r = ERR_EXPECTED_INT;
          } else {
            pmod_depth[voice] = f;
            p += next-1;
          }
        }
      }
      continue;
    }
    if (c == 'n') {
      f = parse_double(&line[p], &valid, &next);
      if (!valid) {
        more = 0;
        r = ERR_EXPECTED_FLOAT;
      } else {
        p += next-1;
        float g = 440.0 * pow(2.0, (f - 69.0) / 12.0);
        note[voice] = f;
        freq[voice] = g;
        osc_set_freq(voice, g, MAIN_SAMPLE_RATE);
      }
      continue;
    }
    if (c == 'F') {
      n = parse_int(&line[p], &valid, &next);
      if (!valid) {
        more = 0;
        r = ERR_EXPECTED_INT;
      } else {
        if (n < 0) {
          fmod_osc[voice] = -1;
        } else {
          if (n < VOICE_MAX) {
            fmod_osc[voice] = n;
            char peek = line[p+1];
            if (peek == ',') {
              p++;
              p++;
              f = parse_double(&line[p], &valid, &next);
              if (!valid) {
                more = 0;
                r = ERR_EXPECTED_FLOAT;
              } else {
                fmod_depth[voice] = f;
                p += next-1;
              }
            }
          } else {
            more = 0;
            r = ERR_INVALID_MODULATOR;
          }
        }
      }
      continue;
    }
    if (c == 'w') {
      n = parse_int(&line[p], &valid, &next);
      if (!valid) {
        more = 0;
        r = ERR_EXPECTED_INT;
      } else {
        p += next-1;
        if (n >= 0 && n < EXWAVMAX) {
          if (wtsel[voice] == n) continue;
          if (wt_data[n] && wt_size[n] && wt_rate[n] > 0.0) {
            wtsel[voice] = n;
            int update_freq = 0;
            if (osc[voice].table_rate != wt_rate[n] || osc[voice].table_size != wt_size[n]) update_freq = 1;
            osc[voice].table_rate = wt_rate[n];
            osc[voice].table_size = wt_size[n];
            osc[voice].table = wt_data[n];
            if (update_freq) {
              osc_set_freq(voice, freq[voice], MAIN_SAMPLE_RATE);
            }
          } else {
            more = 0;
            r = ERR_EMPTY_WAVE;
          }
        } else {
          more = 0;
          r = ERR_INVALID_WAVE;
        }
      }
      continue;
    }
    if (c == 'I') {
      c = line[p++];
      if (c == '0') interp[voice] = 0;
      else if (c == '1') interp[voice] = 1;
      else {
        more = 0;
        r = ERR_INVALID_INTERPOLATE;
      }
      continue;
    }
    if (c == 'p') {
      f = parse_double(&line[p], &valid, &next);
      if (!valid) {
        more = 0;
        r = ERR_EXPECTED_FLOAT;
      } else {
        p += next-1;
        if (f >= -1.0 && f <= 1.0) {
          pan[voice] = f;
          panl[voice] = (1.0 - f) / 2.0;
          panr[voice] = (1.0 + f) / 2.0;          
        } else {
          more = 0;
          r = ERR_PAN_OUT_OF_RANGE;
        }
      }
      continue;
    }
    if (c == '+') {
      f = parse_double(&line[p], &valid, &next);
      if (!valid) {
        more = 0;
        r = ERR_EXPECTED_FLOAT;
      } else {
        p += next-1;
        if (f > 0) {
          fsleep(f);
        } else {
          more = 0;
          r = ERR_INVALID_DELAY;          
        }
      }
      continue;
    }
    if (c == 'A') {
      for (int s = 0; s < 4; s++) {
        f = parse_double(&line[p], &valid, &next);
        if (!valid) {
          more = 0;
          r = ERR_EXPECTED_FLOAT;
          break;
        } else {
          if (s == 0) {
            amp_env[voice].attack_time = f;
          } else if (s == 1) {
            amp_env[voice].decay_time = f;
          } else if (s == 2) {
            amp_env[voice].sustain_level = f * 1; // AFACTOR;
          } else if (s == 3) {
            amp_env[voice].release_time = f;
          }
          p += next-1;
          c = line[p];
          if (c == ',') {
            p++;
          } else {
            break;
          }
        }
      }
      continue;
    }
    if (c == 'd') {
      n = parse_int(&line[p], &valid, &next);
      if (!valid) {
        more = 0;
        r = ERR_EXPECTED_INT;
      } else {
        decimate[voice] = n;
      }
      continue;
    }
    if (c == 'q') {
      n = parse_int(&line[p], &valid, &next);
      if (!valid) {
        more = 0;
        r = ERR_EXPECTED_INT;
      } else {
        quantize[voice] = n;
      }
      continue;
    }
    if (c == '?') {
      char peek = line[p];
      if (peek == '?') {
        p++;
        for (int i=0; i<VOICE_MAX; i++) {
          if (freq[i] == 0) continue;
          c = ' ';
          if (i == current_voice) c = '*';
          show_voice(i, c);
        }
      } else show_voice(voice, ' ');
      continue;
    }
  }
  if (this_voice) *this_voice = voice;
  return r;
}


char my_data[] = "hello";

int main(int argc, char *argv[]) {
  linenoiseHistoryLoad(HISTORY_FILE);
  init_wt();
  init_voice();
  
  // Initialize Sokol Audio
  saudio_setup(&(saudio_desc){
    // .stream_cb = stream_cb,
    .stream_userdata_cb = stream_userdata_cb,
    .user_data = &my_data,
    .sample_rate = MAIN_SAMPLE_RATE,
    .num_channels = CHANNEL_NUM, // Stereo
    // .user_data = &my_data, // todo
  });

  if (show_audio() != 0) return 1;

  while (running) {
    char *line = linenoise("# ");
    if (line == NULL) {
      running = 0;
      break;
    }
    if (strlen(line) == 0) continue;
    linenoiseHistoryAdd(line);
    int n = wire(line, &current_voice, 1);
    if (n < 0) break; // request to stop or error
    if (n > 0) {
      printf("got information %d\n", n);
    }
    linenoiseFree(line);
  }
  linenoiseHistorySave(HISTORY_FILE);

  // Cleanup
  saudio_shutdown();
  sleep(1); // make sure we don't crash the callback b/c thread timing and wt_data
  wt_free();
  return 0;
}

#include "retro/korg.h"

void init_wt(void) {
  float *table;
  float f;
  float d;

#define SIZE_SQR (4096)
  table = (float *)malloc(SIZE_SQR * sizeof(float));
  for (int i=0; i<SIZE_SQR; i++) {
    if (i < SIZE_SQR/2) table[i] = -1;
    else table[i] = 1;
  }
  wt_data[EXWAVESQR] = table;
  wt_size[EXWAVESQR] = SIZE_SQR;
  wt_rate[EXWAVESQR] = MAIN_SAMPLE_RATE;

#define SIZE_SINE (4096)
  table = (float *)malloc(SIZE_SINE * sizeof(float));
  for (int i=0; i<SIZE_SINE; i++) {
    float t = (i * 2.0 * M_PI) / (float)SIZE_SINE;
    table[i] = sin(t);
  }
  wt_data[EXWAVESINE] = table;
  wt_size[EXWAVESINE] = SIZE_SINE;
  wt_rate[EXWAVESINE] = MAIN_SAMPLE_RATE;

#define SIZE_SAWDN (4096)
  table = (float *)malloc(SIZE_SAWDN * sizeof(float));
  f = -1.0;
  d = 2.0 / (float)SIZE_SAWDN;
  for (int i=0; i<SIZE_SAWDN; i++) {
    table[i] = f - 1.0;
    f += d;
  }
  wt_data[EXWAVESAWDN] = table;
  wt_size[EXWAVESAWDN] = SIZE_SAWDN;
  wt_rate[EXWAVESAWDN] = MAIN_SAMPLE_RATE;

#define SIZE_SAWUP (4096)
  table = (float *)malloc(SIZE_SAWUP * sizeof(float));
  f = 1.0;
  d = 2.0 / (float)SIZE_SAWUP;
  for (int i=0; i<SIZE_SAWUP; i++) {
    table[i] = f - 1.0;
    f -= d;
  }
  wt_data[EXWAVESAWUP] = table;
  wt_size[EXWAVESAWUP] = SIZE_SAWUP;
  wt_rate[EXWAVESAWUP] = MAIN_SAMPLE_RATE;

#define SIZE_TRI (4096)
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

  korg_init();

  for (int i = EXWAVEKRG1; i <= EXWAVEKRG32; i++) {
    int k = i - EXWAVEKRG1;
    int s = kwave_size[k];
    printf("[%d] %d\n", k, s);
    table = malloc(s * sizeof(float));
    for (int j = 0 ; j < s; j++) {
      table[j] = (float)kwave[k][j] / (float)32767;
    }
    wt_data[i] = table;
    wt_size[i] = s;
    wt_rate[i] = MAIN_SAMPLE_RATE;
  }

  for (int i=0; i<VOICE_MAX; i++) {
    wtsel[i] = EXWAVESINE;
    osc[i].table = wt_data[wtsel[i]];
    osc[i].table_size = wt_size[wtsel[i]];
    osc[i].table_rate = wt_rate[wtsel[i]];
    osc[i].phase = 0;
    osc[i].phase_inc = 0;
  }

}

void wt_free(void) {
  for (int i = 0; i < EXWAVMAX; i++) {
    if (wt_data[i]) {
      free(wt_data[i]);
      wt_size[i] = 0;
    }
  }
}

void init_voice(void) {
  for (int i=0; i<VOICE_MAX; i++) {
    freq[i] = 0.0;
    // phase[i] = 0;
    sample[i] = 0;
    hold[i] = 0;
    amp[i] = 0;
    pan[i] = 0;
    panl[i] = 0.5;
    panr[i] = 0.5;
    interp[i] = 0;
    wtsel[i] = 0;
    use_adsr[i] = 0;
    fmod_osc[i] = -1;
    pmod_osc[i] = -1;
    hide[i] = 0;
    decimate[i] = 0;
    quantize[i] = 0;
    adsr_init(i, 1.1f, 0.2f, 0.7f, 0.5f, 44100.0f);
  }
}