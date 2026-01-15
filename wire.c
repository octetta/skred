#include "skred.h"
#include "wire.h"
#include "seq.h"
#include "miniwav.h"

#include "synth-types.h"
#include "synth.h"

#include <stdarg.h>
#include <stdio.h>

int wire_puts(wire_t *w, const char *s) {
  //puts("PUTS");
  if (!w || w->log_enable == 0) return 0;
  strncat(w->log, s, w->log_max);
  strncat(w->log, "\n", w->log_max);
  w->log_len = strlen(w->log);
  return 0;
}

int wire_printf(wire_t *w, const char *fmt, ...) {
  if (!w || w->log_enable == 0) return 0;
  //puts("PRINTF");
  char buf[4096];
  va_list ap;
  va_start(ap, fmt);
  int len = vsnprintf(buf, sizeof(buf), fmt, ap);
  va_end(ap);
  if (len > 0) {
    size_t out_len = (len >= (int)sizeof(buf)) ? sizeof(buf)-1 : (size_t)len;
    if (out_len) {
      strncat(w->log, buf, w->log_max);
      w->log_len = strlen(w->log);
    }
  }
  return 0;
}

int null_puts(const char *s) { return 0; }
int null_printf(const char *fmt, ...) { return 0; }

#define MPSC_QUEUE_IMPLEMENTATION
#include "mpsc_queue.h"

#if 1 // performance event listener
#include <pthread.h>

static mpsc_queue mq;
static pthread_t perf_thread_handle;
static int perf_running = 1;
#include "util.h"
static void *perf_main(void *arg) {
  char msg[65536];
  util_set_thread_name("perf");
  while (perf_running) {
    mpsc_queue_receive(&mq, msg, sizeof(msg));
    //bool r = mpsc_queue_receive(&mq, msg, sizeof(msg));
    //printf("# perf_main (%d) <%s>\n", r, msg);
  }
  return NULL;
}

int perf_start(void) {
  mpsc_queue_init(&mq);
  perf_running = 1;
  pthread_create(&perf_thread_handle, NULL, perf_main, NULL);
  pthread_detach(perf_thread_handle);
  return 0;
}

void perf_stop(void) {
  perf_running = 0;
  mpsc_queue_send(&mq, "#"); // tickle perf_main to make sure it sees the flag change
}

#endif

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

#include <stdio.h>
#include <stdint.h>

// TODO, use the voice_record[] array to determine
// which how many channels and voices to write to the
// wave file (update the header too)

// TODO at a minimum make cue notes for the voice number
// and left/right designation to the wave file

// MAYBE store the voice parameters in a cue note?
// MAYBE store patterns in a cue note?
// MAYBE store tempo in cue note?
// MAYBE have a user note for each oscillator that's
//       stored in a queue note
// MAYBE have a user note for pattern that's stored in a queue note

#include <math.h>

void save_wav(wire_t *w, char *filename, float *samples, long num_samples, int *record, int max) {
  int *record_safe;

  record_safe = (int *)malloc(sizeof(int)*max);
  if (record_safe == NULL) {
    w->puts(w, "OUCH"); return;
  } // nowhere to keep state
  
  int num_channels = 0;  // 32 pairs = 64 channels

  for (int i = 0; i < max; i++) {
    record_safe[i] = record[i];
    if (record[i]) num_channels += 2;
  }

  if (num_channels == 0) {
    free(record_safe);
    return;
  } // nothing to record

  FILE *f = fopen(filename, "wb");
  if (!f) {
    w->puts(w, "# can't open file\n");
    return;
  }

  int sample_rate = 44100;
  int bits_per_sample = 16;
  int byte_rate = sample_rate * num_channels * bits_per_sample / 8;
  int block_align = num_channels * bits_per_sample / 8;
  int data_size = num_samples * block_align;
    
  // WAV header
  fwrite("RIFF", 1, 4, f);
  uint32_t chunk_size = 36 + data_size;
  fwrite(&chunk_size, 4, 1, f);
  fwrite("WAVE", 1, 4, f);
    
  // fmt subchunk
  fwrite("fmt ", 1, 4, f);
  uint32_t subchunk1_size = 16;
  fwrite(&subchunk1_size, 4, 1, f);
  uint16_t audio_format = 1;  // PCM
  fwrite(&audio_format, 2, 1, f);
  uint16_t channels = num_channels;
  fwrite(&channels, 2, 1, f);
  fwrite(&sample_rate, 4, 1, f);
  fwrite(&byte_rate, 4, 1, f);
  uint16_t align = block_align;
  fwrite(&align, 2, 1, f);
  uint16_t bps = bits_per_sample;
  fwrite(&bps, 2, 1, f);

  // data subchunk
  fwrite("data", 1, 4, f);
  fwrite(&data_size, 4, 1, f);

  // since skred plays loose and wild with sample ranges,
  // get the min/max of all the samples in the recorded range

  float fbig = 0.0;
  float fsmall = 0.0;
  for (int i = 0; i < num_samples * VOICE_MAX * AUDIO_CHANNELS; i++) {
    float g = samples[i];
    if (g > fbig) fbig = g;
    if (g < fsmall) fsmall = g;
  }

  // use the min/max to make a scale factor that keeps 0
  // in the same relative place

  float scale;
  if (fabsf(fsmall) > fabsf(fbig)) {
    scale = -1.0f / fsmall;
  } else {
    scale = 1.0f / fbig;
  }
 
  // Convert scaled float samples to 16-bit PCM

  for (int i = 0; i < num_samples * VOICE_MAX * AUDIO_CHANNELS; i++) {
    int ri = (i % (VOICE_MAX * AUDIO_CHANNELS)) >> 1;
    if (record_safe[ri] == 0) continue; // skip things that aren't recorded
    float g = samples[i];
    g *= scale;
    if (g > 1.0f) g = 1.0f;
    if (g < -1.0f) g = -1.0f;
    int16_t sample = (int16_t)(g * 32767.0f);
    fwrite(&sample, 2, 1, f);
  }

  fclose(f);
  free(record_safe);
}

