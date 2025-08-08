#define SOKOL_AUDIO_IMPL
#include "sokol_audio.h"

#include "linenoise.h"

#define HISTORY_FILE ".sok1_history"

#include <math.h>
#include <time.h>

#define MAIN_SAMPLE_RATE (44100)

#define WAVE_MAX (99)

double *wave_data[WAVE_MAX] = {NULL};
size_t wave_size[WAVE_MAX] = {0};

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


double *tsqr = NULL; // [] = {-1.00, 0.00, 1.00, 0.00};
double *tsin = NULL; // [] = {-1.00, -0.50, 0.0, 0.50, 1.00};


void free_tables(void);

#define NUM_VOICE (4)
#define num_chan (2)

double freq[NUM_VOICE] = {440.0};
double phase[NUM_VOICE] = {0.0};
double sample[NUM_VOICE] = {0.0};
double amp[NUM_VOICE] = {0.0};
double panl[NUM_VOICE] = {0.0};
double panr[NUM_VOICE] = {0.5};
double pan[NUM_VOICE] = {0.5};
int interp[NUM_VOICE] = {0};

int wtsel[NUM_VOICE] = {EXWAVESINE, EXWAVESINE};

void init_voice(void);

void init_voice(void) {
  for (int i=0; i<NUM_VOICE; i++) {
    freq[i] = 0.0;
    phase[i] = 0;
    sample[i] = 0;
    amp[i] = 0;
    pan[i] = 0;
    panl[i] = 0.5;
    panr[i] = 0.5;
    interp[i] = 0;
    wtsel[i] = 0;
  }
}

// double phase_inc = frequency / MAIN_SAMPLE_RATE;
double phase_inc[NUM_VOICE] = {0,0};

void next_sample(int osc, double *table, int table_size) {
  if (table == NULL) return;
  double index = phase[osc] * table_size;
  int i0 = (int)index;
  sample[osc] = table[i0];
  if (interp[osc]) {
    int i1 = (i0 + 1) % table_size;
    double frac = index - i0;
    sample[osc] += (frac * (table[i1] - table[i0]));
  }
  phase[osc] += phase_inc[osc];
  if (phase[osc] >= 1.0f) phase[osc] = 0.0f;
}

// Stream callback function
void stream_cb(float *buffer, int num_frames, int num_channels) { // , void *user_data) {
  for (int i = 0; i < num_frames; i++) {
    double samplel = 0;
    double sampler = 0;
    for (int osc = 0; osc < NUM_VOICE; osc++) {
      if (phase_inc[osc] == 0) continue;
      if (amp[osc] == 0) continue;
      int w = wtsel[osc];
      next_sample(osc, wave_data[w], wave_size[w]);
      // apply amp to sample
      sample[osc] *= amp[osc];
      // accumulate samples
      samplel += sample[osc] * panl[osc];
      sampler += sample[osc] * panr[osc];
    }
    // Write to all channels
    buffer[i * num_channels + 0] = samplel;
    buffer[i * num_channels + 1] = sampler;
  }
}

double hz2inc(int voice, double hz) {
  double size = 1;
  double phase_inc = (hz * (double)size) / (double)MAIN_SAMPLE_RATE;
  printf("hz = %f :: inc = %f\n", hz, phase_inc);
  return phase_inc;
}

void fsleep(double seconds) {
  if (seconds < 0.0f) return; // Invalid input
  struct timespec ts;
  ts.tv_sec = (time_t)seconds; // Whole seconds
  ts.tv_nsec = (long)((seconds - (double)ts.tv_sec) * 1e9); // Fractional part to nanoseconds
  nanosleep(&ts, NULL);
}

void init_tables(void);

int running = 1;

int current_voice = 0;

