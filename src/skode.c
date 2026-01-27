#include "skred.h"
#include "skode.h"
#ifndef MINI
#include "seq.h"
#endif
#include "miniwav.h"

#include "synth-types.h"
#include "synth.h"

#include <stdarg.h>
#include <stdio.h>

int skode_puts(skode_t *ctx, const char *s) {
  if (!ctx || ctx->log_enable == 0) return 0;
  if (ctx->log_len + strlen(s) >= SKODE_LOG_MAX) return 0;
  strncat(ctx->log, s, ctx->log_max);
  strncat(ctx->log, "\n", ctx->log_max);
  ctx->log_len = strlen(ctx->log);
  return 0;
}

int skode_printf(skode_t *ctx, const char *fmt, ...) {
  if (!ctx || ctx->log_enable == 0) return 0;
  if (ctx->log_len + strlen(fmt) >= SKODE_LOG_MAX) return 0;
  //puts("PRINTF");
  char buf[SKODE_LOG_MAX + 1024];
  va_list ap;
  va_start(ap, fmt);
  int len = vsnprintf(buf, sizeof(buf), fmt, ap);
  va_end(ap);
  if (len > 0) {
    size_t out_len = (len >= (int)sizeof(buf)) ? sizeof(buf)-1 : (size_t)len;
    if (out_len) {
      strncat(ctx->log, buf, ctx->log_max);
      ctx->log_len = strlen(ctx->log);
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

void save_wav(skode_t *ctx, char *filename, float *samples, long num_samples, int *record, int max) {
  int *record_safe;

  record_safe = (int *)malloc(sizeof(int)*max);
  if (record_safe == NULL) {
    ctx->puts(ctx, "OUCH"); return;
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
    ctx->puts(ctx, "# can't open file\n");
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

void voice_show(skode_t *ctx, int v, char c, int verbose) {
  char s[1024];
  char e[8] = "";
  if (c != ' ') sprintf(e, " # *");
  voice_format(v, s, verbose);
  if (strlen(s)) ctx->printf(ctx, "; %s%s\n", s, e);
}

int voice_show_all(skode_t *ctx, int voice, int verbose) {
  for (int i=0; i<VOICE_MAX; i++) {
    if (voice_amp[i] == 0) continue;
    char t = ' ';
    if (i == voice) t = '*';
    voice_show(ctx, i, t, verbose);
  }
  return 0;
}

#define SKODE_POINTER_MAX (100)
static skode_t *_skode_all[SKODE_POINTER_MAX];

#define STRING_BUF_LEN (256)
#define STRING_BUF_IDX_MAX (128) // idea one macro per midi key?
static char _skode_extra[STRING_BUF_IDX_MAX][STRING_BUF_LEN];
#define EXTRA_PTR(n) _skode_extra[n % STRING_BUF_IDX_MAX]
#define EXTRA_INIT() {for (int i=0; i<STRING_BUF_IDX_MAX; i++) EXTRA_PTR(i)[0] = '\0';}

int skode_hash(skode_t *ctx) {
  uintptr_t addr = (uintptr_t)ctx;
  addr *= 2654435769u; // knuth's multiplicitive hash (based on golden thingy?)
  return addr % SKODE_POINTER_MAX;
}

void skode_show(skode_t *ctx) {
  if (ctx != NULL) {
    ctx->printf(ctx, "# voice %d\n", ctx->voice);
    ctx->printf(ctx, "# pattern %d\n", ctx->pattern);
    ctx->printf(ctx, "# scratch %s\n", ands_string(ctx->parse));
    ctx->printf(ctx, "( ");
    int flag = 1;
    int show_dots = 0;
    double *data = ands_data(ctx->parse);
    int data_len = ands_data_len(ctx->parse);
#define DOT_NUM (3)
    for (int i = 0; i < data_len; i++) {
      if (i < DOT_NUM) {
        show_dots = 0;
        ctx->printf(ctx, "%.8g ", data[i]);
      } else if (i >= (data_len - DOT_NUM)) {
        show_dots = 0;
        ctx->printf(ctx, "%.8g ", data[i]);
      } else {
        show_dots = 1;
      }
      if (flag && show_dots) {
        flag = 0; // only once
        ctx->printf(ctx, " ... ");
      }
    }
    ctx->printf(ctx, ") # %d elements\n", data_len);
  }
  for (int i = 0; i < SKODE_POINTER_MAX; i++) {
    if (_skode_all[i]) {
      ctx->printf(ctx, "# wl/%d ", skode_hash(_skode_all[i]));
      ctx->printf(ctx, " .voice=%d", _skode_all[i]->voice);
      ctx->printf(ctx, " .pattern=%d", _skode_all[i]->pattern);
      ctx->printf(ctx, " .step=%d", _skode_all[i]->step);
      ctx->printf(ctx, " .events=%dn", _skode_all[i]->events);
      ctx->printf(ctx, "\n");
    }
  }
}

#include "udp.h"

void system_show(skode_t *ctx) {
  skode_t wprime;
  if (ctx == NULL) {
    ctx = &wprime;
    skode_init(ctx);
  }
#ifndef MINI
  ctx->printf(ctx, "# udp_port %d\n", udp_info());
#endif
}

#ifndef MINI
int show_stats_cb(int n, uint64_t timestamp, uint64_t id, int tag, const event_t *e, void *user) {
  uint64_t now = SAMPLE_COUNT_GET();
  uint64_t then = timestamp - now;
  double ms = (double)then / (double)MAIN_SAMPLE_RATE * 1000.0;
  skode_t *ctx = user;
  ctx->printf(ctx, "# (%d,%ld,%d) %ld +%g ms %d {%s}\n",
    n,
    id,
    tag,
    timestamp,
    ms,
    e->voice,
    e->what
  );
  return 0;
}

void show_stats(skode_t *ctx) {
  ctx->printf(ctx, "# rec_state : %d rec_ptr %ld\n", rec_state, rec_ptr);
  ctx->printf(ctx, "# synth frames per callback %d : %gms\n",
    synth_frames_per_callback, (float)synth_frames_per_callback / (float)MAIN_SAMPLE_RATE * 1000.0f);
  ctx->printf(ctx, "# seq frames per callback %d : %gms\n",
    seq_frames_per_callback, (float)seq_frames_per_callback / (float)MAIN_SAMPLE_RATE * 1000.0f);
  ctx->printf(ctx, "# queue_size %d\n", seq_queued());
  seq_foreach(show_stats_cb, ctx);
}
#endif

#ifdef _WIN32
#include <windows.h>
#include <tlhelp32.h>
#include <processthreadsapi.h>
#endif

void show_threads(skode_t *ctx) {
  skode_t wprime;
  if (ctx == NULL) {
    ctx = &wprime;
    skode_init(ctx);
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
            ctx->printf(ctx, "# %lu <GetThreadDescription failed>\n", te32.th32ThreadID);
          } else if (threadName == NULL || wcslen(threadName) == 0) {
            ctx->printf(ctx, "# %lu <unnamed>\n", te32.th32ThreadID);
          } else {
            char narrowName[256];
            WideCharToMultiByte(CP_UTF8, 0, threadName, -1, narrowName, sizeof(narrowName), NULL, NULL);
            ctx->printf(ctx, "# %lu %s\n", te32.th32ThreadID, narrowName);
            LocalFree(threadName);
          }
          CloseHandle(hThread);
        } else {
          ctx->printf(ctx, "# %lu <cannot open thread>\n", te32.th32ThreadID);
        }
      }
    } while (Thread32Next(hSnapshot, &te32));
  }

  CloseHandle(hSnapshot);
#else
#ifndef __APPLE__
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
    ctx->printf(ctx, "# %s %s\n", entry->d_name, name);
  }

  closedir(dir);
#endif
#endif
}