void voice_show(wire_t *w, int v, char c, int verbose) {
  char s[1024];
  char e[8] = "";
  if (c != ' ') sprintf(e, " # *");
  voice_format(v, s, verbose);
  if (strlen(s)) w->printf(w, "; %s%s\n", s, e);
}

int voice_show_all(wire_t *w, int voice, int verbose) {
  for (int i=0; i<VOICE_MAX; i++) {
    if (voice_amp[i] == 0) continue;
    char t = ' ';
    if (i == voice) t = '*';
    voice_show(w, i, t, verbose);
  }
  return 0;
}

#define WIRE_POINTER_MAX (100)
static wire_t *wl[WIRE_POINTER_MAX];

int wire_hash(wire_t *w) {
  uintptr_t addr = (uintptr_t)w;
  addr *= 2654435769u; // knuth's multiplicitive hash (based on golden thingy?)
  return addr % WIRE_POINTER_MAX;
}

void wire_show(wire_t *w) {
  if (w != NULL) {
    w->printf(w, "# voice %d\n", w->voice);
    w->printf(w, "# pattern %d\n", w->pattern);
    w->printf(w, "# scratch %s\n", skode_string(w->sk));
    w->printf(w, "( ");
    int flag = 1;
    int show_dots = 0;
    double *data = skode_data(w->sk);
    int data_len = skode_data_len(w->sk);
#define DOT_NUM (3)
    for (int i = 0; i < data_len; i++) {
      if (i < DOT_NUM) {
        show_dots = 0;
        w->printf(w, "%.8g ", data[i]);
      } else if (i >= (data_len - DOT_NUM)) {
        show_dots = 0;
        w->printf(w, "%.8g ", data[i]);
      } else {
        show_dots = 1;
      }
      if (flag && show_dots) {
        flag = 0; // only once
        w->printf(w, " ... ");
      }
    }
    w->printf(w, ") # %d elements\n", data_len);
  }
  for (int i = 0; i < WIRE_POINTER_MAX; i++) {
    if (wl[i]) {
      w->printf(w, "# wl[%d] {.voice=%d .pattern=%d .step=%d. .events=%d}\n",
        wire_hash(wl[i]),
        wl[i]->voice,
        wl[i]->pattern,
        wl[i]->step,
        wl[i]->events);
    }
  }
}

#include "udp.h"

void system_show(wire_t *w) {
  wire_t wprime;
  if (w == NULL) {
    w = &wprime;
    wire_init(w);
  }
  w->printf(w, "# udp_port %d\n", udp_info());
}

void show_stats(wire_t *w) {
  // do something useful
  w->printf(w, "# rec_state : %d rec_ptr %ld\n", rec_state, rec_ptr);
  w->printf(w, "# synth frames per callback %d : %gms\n",
    synth_frames_per_callback, (float)synth_frames_per_callback / (float)MAIN_SAMPLE_RATE * 1000.0f);
  w->printf(w, "# seq frames per callback %d : %gms\n",
    seq_frames_per_callback, (float)seq_frames_per_callback / (float)MAIN_SAMPLE_RATE * 1000.0f);
  for (int i = 0; i < QUEUE_SIZE; i++) {
    if (work_queue[i].state != Q_FREE) {
#ifdef _WIN32
      w->printf(w, "# [%d] (%d) @%lld {%s}\n", i, work_queue[i].state, work_queue[i].when, work_queue[i].what);
#else
      w->printf(w, "# [%d] (%d) @%ld {%s}\n", i, work_queue[i].state, work_queue[i].when, work_queue[i].what);
#endif
    }
  }
}