long mytol(char *str, int *valid, int *next) {
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

double mytod(char *str, int *valid, int *next) {
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
};

void show_voice(int current_voice) {
  printf("# v%d w%d f%f a%f p%f pl%f pr%f I%d\n",
    current_voice,
    wtsel[current_voice],
    freq[current_voice],
    amp[current_voice],
    pan[current_voice],
    panl[current_voice],
    panr[current_voice],
    interp[current_voice]);
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
    if (c == ' ' || c == '\t') continue;
    if (c == 'v') {
      n = mytol(&line[p], &valid, &next);
      if (!valid) {
        more = 0;
        r = ERR_EXPECTED_INT;
      } else {
        p += next-1;
        if (n >= 0 && n < NUM_VOICE) voice = n;
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
        default:
          r = 1; more = 0; break;
      }
      continue;
    }
    if (c == 'f') {
      f = mytod(&line[p], &valid, &next);
      if (!valid) {
        more = 0;
        r = ERR_EXPECTED_FLOAT;
      } else {
        if (f > 0 && f < (double)MAIN_SAMPLE_RATE) {
          freq[voice] = f;
          phase_inc[voice] = hz2inc(voice, f);
        } else {
          more = 0;
          r = ERR_FREQUENCY_OUT_OF_RANGE;
        }
      }
      continue;
    }
    if (c == 'a') {
      f = mytod(&line[p], &valid, &next);
      if (!valid) {
        more = 0;
        r = ERR_EXPECTED_FLOAT;
      } else {
        if (f >= 0) {
          amp[voice] = f;
        } else {
          more = 0;
          r = ERR_AMPLITUDE_OUT_OF_RANGE;
        }
      }
      continue;
    }
    if (c == 'w') {
      n = mytol(&line[p], &valid, &next);
      if (!valid) {
        more = 0;
        r = ERR_EXPECTED_INT;
      } else {
        p += next-1;
        if (n >= 0 && n < EXWAVMAX) {
          if (wave_data[n] && wave_size[n]) {
            wtsel[voice] = n;
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
      f = mytod(&line[p], &valid, &next);
      if (!valid) {
        more = 0;
        r = ERR_EXPECTED_FLOAT;
      } else {
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
    if (c == '?') {
      char peek = line[p];
      if (peek == '?') {
        p++;
        for (int i=0; i<NUM_VOICE; i++) {
          show_voice(i);
        }
      } else show_voice(voice);
      continue;
    }
  }
  if (this_voice) *this_voice = voice;
  return r;
}

int main(int argc, char *argv[]) {
  linenoiseHistoryLoad(HISTORY_FILE);
  init_tables();
  init_voice();
  
  // Initialize Sokol Audio
  saudio_setup(&(saudio_desc){
    .stream_cb = stream_cb,
    .sample_rate = MAIN_SAMPLE_RATE,
    .num_channels = num_chan // Stereo
  });

  while (running) {
    char *line = linenoise("> ");
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
  sleep(1); // make sure we don't crash the callback b/c thread timing and wave_data
  free_tables();
  return 0;
}


void init_tables(void) {
#define SIZE_SQR (256)
  if (tsqr) free(tsqr);
  tsqr = (double *)malloc(SIZE_SQR * sizeof(double));
  for (int i=0; i<SIZE_SQR; i++) {
    if (i < SIZE_SQR/2) tsqr[i] = -1;
    else tsqr[i] = 1;
  }
  // wave[0] = tsqr;
  // wlen[0] = SIZE_SQR;

  wave_data[EXWAVESQR] = tsqr;
  wave_size[EXWAVESQR] = SIZE_SQR;

#define SIZE_SINE (44100)
  if (tsin) free(tsin);
  tsin = (double *)malloc(SIZE_SINE * sizeof(double));
  for (int i=0; i<SIZE_SINE; i++) {
    double t = (i * 2.0 * M_PI) / (double)SIZE_SINE;
    tsin[i] = sin(t);
  }
  // wave[1] = tsin;
  // wlen[1] = SIZE_SINE;

  wave_data[EXWAVESINE] = tsin;
  wave_size[EXWAVESINE] = SIZE_SINE;
}


void free_tables(void) {
  for (int i = 0; i < EXWAVMAX; i++) {
    if (wave_data[i]) {
      free(wave_data[i]);
      wave_size[i] = 0;
    }
  }
}