int skode_load(skode_t *ctx, int voice, int n) {
  skode_t wprime;
  if (ctx == NULL) {
    ctx = &wprime;
    skode_init(ctx);
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
    static skode_t wprime = SKODE_EMPTY();
    char line[1024];
    while (fgets(line, sizeof(line), in) != NULL) {
      size_t len = strlen(line);
      if (len > 0 && line[len-1] == '\n') line[len-1] = '\0';
      ctx->printf(ctx, "# %s\n", line);
      r = skode_consume(line, &wprime);
      if (r != 0) {
        ctx->printf(ctx, "# error in patch\n");
        break;
      }
    }
    fclose(in);
  }
  return r;
}

int data_load(skode_t *ctx, int wave_slot, int one_shot, float rate, float offset) {
  if (ctx == NULL) return 100; // fix todo
  ctx->printf(ctx, "# data_load(ctx, %d, %d, %g, %g)\n", wave_slot, one_shot, rate, offset);
  if (wave_slot < 0 || wave_slot >= EXT_SAMPLE_999) {
    ctx->printf(ctx, "# invalid slot %d\n", wave_slot);
    return -1;
  }
  double *data = ands_data(ctx->parse);
  int data_len = ands_data_len(ctx->parse);
  if (data == NULL) {
    ctx->printf(ctx, "# no data\n");
    return 100; // fix todo
  }
  if (data_len <= 0) {
    ctx->printf(ctx, "# no data len\n");
    return 100;
  }
  if (wave_readonly[wave_slot] == 1) {
    ctx->printf(ctx, "# cannot write to w%d r/o\n", wave_slot);
    return -1;
  }
  if (rate <= 0) {
    ctx->printf(ctx, "# invalid rate %g > 0\n", rate);
    return -1;
  }
  if (wave_refcount[wave_slot] > 0) {
    ctx->printf(ctx, "# cannot write to w%d ref > 0\n", wave_slot);
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
    ctx->printf(ctx, "# read %d frames from %s to %d (ch:%d sr:%d)\n", len, name, wave_slot, channels, 44100);
  return 0;
}

int wave_load(skode_t *ctx, int file_num, int wave_index, int ch, int normalize) {
  if (ctx == NULL) return 100; // fix todo
  if (wave_index < EXT_SAMPLE_000 || wave_index >= EXT_SAMPLE_999) return -1;
  if (wave_readonly[wave_index] == 1) {
    ctx->printf(ctx, "# cannot write to w%d r/o\n", wave_index);
    return -1;
  }
  if (wave_refcount[wave_index] > 0) {
    ctx->printf(ctx, "# cannot write to w%d ref > 0\n", wave_index);
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
      ctx->printf(ctx, "# cannot open %d.wav or wav/%d.wav\n", file_num, file_num);
      return -1;
    }
  }
  wav_t wav;
  int len;
  char out[4096];
  float *table = mw_get_str(name, &len, &wav, ch, out, sizeof(out));
  if (table == NULL) {
    ctx->printf(ctx, "# can not read %s\n", name);
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
    ctx->printf(ctx, "# read %d frames from %s to %d (ch:%d sr:%d)\n",
      len, name, wave_index, wav.Channels, wav.SamplesRate);
    normalize_preserve_zero(table, len);
  }
  return 0;
}