#ifdef _WIN32
#include <windows.h>
#include <tlhelp32.h>
#include <processthreadsapi.h>
#endif

void show_threads(wire_t *w) {
  wire_t wprime;
  if (w == NULL) {
    w = &wprime;
    wire_init(w);
  }
#ifdef _WIN32
  DWORD processId = GetCurrentProcessId();
  HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
  if (hSnapshot == INVALID_HANDLE_VALUE) {
      return;
  }

  THREADENTRY32 te32;
  te32.dwSize = sizeof(THREADENTRY32);

  if (Thread32First(hSnapshot, &te32)) {
    do {
      if (te32.th32OwnerProcessID == processId) {
        HANDLE hThread = OpenThread(THREAD_QUERY_LIMITED_INFORMATION, FALSE, te32.th32ThreadID);
        if (hThread) {
          PWSTR threadName = NULL;
          HRESULT hr = GetThreadDescription(hThread, &threadName);
          if (FAILED(hr)) {
            w->printf(w, "# %lu <GetThreadDescription failed>\n", te32.th32ThreadID);
          } else if (threadName == NULL || wcslen(threadName) == 0) {
            w->printf(w, "# %lu <unnamed>\n", te32.th32ThreadID);
          } else {
            char narrowName[256];
            WideCharToMultiByte(CP_UTF8, 0, threadName, -1, narrowName, sizeof(narrowName), NULL, NULL);
            w->printf(w, "# %lu %s\n", te32.th32ThreadID, narrowName);
            LocalFree(threadName);
          }
          CloseHandle(hThread);
        } else {
          w->printf(w, "# %lu <cannot open thread>\n", te32.th32ThreadID);
        }
      }
    } while (Thread32Next(hSnapshot, &te32));
  }

  CloseHandle(hSnapshot);
#else
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
    w->printf(w, "# %s %s\n", entry->d_name, name);
  }

  closedir(dir);
#endif
}

int sk_load(wire_t *w, int voice, int n) {
  wire_t wprime;
  if (w == NULL) {
    w = &wprime;
    wire_init(w);
  }
  char file[1024];
  sprintf(file, "%d.sk", n);
  FILE *in = fopen(file, "r");
  if (in == NULL) {
    sprintf(file, "sk/%d.sk", n);
    in = fopen(file, "r");
  }
  int r = 0;
  if (in) {
    static wire_t wprime = WIRE();
    char line[1024];
    while (fgets(line, sizeof(line), in) != NULL) {
      size_t len = strlen(line);
      if (len > 0 && line[len-1] == '\n') line[len-1] = '\0';
      w->printf(w, "# %s\n", line);
      r = wire(line, &wprime);
      if (r != 0) {
        w->printf(w, "# error in patch\n");
        break;
      }
    }
    fclose(in);
  }
  return r;
}

int data_load(wire_t *w, int wave_slot, int one_shot, float rate, float offset) {
  if (w == NULL) return 100; // fix todo
  w->printf(w, "# data_load(w, %d, %d, %g, %g)\n", wave_slot, one_shot, rate, offset);
  if (wave_slot < 0 || wave_slot >= EXT_SAMPLE_999) {
    w->printf(w, "# invalid slot %d\n", wave_slot);
    return -1;
  }
  double *data = skode_data(w->sk);
  int data_len = skode_data_len(w->sk);
  if (data == NULL) {
    w->printf(w, "# no data\n");
    return 100; // fix todo
  }
  if (data_len <= 0) {
    w->printf(w, "# no data len\n");
    return 100;
  }
  if (wave_readonly[wave_slot] == 1) {
    w->printf(w, "# cannot write to w%d r/o\n", wave_slot);
    return -1;
  }
  if (rate <= 0) {
    w->printf(w, "# invalid rate %g > 0\n", rate);
    return -1;
  }
  if (wave_refcount[wave_slot] > 0) {
    w->printf(w, "# cannot write to w%d ref > 0\n", wave_slot);
    return -1;
  } else {
    wave_free_one(wave_slot);
  }
  float *table = calloc(data_len, sizeof(float));
  for (int i=0; i<data_len; i++) table[i] = (float)data[i];
  int len = data_len;
    snprintf(wave_name[wave_slot], WAVE_NAME_MAX, "data[%d]", data_len);
    wave_is_miniwav[wave_slot] = 0;
    wave_table_data[wave_slot] = table;
    wave_size[wave_slot] = len;
    wave_rate[wave_slot] = rate;
    wave_one_shot[wave_slot] = (one_shot != 0);
    wave_loop_enabled[wave_slot] = 0;
    wave_loop_start[wave_slot] = 1;
    wave_loop_end[wave_slot] = len;
    if (offset > 0) {
      wave_offset_hz[wave_slot] = (float)len / rate * 440.0f;
      wave_midi_note[wave_slot] = 69;
    } else {
      wave_offset_hz[wave_slot] = 0.0f;
      wave_midi_note[wave_slot] = 0;
    }
    char *name = "data";
    int channels = 1;
    w->printf(w, "# read %d frames from %s to %d (ch:%d sr:%d)\n", len, name, wave_slot, channels, 44100);
  return 0;
}

int wave_load(wire_t *w, int file_num, int wave_index, int ch, int normalize) {
  if (w == NULL) return 100; // fix todo
  if (wave_index < EXT_SAMPLE_000 || wave_index >= EXT_SAMPLE_999) return -1;
  if (wave_readonly[wave_index] == 1) {
    w->printf(w, "# cannot write to w%d r/o\n", wave_index);
    return -1;
  }
  if (wave_refcount[wave_index] > 0) {
    w->printf(w, "# cannot write to w%d ref > 0\n", wave_index);
    return -1;
  } else {
    wave_free_one(wave_index);
  }
  char name[1024];
  sprintf(name, "%d.wav", file_num);
  FILE *in = fopen(name, "r");
  if (in) fclose(in);
  else {
    sprintf(name, "wav/%d.wav", file_num);
    in = fopen(name, "r");
    if (in) fclose(in);
    else {
      w->printf(w, "# cannot open %d.wav or wav/%d.wav\n", file_num, file_num);
      return -1;
    }
  }
  wav_t wav;
  int len;
  char out[4096];
  float *table = mw_get_str(name, &len, &wav, ch, out, sizeof(out));
  if (table == NULL) {
    w->printf(w, "# can not read %s\n", name);
    return -1;
  } else {
    strncpy(wave_name[wave_index], name, WAVE_NAME_MAX);
    wave_is_miniwav[wave_index] = 1;
    wave_table_data[wave_index] = table;
    wave_size[wave_index] = len;
    wave_rate[wave_index] = (float)wav.SamplesRate;
    wave_one_shot[wave_index] = 1;
    wave_loop_enabled[wave_index] = 0;
    wave_loop_start[wave_index] = 1;
    wave_loop_end[wave_index] = len;
    wave_midi_note[wave_index] = 69;
    wave_offset_hz[wave_index] = (float)len / (float)wav.SamplesRate * 440.0f;
    w->printf(w, "# read %d frames from %s to %d (ch:%d sr:%d)\n",
      len, name, wave_index, wav.Channels, wav.SamplesRate);
    normalize_preserve_zero(table, len);
  }
  return 0;
}


// this is a mess i need to clean up

#include "scope-shared.h"
extern int scope_enable;
extern scope_buffer_t *scope;

void pattern_show(wire_t *w, int pattern_pointer) {
  int first = 1;
  for (int s = 0; s < SEQ_STEPS_MAX; s++) {
    char *line = seq_pattern[pattern_pointer][s];
    if (strlen(line) == 0) break;
    if (first) {
      w->printf(w, "; y%d %%%d # step %d\n",
        pattern_pointer, seq_modulo[pattern_pointer], w->step);
      first = 0;
    }
    w->printf(w, "; {%s} x%d", line, s);
    if (seq_pattern_mute[pattern_pointer][s]) w->printf(w, " @%d", pattern_pointer);
    w->puts(w, "");
  }
}

void tempo_set(float m);

void downsample_block_average_min_max(
    const float *source, int source_len, float *dest, int dest_len,
    float *min, float *max) {
    
    if (source_len <= 0 || dest_len <= 0) return;

    // CASE 1: STRETCH (source is smaller than display)
    if (dest_len > source_len) {
        float step = (float)(source_len - 1) / (float)(dest_len - 1);
        for (int i = 0; i < dest_len; i++) {
            int src_idx = (int)(i * step);
            float val = source[src_idx];
            
            dest[i] = val;
            if (min) min[i] = val;
            if (max) max[i] = val;
        }
        return;
    }

    // CASE 2: DOWNSAMPLE (source is larger than display)
    float block_size = (float)source_len / (float)dest_len;

    for (int i = 0; i < dest_len; i++) {
        int start_idx = (int)(i * block_size);
        int end_idx = (int)((i + 1) * block_size);
        
        // Ensure we don't go out of bounds
        if (end_idx > source_len) end_idx = source_len;
        if (start_idx >= source_len) start_idx = source_len - 1;

        float sum = 0;
        int count = 0;
        float this_min = source[start_idx];
        float this_max = source[start_idx];

        for (int j = start_idx; j < end_idx; j++) {
            float val = source[j];
            sum += val;
            count++;
            if (val < this_min) this_min = val;
            if (val > this_max) this_max = val; // Fixed: was this_min
        }

        if (min) min[i] = this_min;
        if (max) max[i] = this_max;
        dest[i] = (count > 0) ? sum / (float)count : 0;
    }
}