// this is a mess i need to clean up

#ifndef MINI
#include "scope-shared.h"
extern int scope_enable;
extern scope_buffer_t *scope;

void pattern_show(skode_t *ctx, int pattern_pointer) {
  int first = 1;
  for (int s = 0; s < SEQ_STEPS_MAX; s++) {
    char *line = seq_pattern[pattern_pointer][s];
    if (strlen(line) == 0) break;
    if (first) {
      ctx->printf(ctx, "; y%d %%%d # step %d\n",
        pattern_pointer, seq_modulo[pattern_pointer], ctx->step);
      first = 0;
    }
    ctx->printf(ctx, "; {%s} x%d", line, s);
    if (seq_pattern_mute[pattern_pointer][s]) ctx->printf(ctx, " @%d", pattern_pointer);
    ctx->puts(ctx, "");
  }
}

void tempo_set(float m);
#endif

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

int wavetable_show(skode_t *ctx, int n) {
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
    ctx->printf(ctx, "# w%d size:%d", n, size);
    ctx->printf(ctx, " rate:%g +hz:%g midi:%g",
      wave_rate[n],
      wave_offset_hz[n],
      wave_midi_note[n]);
    if (readonly) {
      ctx->printf(ctx, " r/o");
    } else {
      ctx->printf(ctx, " r/w ref:%d", refcount);
    }
    ctx->printf(ctx, " '%s'", wave_name[n]);
    ctx->puts(ctx, "");
#ifndef MINI
    if (scope_enable) {
      downsample_block_average_min_max(table, size, scope->wave_data, SCOPE_WAVE_WIDTH, scope->wave_min, scope->wave_max);
      scope->wave_len = SCOPE_WAVE_WIDTH;
    }
#endif
  } else {
    ctx->printf(ctx, "# w%d nil\n", n);
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

int skode_function(ands_t *s, int info) {
  int atom = ands_atom_num(s);
  int argc = ands_arg_len(s);
  skode_t *ctx = (skode_t*)ands_user(s);
  double *arg = ands_arg(s);
  int voice = ctx->voice;
  int x = (int)arg[0];
  if (ctx->trace) {
    ctx->printf(ctx, "# SKODE_FUNCTION ");
    ctx->printf(ctx, "%s", ands_atom_string(s));
    if (argc) {
      for (int i=0; i<argc; i++) ctx->printf(ctx, " %g", arg[i]);
    }
    ctx->puts(ctx, "");
  }
  switch (atom) {
    case 'a___': // amp loudness
      if (argc) amp_set(voice, arg[0]);
      break;
    case 'A___': // AM voice depth
      if (argc < 2) {
        amp_mod_set(voice, -1, 0);
      } else if (argc > 1) {
        amp_mod_set(voice, x, arg[1]);
      }
      break;
    case 'b___': // wave-direction bool
      if (argc == 0) { wave_dir(voice, -1); } else { wave_dir(voice, x); } break;
    case 'B___': // wave-loop bool
      if (argc == 0) { wave_loop(voice, -1); } else { wave_loop(voice, x); } break;
    case 'c___': // phase-distortion algo distortion
      if (argc == 0) {
        cz_set(voice, 0, .5);
      } else if (argc == 1) {
        cz_set(voice, x, .5);
      } else {
        cz_set(voice, x, arg[1]);
      }
      break;
    case 'C___': // PD-mod voice depth
      if (argc < 2) {
        cmod_set(voice, -1, 0);
      } else if (argc > 1) {
        cmod_set(voice, x, arg[1]);
      }
      break;
    case 'D___': // data-size
      // need to use the data array in skode here, not ctx->data
      break;
    case 'f___': // freq hz
      if (argc) freq_set(voice, arg[0]);
      break;
    case 'F___': // FM voice depth
      if (argc <= 1) {
        freq_mod_set(voice, -1, 0);
      } else if (argc > 1) {
        freq_mod_set(voice, x, arg[1]);
      }
      break;
    case 'g___': // glissando speed
      if (argc) {
        if (arg[0] <= 0) {
          voice_glissando_enable[voice] = 0;
        } else {
          voice_glissando_enable[voice] = 1;
          voice_glissando_speed[voice] = arg[0];
        }
      }
      break;
    case 'G___': // link-midi voice [voice]
      if (argc) {
        voice_link_midi_a[voice] = x;
        if (argc > 1) voice_link_midi_b[voice] = (int)arg[1];
      }
      break;
    case 'h___': // sample-hold phase-count
      if (argc) { voice_sample_hold_max[voice] = x; } break;
    case 'H___': // link-velo voice [voice]
      if (argc) {
        voice_link_velo_a[voice] = x;
        if (argc > 1) voice_link_velo_b[voice] = (int)arg[1];
      }
      break;
    // TODO re-allocate the data/array buffer with the arg
    case '/D__': // resize-data count
      if (argc) {
        // free and re-allocate...
        if (x > 0) ands_data_resize(ctx->parse, x);
      }
      ctx->printf(ctx, "# /D data %p cap %d len %d\n",
        ands_data(ctx->parse),
        ands_data_cap(ctx->parse),
        ands_data_len(ctx->parse));
      break;
    case 'I___': // log-event bool
      if (argc) {} break; // TODO en/dis-able send timestamp wire to the event logger
    case 'L___': // link-trigger voice
      if (argc) { voice_link_trig[voice] = x; } break;
    case 'J___': // filter-mode selector
      if (argc) {
        voice_filter_mode[voice] = x;
        mmf_set_params(voice,
          voice_filter_freq[voice],
          voice_filter_res[voice]);
      }
      break;
    case 'K___': // filter-cutoff freq
      if (argc) { mmf_set_freq(voice, arg[0]); } break;
    case 'k___': // adsr-mode bool
      if (argc) { voice_amp_envelope_mode[voice] = x; } break;
    case 'udp_': // show-udp
      if (argc) {
        ctx->printf(ctx, "# udp [%d] %d/%d\n", ctx->which, ctx->ip, ctx->port);
      }
      break;
    case 'log_': // log-enable bool
      if (argc) {
        if (x) { ctx->log_enable = 1; } else { ctx->log_enable = 0; }
      }
      break;
    case 'l___': // velocity amount
      if (argc) {
        envelope_velocity(voice, arg[0]);
        if (voice_link_velo_a[voice] >= 0) envelope_velocity(voice_link_velo_a[voice], arg[0]);
        if (voice_link_velo_b[voice] >= 0) envelope_velocity(voice_link_velo_b[voice], arg[0]);
      }
      break;
    case 'm___': // mute-audio bool
      if (argc) { wave_mute(voice, x); }
      break;
#ifndef MINI
    case 'M___': // tempo bpm
      if (argc) { tempo_set(arg[0]); }
      break;
#endif
    case 'n___': // midi-freq note-number
      if (argc) {
        float note = arg[0];
        if (isnan(note)) note = voice_last_midi_note[voice];
        freq_midi(voice, note);
        if (voice_link_midi_a[voice] >= 0) freq_midi(voice_link_midi_a[voice], note);
        if (voice_link_midi_b[voice] >= 0) freq_midi(voice_link_midi_b[voice], note);
      }
      break;
    case 'N___': // detune-midi key cents
      if (argc) {
        if (isnan(arg[0])) {
          // do nothing
        } else {
          voice_midi_transpose[voice] = arg[0];
        }
        if (argc > 1) voice_midi_cents[voice] = arg[1];
      }
      break;
    case 'p___': // pan value
      if (argc) pan_set(voice, arg[0]);
      break;
    case 'P___': // pan-mod voice depth
      if (argc < 2) {
        pan_mod_set(voice, -1, 0);
      } else if (argc > 1) {
        pan_mod_set(voice, x, arg[1]);
      }
      break;
    case 'q___':  // bit-crush bit-depth
      if (argc) { wave_quant(voice, x); }
      break;
    case 'Q___':
      if (argc) { mmf_set_res(voice, arg[0]); }
      break;
#ifndef MINI
    case 'r___': // record-mode bool
      if (argc) { if (rec_state == 0) voice_record[voice] = x; }
      break;
    case 'R!__':  // remove-events tag
      if (argc) {
        int tag = x;
        seq_kill_by_tag(tag);
      }
      break;
    case 'R!!_':
      seq_kill_all();
      break;
    case 'R\'__': // repeat-string-tempo count delay [tag]
      if (argc > 1) {
        uint64_t qt = SAMPLE_COUNT_GET();
        double t = (tempo_time_per_step * 4.0f);
        double fdt = t * arg[1] * (float)MAIN_SAMPLE_RATE;
        uint64_t dt = (uint64_t)fdt;
        int tag = 0;
        if (argc > 2) tag = (int)arg[2];
        for (int i=0; i<x; i++) {
          queue_item(qt, ands_string(ctx->parse), ctx->voice, tag);
          qt += dt;
        }
      } break;
    case 'R___': // repeat-string count delay [tag]
      if (argc > 1) {
        uint64_t qt = SAMPLE_COUNT_GET();
        double fdt = arg[1] * (float)MAIN_SAMPLE_RATE;
        uint64_t dt = (uint64_t)fdt;
        int tag = 0;
        if (argc > 2) tag = (int)arg[2];
        for (int i=0; i<x; i++) {
          queue_item(qt, ands_string(ctx->parse), ctx->voice, tag);
          qt += dt;
        }
      } break;
#endif
    case 's___': // volume-smooth bool
      if (argc) {
        if (arg[0] <= 0) {
          voice_smoother_enable[voice] = 0;
        } else {
          voice_smoother_enable[voice] = 1;
          voice_smoother_smoothing[voice] = arg[0];
        }
      }
      break;
    case 'S___': // voice-reset voice
      if (argc) wave_reset(voice, x);
      break;
    case 't___': // adsr-set attack decay sustain release
      if (argc > 3) envelope_set(voice, arg[0], arg[1], arg[2], arg[3]);
      break;
    case 'T___': // trigger
      {
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
    case 'v___': // voice-select voice
      if (argc) voice_set(x, &ctx->voice);
      break;
    case 'V___': // main-volume loudness
      if (argc) volume_set(arg[0]);
      break;
    case 'w___': // wave-select which-wave
      if (argc) {
        wave_set(voice, x);
#ifndef MINI
        if (scope_enable) sprintf(scope->wave_text, "w%d", x);
#endif
      }
      break;
    case 'W___': // wave-show which-wave
      if (argc) {
        wavetable_show(ctx,x);
#ifndef MINI
        if (scope_enable) sprintf(scope->wave_text, "w%d", x);
#endif
      } else if (argc == 0) {
        int c = 0;
        for (int i=0; i<WAVE_TABLE_MAX; i++) {
          if (wave_table_data[i] && wave_readonly[i] == 0) {
            wavetable_show(ctx, i);
            c++;
          }
        }
#ifndef MINI
        if (scope_enable) sprintf(scope->wave_text, "%d waves loaded", c);
#endif
      }
      break;
#ifndef MINI
    case 'x___': // set-step-string step
      if (argc) {
        if (arg[0] == NAN || x < 0) {
          ctx->step++;
          x = ctx->step;
        } else {
          ctx->step = x;
        }
        if (x >= 0 && x < SEQ_STEPS_MAX) {
          seq_step_set(ctx->pattern, ctx->step, ands_string(ctx->parse));
        }
      }
      break;
    case 'y___': // select-pattern which
      if (argc) {
        ctx->pattern = x;
        scope_pattern_pointer = x;
      }
      break;
    case 'Y___': // clear-pattern which
      if (argc && x >= 0 && x < PATTERNS_MAX) {
        pattern_reset(x);
      }
      break;
    case 'z___': // one-pattern-play-mode bool
      if (argc) {
        seq_state_set(ctx->pattern, x);
      } else pattern_show(ctx, ctx->pattern);
      break;
    case 'Z___': // all-pattern-play-mode bool
      if (argc) {
        seq_state_all(x);
      } else {
        ctx->printf(ctx, "; M%g\n", tempo_bpm * 4.0f);
        for (int p = 0; p < PATTERNS_MAX; p++) pattern_show(ctx, p);
      }
      break;
#endif
    case '?___': // show-voice
      voice_show(ctx, voice, ' ', ctx->verbose); break;
    case '\\___': // verbose-show-voice
      voice_show(ctx, voice, ' ', 1); break;
    case '??__': // show-active-voices
      voice_show_all(ctx, voice, ctx->verbose); break;
    case '?s__': // show-skode-string
      {
        ctx->printf(ctx, "# {%}s\n", ands_string(ctx->parse));
      }
      break;
    case 'l>g_':
      if (argc) ands_local_to_global(ctx->parse, x);
      break;
    case 'g>l_':
      if (argc) ands_global_to_local(ctx->parse, x);
      break;
    case '/m__': // benchmark voice
      synth_voice_bench(voice);
      break;
    case '/q__': // quit
      ctx->quit = -1;
      return 0;
    case '/d__': // data-to-wave slot rate rate offset
      {
        int wave_slot = EXT_SAMPLE_000;
        int one_shot = 0;
        float rate = 44100.0;
        float offset = 0.0;
        if (argc) wave_slot = (int)arg[0];
        if (argc > 1) rate = arg[1];
        if (argc > 2) rate = arg[2];
        if (argc > 3) offset = arg[3];
        data_load(ctx, wave_slot, one_shot, rate, offset);
      }
      break;
    case '/f__': // flag-mode num
      if (argc) { ctx->flag = x; }
      else { ctx->printf(ctx, "# /f%d\n", ctx->flag); }
      break;
    case '/c__': // chunk-mode bool
      if (argc) { ands_chunk_mode(ctx->parse, x); }
      else { ctx->printf(ctx, "# /c%d\n", ands_chunk_mode_get(ctx->parse)); }
      break;
    case '/t__': // trace-mode num
      if (argc == 0) x = (ctx->trace) ? 0 : 1;
      ctx->trace = x;
      ands_trace_set(s, x > 1);
      break;
    case '/v__': // verbose-mode num
      if (argc == 0) x = (ctx->verbose) ? 0 : 1;
      ctx->verbose = x;
      break;
    case '<e__': // external-string-to-skode external-index
      if (arg == 0) {
      } else {
        //char *s = ands_string_from_extra(ctx->parse, x);
        char *s = ands_string_from_external(ctx->parse, EXTRA_PTR(x), STRING_BUF_LEN);
        ctx->printf(ctx, "# %s <- [%d]\n", s, x);
      }
      break;
    case 'e>__': // skode-string-to-external external-index
      if (arg == 0) {
      } else {
        char *s = ands_string(ctx->parse);
        //ands_string_to_extra(ctx->parse, x, s);
        //ands_string_to_external(ctx->parse, EXTRA_PTR(x), STRING_BUF_LEN);
        strncpy(EXTRA_PTR(x), s, STRING_BUF_LEN);
      }
      break;
    case 'e!__': // execute-string num
      {
        char *s = "";
        if (arg == 0) {
          s = ands_string(ctx->parse);
        } else {
          //s = ands_string_from_extra(ctx->parse, x);
          s = _skode_extra[x % STRING_BUF_IDX_MAX];
        }
        uint64_t now = SAMPLE_COUNT_GET();
        int tag = 0;
        queue_item(now, s, voice, tag);
      }
      break;
    case '/s__': // system-show num
      {
        if (argc == 0) {
          system_show(ctx);
        } else {
          switch (x) {
            default:
            case 0: system_show(ctx); break;
            case 2: audio_show(ctx); break;
            case 3: ctx->printf(ctx, "%s", synth_stats()); break;
            case 5: skode_show(ctx); break;
            case 7:
              ctx->printf(ctx, "# {%s}\n", ands_string(ctx->parse));
              for (int i=0; i<STRING_BUF_IDX_MAX; i++) {
                //ctx->printf(ctx, "# [%d] %s\n", i, ands_extra(ctx->parse, i));
                if (strlen(EXTRA_PTR(i))) ctx->printf(ctx, "# [%d] %s\n", i, EXTRA_PTR(i));
              }
              break;
#ifndef MINI
            case 1: show_threads(ctx); break;
            case 4: show_stats(ctx); break;
            case 6: ctx->printf(ctx, "%s", seq_stats()); break;
#endif
          }
        }
      }
      break;
    case '/l__': // skode-load num
      if (argc) { skode_load(ctx, voice, x); } break;
    case '/w__': // wave-load num wave channel
      {
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
        if (argc) wave_load(ctx, file_num, wave_slot, ch, 1);
      }
      break;
#ifndef MINI
    case '<___': // record duration
      if (arg) {
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
    case '*___': // write-recorded
      {
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
          ctx->printf(ctx, "# file %s (%ld frames)\n", name, rec_ptr);
          save_wav(ctx, name, recording, rec_ptr/VOICE_MAX/AUDIO_CHANNELS, voice_record, VOICE_MAX);
        }
      }
      break;
    case '>___': // copy-voice dest-voice
      if (arg) voice_copy(voice, x);
      break;
    case '/___': // default-wave voice
      wave_default(voice);
      break;
    case '%___': // pattern-modulus num
      if (arg) seq_modulo_set(ctx->pattern, x);
      break;
    case 'x!__': // step-mute step
      if (arg) seq_mute_set(ctx->pattern, x, 0);
      break;
    case 'x@__': // step-unmute step
      if (arg) seq_mute_set(ctx->pattern, x, 1);
      break;
#endif
    case '=___':  // variable-set slot value
      if (argc>1) ands_set_local(ctx->parse, x, arg[1]);
      else if (argc == 1) {
        double f = ands_get_local(ctx->parse, x);
        ctx->printf(ctx, "# $%d %g\n", x, f);
      }
      else {
        for (int i=0; i<10; i++) {
          double f = ands_get_local(ctx->parse, i);
          ctx->printf(ctx, "# $%d %g\n", i, f);
        }
      }
      break;
    case '/wex': // wave-expand wave
      if (argc && x >= 200 && x <=999) wave_table_dynamic_expand(x);
      break;
    default:
      if (ctx->trace) {
        ctx->printf(ctx, "# SKODE_UNKNOWN_FUNCTION %d [%x] :: %d", info, atom, argc);
        ctx->printf(ctx, " v%d", ctx->voice);
        ctx->puts(ctx, "");
      }
      break;
  }
  return 0;
}

int skode_defer(ands_t *s, int info) {
#ifndef MINI
  skode_t *ctx = (skode_t*)ands_user(s);
  char mode = ands_defer_mode(s);
  double t = ands_defer_num(s);
  if (ctx->defer_sample_time == 0) {
    ctx->defer_sample_time = SAMPLE_COUNT_GET();
  }
  uint64_t dst = ctx->defer_sample_time;
  if (mode == '+') t *= (tempo_time_per_step * 4.0f);
  t += ctx->defer_last;
  uint64_t qt = (uint64_t)(t * (float)MAIN_SAMPLE_RATE) + dst;
  if (ctx->trace) {
#ifdef _WIN32
    ctx->printf(ctx, "# SKODE_DEFER %c %g(%lld/%lld) '%s' (%g)\n",
#else
    ctx->printf(ctx, "# SKODE_DEFER %c %g(%ld/%ld) '%s' (%g)\n",
#endif
      mode,
      t, qt, dst,
      ands_defer_string(s),
      ctx->defer_last);
  }
  queue_item(qt, ands_defer_string(s), ctx->voice, -1);
  ctx->defer_last += ands_defer_num(s);
#endif
  return 0;
}

int skode_chunk_end(ands_t *s, int info) {
  skode_t *ctx = (skode_t*)ands_user(s);
  if (ctx->trace) ctx->printf(ctx, "# CHUNK_END %d\n", info);
  ctx->defer_last = 0;
  ctx->defer_sample_time = 0;
  return 0;
}

int skode_unknown(skode_t *ctx, ands_t *s, int info) {
  ctx->printf(ctx, "# SKODE_UNKNOWN %d\n", info);
  return 0;
}

int skode_callback(ands_t *s, int info) {
  skode_t *ctx = (skode_t*)ands_user(s);
  switch (info) {
    case FUNCTION: return skode_function(s, info);
    case DEFER: return skode_defer(s, info);
    case CHUNK_END: return skode_chunk_end(s, info);
    case PUSH: { voice_push(&ctx->stack, ctx->voice); ctx->printf(ctx, "pushed v%d\n", ctx->voice); } break;
    case POP: { ctx->voice = voice_pop(&ctx->stack); } break;
    case GOT_STRING: { if (ctx->trace) ctx->printf(ctx, "# -> {%s}\n", ands_string(s)); } break;
    case GOT_ARRAY: { if (ctx->trace) ctx->printf(ctx, "# -> (..%d..)\n", ands_data_len(s)); } break;
    default: return skode_unknown(ctx, s, info);
  }
  return 0;
}

double global_var[10];


int skode_consume(char *line, skode_t *ctx) {
  if (ctx->parse == NULL) {
    // TODO this should live in wire-init or similar
    ctx->parse = ands_new(skode_callback, (void *)ctx);
    ands_set_global(ctx->parse, global_var);
  }
  ctx->log_len = 0;
  ctx->log[0] = '\0';
  _skode_all[skode_hash(ctx)] = ctx;

  if (ctx->events) mpsc_queue_send(&mq, line);

  int r = 0;

  ands_consume(ctx->parse, line, skode_callback);
  return ctx->quit;
  return r;
}

int audio_show(skode_t *ctx) {
  skode_t wprime;
  if (ctx == NULL) {
    ctx = &wprime;
    skode_init(ctx);
  }
  ctx->printf(ctx, "# synth backend is running\n");
  ctx->printf(ctx, "# synth total voice count %d\n", VOICE_MAX);
  int active = 0;
  for (int i = 0; i < VOICE_MAX; i++) if (voice_amp[i] != 0) active++;
  ctx->printf(ctx, "# synth active voice count %d\n", active);
#ifdef _WIN32
  ctx->printf(ctx, "# synth sample count %lld\n", SAMPLE_COUNT_GET());
#else
  ctx->printf(ctx, "# synth sample count %ld\n", SAMPLE_COUNT_GET());
#endif
  return 0;
}

void skode_init(skode_t *ctx) {
  static int first = 1;
  if (first) {
    for (int i = 0; i < SKODE_POINTER_MAX; i++) {
      _skode_all[i] = NULL;
    }
    EXTRA_INIT();
    first = 0;
  }
  ctx->voice = 0;
  ctx->pattern = 0;
  ctx->step = -1;
  ctx->trace = 0;
  ctx->verbose = 0;
  ctx->events = 0;
  ctx->parse = NULL;
  ctx->quit = 0;
  ctx->puts = skode_puts;
  ctx->printf = skode_printf;
  ctx->log_enable = 0;
  ctx->log_max = SKODE_LOG_MAX;
  ctx->log_len = 0;
  ctx->log[0] = '\0';
}