void downsample_block_average(const float *source, int source_len, float *dest, int dest_len) {
  downsample_block_average_min_max(source, source_len, dest, dest_len, NULL, NULL);
}

#if 0
void scope_wave_update(const float *table, int size) {
  new_scope->wave_len = 0;
  downsample_block_average_min_max(table, size, new_scope->wave_data, SCOPE_WAVE_WIDTH, new_scope->wave_min, new_scope->wave_max);
  new_scope->wave_len = SCOPE_WAVE_WIDTH;
}
#endif

int wavetable_show(wire_t *w, int n) {
  if (n >= 0 && n < WAVE_TABLE_MAX && wave_table_data[n] && wave_size[n]) {
    float *table = wave_table_data[n];
    int readonly = wave_readonly[n];
    int refcount = wave_refcount[n];
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
    w->printf(w, "# w%d size:%d", n, size);
    w->printf(w, " rate:%g +hz:%g midi:%g",
      wave_rate[n],
      wave_offset_hz[n],
      wave_midi_note[n]);
    if (readonly) {
      w->printf(w, " r/o");
    } else {
      w->printf(w, " r/w ref:%d", refcount);
    }
    w->printf(w, " '%s'", wave_name[n]);
    w->puts(w, "");
    if (scope_enable) {
      downsample_block_average_min_max(table, size, scope->wave_data, SCOPE_WAVE_WIDTH, scope->wave_min, scope->wave_max);
      scope->wave_len = SCOPE_WAVE_WIDTH;
    }
  } else {
    w->printf(w, "# w%d nil\n", n);
  }
  return 0;
}

void wave_table_dynamic_expand(int n) {
  float fbig = 0.0;
  float fsmall = 0.0;
  int len = wave_size[n];
  float *samples = wave_table_data[n];
  if (len <= 0 || samples == NULL) {
    return;
  }
  for (int i = 0; i < len; i++) {
    float g = samples[i];
    if (g > fbig) fbig = g;
    if (g < fsmall) fsmall = g;
  }

  // use the min/max to make a scale factor that keeps 0
  // in the same relative place

  float scale;
  if (fabsf(fsmall) > fabsf(fbig)) {
    scale = -1.0f / fsmall;
  } else {
    scale = 1.0f / fbig;
  }

  // Convert scaled float samples to 16-bit PCM

  for (int i = 0; i < len; i++) {
    float g = samples[i];
    g *= scale;
    if (g > 1.0f) g = 1.0f;
    if (g < -1.0f) g = -1.0f;
    samples[i] = g;
  }
}

#include <sys/time.h>
#include <unistd.h>

int wire_function(skode_t *s, int info) {
  int atom = skode_atom_num(s);
  int argc = skode_arg_len(s);
  wire_t *w = (wire_t*)skode_user(s);
  double *arg = skode_arg(s);
  int voice = w->voice;
  int x = (int)arg[0];
  if (w->trace) {
    w->printf(w, "# WIRE_FUNCTION ");
    w->printf(w, "%s", skode_atom_string(s));
    if (argc) {
      for (int i=0; i<argc; i++) w->printf(w, " %g", arg[i]);
    }
    w->puts(w, "");
  }
  switch (atom) {
    case 'a___': if (argc) amp_set(voice, arg[0]); break;
    case 'A___': if (argc < 2) {
        amp_mod_set(voice, -1, 0);
      } else if (argc > 1) {
        amp_mod_set(voice, x, arg[1]);
      }
      break;
    case 'b___': if (argc == 0) { wave_dir(voice, -1); } else { wave_dir(voice, x); } break;
    case 'B___': if (argc == 0) { wave_loop(voice, -1); } else { wave_loop(voice, x); } break;
    case 'c___': if (argc == 0) {
        cz_set(voice, 0, .5);
      } else if (argc == 1) {
        cz_set(voice, x, .5);
      } else {
        cz_set(voice, x, arg[1]);
      }
      break;
    case 'C___': if (argc < 2) {
        cmod_set(voice, -1, 0);
      } else if (argc > 1) {
        cmod_set(voice, x, arg[1]);
      }
      break;
    case 'D___': // need to use the data array in skode here, not w->data
      break;
    case 'f___': if (argc) freq_set(voice, arg[0]); break;
    case 'F___': if (argc <= 1) {
        freq_mod_set(voice, -1, 0);
      } else if (argc > 1) {
        freq_mod_set(voice, x, arg[1]);
      }
      break;
    case 'g___': if (argc) {
        if (arg[0] <= 0) {
          voice_glissando_enable[voice] = 0;
        } else {
          voice_glissando_enable[voice] = 1;
          voice_glissando_speed[voice] = arg[0];
        }
      }
      break;
    case 'G___': if (argc) {
        voice_link_midi_a[voice] = x;
        if (argc > 1) voice_link_midi_b[voice] = (int)arg[1];
      }
      break;
    case 'h___': if (argc) { voice_sample_hold_max[voice] = x; } break;
    case 'H___': if (argc) {
        voice_link_velo_a[voice] = x;
        if (argc > 1) voice_link_velo_b[voice] = (int)arg[1];
      }
      break;
    // TODO re-allocate the data/array buffer with the arg
    case '/D__':
      if (argc) {
        // free and re-allocate...
        if (x > 0) skode_data_resize(w->sk, x);
      }
      w->printf(w, "# /D data %p cap %d len %d\n",
        skode_data(w->sk),
        skode_data_cap(w->sk),
        skode_data_len(w->sk));
      break;
    case 'I___': if (argc) {} break; // TODO en/dis-able send timestamp wire to the event logger
    case 'L___': if (argc) { voice_link_trig[voice] = x; } break;
    case 'J___': if (argc) {
        voice_filter_mode[voice] = x;
        mmf_set_params(voice,
          voice_filter_freq[voice],
          voice_filter_res[voice]);
      }
      break;
    case 'K___': if (argc) { mmf_set_freq(voice, arg[0]); } break;
    case 'k___': if (argc) { voice_amp_envelope_mode[voice] = x; } break;
    case 'l___': if (argc) {
        envelope_velocity(voice, arg[0]);
        if (voice_link_velo_a[voice] >= 0) envelope_velocity(voice_link_velo_a[voice], arg[0]);
        if (voice_link_velo_b[voice] >= 0) envelope_velocity(voice_link_velo_b[voice], arg[0]);
      }
      break;
    case 'm___': if (argc) { wave_mute(voice, x); } break;
    case 'M___': if (argc) { tempo_set(arg[0]); } break;
    case 'n___': if (argc) {
        float note = arg[0];
        if (isnan(note)) note = voice_last_midi_note[voice];
        freq_midi(voice, note);
        if (voice_link_midi_a[voice] >= 0) freq_midi(voice_link_midi_a[voice], note);
        if (voice_link_midi_b[voice] >= 0) freq_midi(voice_link_midi_b[voice], note);
      }
      break;
    case 'N___': if (argc) {
        if (isnan(arg[0])) {
          // do nothing
        } else {
          voice_midi_transpose[voice] = arg[0];
        }
        if (argc > 1) voice_midi_cents[voice] = arg[1];
      } break;
    case 'p___': if (argc) pan_set(voice, arg[0]); break;
    case 'P___': if (argc < 2) {
        pan_mod_set(voice, -1, 0);
      } else if (argc > 1) {
        pan_mod_set(voice, x, arg[1]);
      }
      break;
    case 'q___': if (argc) { wave_quant(voice, x); } break;
    case 'Q___': if (argc) { mmf_set_res(voice, arg[0]); } break;
    case 'r___': if (argc) { if (rec_state == 0) voice_record[voice] = x; } break;
    case 's___': if (argc) {
        if (arg[0] <= 0) {
          voice_smoother_enable[voice] = 0;
        } else {
          voice_smoother_enable[voice] = 1;
          voice_smoother_smoothing[voice] = arg[0];
        }
      }
      break;
    case 'S___': if (argc) wave_reset(voice, x); break;
    case 't___': if (argc > 3) envelope_set(voice, arg[0], arg[1], arg[2], arg[3]); break;
    case 'T___': {
#if 1
        envelope_velocity(voice, 1);
        if (voice_link_velo_a[voice] >= 0) envelope_velocity(voice_link_velo_a[voice], 1);
        if (voice_link_velo_b[voice] >= 0) envelope_velocity(voice_link_velo_b[voice], 1);
#else
        voice_trigger(voice);
        if (voice_link_trig[voice] > 0) voice_trigger(voice_link_trig[voice]);
#endif
      }
      break;
    case 'v___': if (argc) voice_set(x, &w->voice); break;
    case 'V___': if (argc) volume_set(arg[0]); break;
    case 'w___': if (argc) {
        wave_set(voice, x);
        if (scope_enable) sprintf(scope->wave_text, "w%d", x);
      }
      break;
    case 'W___': if (argc) {
        wavetable_show(w,x);
        if (scope_enable) sprintf(scope->wave_text, "w%d", x);
      } else if (argc == 0) {
        int c = 0;
        for (int i=0; i<WAVE_TABLE_MAX; i++) {
          if (wave_table_data[i] && wave_readonly[i] == 0) {
            wavetable_show(w, i);
            c++;
          }
        }
        if (scope_enable) sprintf(scope->wave_text, "%d waves loaded", c);
      }
      break;
    case 'x___': if (argc) {
        if (arg[0] == NAN || x < 0) {
          w->step++;
          x = w->step;
        } else {
          w->step = x;
        }
        if (x >= 0 && x < SEQ_STEPS_MAX) {
          seq_step_set(w->pattern, w->step, skode_string(w->sk));
        }
      }
      break;
    case 'y___': if (argc) {
        w->pattern = x;
        scope_pattern_pointer = x;
      }
      break;
    case 'z___': if (argc) {
        seq_state_set(w->pattern, x);
      } else pattern_show(w, w->pattern);
      break;
    case 'Z___': if (argc) {
        seq_state_all(x);
      } else {
        w->printf(w, "; M%g\n", tempo_bpm * 4.0f);
        for (int p = 0; p < PATTERNS_MAX; p++) pattern_show(w, p);
      }
      break;
    case '?___': voice_show(w, voice, ' ', w->verbose); break;
    case '\\___': voice_show(w, voice, ' ', 1); break;
    case '??__': voice_show_all(w, voice, w->verbose); break;
    case '?s__':
      {
        w->printf(w, "# %s\n", skode_string(w->sk));
      }
      break;
    case 'l>g_': if (argc) skode_local_to_global(w->sk, x); break;
    case 'g>l_': if (argc) skode_global_to_local(w->sk, x); break;
    case '/m__': synth_voice_bench(voice); break;
    case '/q__': w->quit = -1; return 0;
    case '/d__': {
        int wave_slot = EXT_SAMPLE_000;
        int one_shot = 0;
        float rate = 44100.0;
        float offset = 0.0;
        if (argc) wave_slot = (int)arg[0];
        if (argc > 1) rate = arg[1];
        if (argc > 2) rate = arg[2];
        if (argc > 3) offset = arg[3];
        data_load(w, wave_slot, one_shot, rate, offset);
      }
      break;
    case '/f__':
      if (argc) { w->flag = x; }
      else { w->printf(w, "# /f%d\n", w->flag); }
      break;
    case '/c__':
      if (argc) { skode_chunk_mode(w->sk, x); }
      else { w->printf(w, "# /c%d\n", skode_chunk_mode_get(w->sk)); }
      break;
    case '/t__': if (argc == 0) x = (w->trace) ? 0 : 1;
      w->trace = x;
      skode_trace_set(s, x > 1);
      break;
    case '/v__': if (argc == 0) x = (w->verbose) ? 0 : 1;
      w->verbose = x;
      break;
    case '/s__': {
        system_show(w);
        show_threads(w);
        audio_show(w);
        w->printf(w, "%s", synth_stats());
      }
      break;
    case '/S__': {
        show_stats(w);
        wire_show(w);
      }
      break;
    case '/o__': scope_enable = x; break;
              // sub x for scope_cross = 1
              // sub q for scope_quit = 0
              // sub 0..VOICE_MAX-1 for scope_channel = n
              // sub -1 for scope_channel = -1 (all channels)
    case '/l__': if (argc) { sk_load(w, voice, x); } break;
    case '/w__': {
        int file_num = 0;
        int wave_slot = EXT_SAMPLE_000;
        int ch = -1;
        if (argc >= 2) {
          file_num = (int)arg[0];
          wave_slot = (int)arg[1];
          if (argc > 2) ch = (int)arg[2];
        } else if (argc == 1) {
          file_num = (int)arg[0];
          wave_slot = EXT_SAMPLE_000;
        }
        if (argc) wave_load(w, file_num, wave_slot, ch, 1);
      }
      break;
    case '<___': if (arg) {
        rec_state = 0;
        float max_sec = arg[0];
        float max_samples;
        if (max_sec > 0.0f) {
          if (max_sec > rec_sec) {
            max_sec = rec_sec;
          }
          max_samples = max_sec * (float)(MAIN_SAMPLE_RATE * AUDIO_CHANNELS * VOICE_MAX);
          rec_max = max_samples;
        }
        rec_ptr = 0;
        rec_state = 1;
      }
      break;
    case '*___': {
        #include <sys/time.h>
        #include <unistd.h>
        if (rec_ptr) {
          rec_state = 0;
          pid_t pid = getpid();
          struct timeval tv;
          gettimeofday(&tv, NULL);
          long long ms = (long long)tv.tv_sec * 1000 + tv.tv_usec / 1000;
          char name[1024];
#ifdef _WIN32
          sprintf(name, "skred-%lld-%lld.wav", pid, ms);
#else
          sprintf(name, "skred-%d-%lld.wav", pid, ms);
#endif
          w->printf(w, "# file %s (%ld frames)\n", name, rec_ptr);
          save_wav(w, name, recording, rec_ptr/VOICE_MAX/AUDIO_CHANNELS, voice_record, VOICE_MAX);
        }
      }
      break;
    case '>___': if (arg) voice_copy(voice, x); break;
    case '/___': wave_default(voice); break;
    case '%___': if (arg) seq_modulo_set(w->pattern, x); break;
    case '!___': if (arg) seq_mute_set(w->pattern, x, 0); break;
    case '@___': if (arg) seq_mute_set(w->pattern, x, 1); break;
    case '=___': if (argc>1) skode_set_local(w->sk, x, arg[1]); break;
    case '/wex': if (argc && x >= 200 && x <=999) wave_table_dynamic_expand(x);
    default:
      if (w->trace) {
        w->printf(w, "# WIRE_UNKNOWN_FUNCTION %d [%x] :: %d", info, atom, argc);
        w->printf(w, " v%d", w->voice);
        w->puts(w, "");
      }
      break;
  }
  return 0;
}

int wire_defer(skode_t *s, int info) {
  wire_t *w = (wire_t*)skode_user(s);
  if (w->defer_sample_time == 0) w->defer_sample_time = synth_sample_count;
  uint64_t dst = w->defer_sample_time;
  char mode = skode_defer_mode(s);
  float t = skode_defer_num(s);
  if (mode == '+') t *= (tempo_time_per_step * 4.0f);
  t += w->defer_last;
  uint64_t qt = (uint64_t)(t * (float)MAIN_SAMPLE_RATE) + dst;
  if (w->trace) {
#ifdef _WIN32
    w->printf(w, "# WIRE_DEFER %c %g(%lld/%lld) '%s' (%g)\n",
#else
    w->printf(w, "# WIRE_DEFER %c %g(%ld/%ld) '%s' (%g)\n",
#endif
      mode,
      t, qt, dst,
      skode_defer_string(s),
      w->defer_last);
  }
  queue_item(qt, skode_defer_string(s), w->voice);
  w->defer_last += skode_defer_num(s);
  return 0;
}

int wire_chunk_end(skode_t *s, int info) {
  wire_t *w = (wire_t*)skode_user(s);
  if (w->trace) w->printf(w, "# CHUNK_END %d\n", info);
  w->defer_last = 0;
  w->defer_sample_time = 0;
  return 0;
}

int wire_unknown(wire_t *w, skode_t *s, int info) {
  w->printf(w, "# WIRE_UNKNOWN %d\n", info);
  return 0;
}

int wire_cb(skode_t *s, int info) {
  wire_t *w = (wire_t*)skode_user(s);
  switch (info) {
    case FUNCTION: return wire_function(s, info);
    case DEFER: return wire_defer(s, info);
    case CHUNK_END: return wire_chunk_end(s, info);
    case PUSH: { voice_push(&w->stack, w->voice); w->printf(w, "pushed v%d\n", w->voice); } break;
    case POP: { w->voice = voice_pop(&w->stack); } break;
    case GOT_STRING: { if (w->trace) w->printf(w, "# -> {%s}\n", skode_string(s)); } break;
    case GOT_ARRAY: { if (w->trace) w->printf(w, "# -> (..%d..)\n", skode_data_len(s)); } break;
    default: return wire_unknown(w, s, info);
  }
  return 0;
}

double global_var[10];

int wire(char *line, wire_t *w) {
  if (w->sk == NULL) {
    // TODO this should live in wire-init or similar
    w->sk = skode_new(wire_cb, (void *)w);
    skode_set_global(w->sk, global_var);
  }
  w->log_len = 0;
  w->log[0] = '\0';
  wl[wire_hash(w)] = w;

  if (w->events) mpsc_queue_send(&mq, line);

  int r = 0;

  skode(w->sk, line, wire_cb);
  return w->quit;
  return r;
}

int audio_show(wire_t *w) {
  wire_t wprime;
  if (w == NULL) {
    w = &wprime;
    wire_init(w);
  }
  w->printf(w, "# synth backend is running\n");
  w->printf(w, "# synth total voice count %d\n", VOICE_MAX);
  int active = 0;
  for (int i = 0; i < VOICE_MAX; i++) if (voice_amp[i] != 0) active++;
  w->printf(w, "# synth active voice count %d\n", active);
#ifdef _WIN32
  w->printf(w, "# synth sample count %lld\n", synth_sample_count);
#else
  w->printf(w, "# synth sample count %ld\n", synth_sample_count);
#endif
  return 0;
}

void wire_init(wire_t *w) {
  static int first = 1;
  if (first) {
    for (int i = 0; i < WIRE_POINTER_MAX; i++) {
      wl[i] = NULL;
    }
    first = 0;
  }
  w->voice = 0;
  w->pattern = 0;
  w->step = -1;
  w->trace = 0;
  w->verbose = 0;
  w->events = 0;
  w->sk = NULL;
  w->quit = 0;
  w->puts = wire_puts;
  w->printf = wire_printf;
  w->log_enable = 0;
  w->log_max = 4096;
  w->log_len = 0;
  w->log[0] = '\0';
}
