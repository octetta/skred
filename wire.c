#include "skred.h"
#include "wire.h"
#include "seq.h"
#include "miniwav.h"

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
  [FUNC_HOLD] = "hold",
  [FUNC_RESET] = "reset",
  [FUNC_FILTER_MODE] = "filter-mode",
  [FUNC_FILTER_FREQ] = "filter-freq",
  [FUNC_FILTER_RES] = "filter-q",
  [FUNC_COPY] = "copy-voice",
  [FUNC_WAVE_DEFAULT] = "wave-get-default",
  [FUNC_UNKNOWN] = "unknown",
  [FUNC_SMOOTHER] = "smoother",
  [FUNC_GLISSANDO] = "glissando",
  [FUNC_DATA] = "data",
  //
  [FUNC_HELP] = "help",
  [FUNC_SEQ] = "seq",
  [FUNC_MAIN_SEQ] = "main-seq",
  [FUNC_STEP] = "step",
  [FUNC_PATTERN] = "pattern",
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
  [FUNC_DATA_READ] = "data-read",
  [FUNC_CZ] = "cz",
  [FUNC_VOLUME_SET] = "volume",
};

char *func_func_str(int n) {
  if (n >= 0 && n <= FUNC_UNKNOWN) {
    if (display_func_func_str[n]) {
      return display_func_func_str[n];
    }
  }
  return "no-string";
}

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

value_t parse_none(int func, int sub_func, wire_t *w) {
  value_t v;
  v.func = func;
  v.sub_func = sub_func;
  v.argc = 0;
  v.next = 0;
  if (w->trace) dump(v);
  return v;
}

value_t parse(const char *ptr, int func, int sub_func, int argc, wire_t *w) {
  if (w) {
    w->last_func = func;
    w->last_sub_func = sub_func;
  }
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
  if (w->debug) {
    printf("# argc:%d next:%d", v.argc, v.next);
    puts("");
  }
  if (w->trace) dump(v);

  return v;
}

#include "skred.h"
#include "synth-types.h"
#include "synth.h"

void wire_show(wire_t *w) {
  if (w == NULL) return;
  printf("# voice %d\n", w->voice);
  printf("# state %d\n", w->state);
  printf("# pattern %d\n", w->pattern);
  printf("# scratch %s\n", w->scratch);
  printf("# data max %d\n", w->data_max);
  printf("# data len %d\n", w->data_len);
  printf("( ");
  for (int i = 0; i < w->data_len; i++) printf("%.8f ", w->data[i]);
  printf(")\n");
}

#include "udp.h"

void system_show(void) {
  printf("# udp_port %d\n", udp_info());
}

void show_stats(void) {
  // do something useful
  printf("# synth frames per callback %d : %gms\n",
    synth_frames_per_callback, (float)synth_frames_per_callback / (float)MAIN_SAMPLE_RATE * 1000.0f);
  printf("# seq frames per callback %d : %gms\n",
    seq_frames_per_callback, (float)seq_frames_per_callback / (float)MAIN_SAMPLE_RATE * 1000.0f);
  for (int i = 0; i < QUEUE_SIZE; i++) {
    if (work_queue[i].state != Q_FREE) {
      printf("# [%d] (%d) @%ld {%s}\n", i, work_queue[i].state, work_queue[i].when, work_queue[i].what);
    }
  }
}

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

int patch_load(int voice, int n, int output) {
  char file[1024];
  sprintf(file, "exp%d.patch", n);
  FILE *in = fopen(file, "r");
  int r = 0;
  if (in) {
    wire_t w = WIRE();
    char line[1024];
    while (fgets(line, sizeof(line), in) != NULL) {
      size_t len = strlen(line);
      if (len > 0 && line[len-1] == '\n') line[len-1] = '\0';
      if (output) printf("# %s\n", line);
      r = wire(line, &w);
      if (r != 0) {
        if (output) printf("# error in patch\n");
        break;
      }
    }
    fclose(in);
  }
  return r;
}

#define SAVE_WAVE_LEN (8)
static float *save_wave_list[SAVE_WAVE_LEN]; // to keep from crashing the synth, have a place to store free-ed waves
static int save_wave_ptr = 0;

int data_load(wire_t *w, int where) {
  if (where < EXT_SAMPLE_00 || where >= EXT_SAMPLE_99) return ERR_INVALID_EXT_SAMPLE;
  if (w == NULL) return 100; // fix todo
  if (w->data == NULL) return 100; // fix todo
  float *table = w->data;
  int len = w->data_len;
    // duped elsewhere todo consolidate
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
    wave_rate[where] = (float)44100.0f;
    wave_one_shot[where] = 1;
    wave_loop_enabled[where] = 0;
    wave_loop_start[where] = 1;
    wave_loop_end[where] = len;
    wave_midi_note[where] = 69;
    wave_offset_hz[where] = (float)len / 44100.0f * 440.0f;
    char *name = "data";
    int channels = 1;
    printf("# read %d frames from %s to %d (ch:%d sr:%d)\n", len, name, where, channels, 44100);
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


// this is a mess i need to clean up

#include "scope-shared.h"
extern int scope_enable;
extern scope_buffer_t *new_scope;


void pattern_show(int pattern_pointer) {
  int first = 1;
  for (int s = 0; s < SEQ_STEPS_MAX; s++) {
    char *line = seq_pattern[pattern_pointer][s];
    if (strlen(line) == 0) break;
    if (first) {
      int state = seq_state[pattern_pointer];
      printf("; M%g\n", tempo_bpm);
      printf("; y%d z%d %%%d # [%d]\n",
        pattern_pointer, state, seq_modulo[pattern_pointer], seq_pointer[pattern_pointer]);
      first = 0;
    }
    printf("; {%s} x%d", line, s);
    if (seq_pattern_mute[pattern_pointer][s]) printf(" .%d", pattern_pointer);
    puts("");
  }
}

void tempo_set(float m);

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

#if 0
void scope_wave_update(const float *table, int size) {
  new_scope->wave_len = 0;
  downsample_block_average_min_max(table, size, new_scope->wave_data, SCOPE_WAVE_WIDTH, new_scope->wave_min, new_scope->wave_max);
  new_scope->wave_len = SCOPE_WAVE_WIDTH;
}
#endif

int wavetable_show(int n) {
  if (n >= 0 && n < WAVE_TABLE_MAX && wave_table_data[n] && wave_size[n]) {
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
    printf(" +hz:%g midi:%g", wave_offset_hz[n], wave_midi_note[n]);
    puts("");
    downsample_block_average_min_max(table, size, new_scope->wave_data, SCOPE_WAVE_WIDTH, new_scope->wave_min, new_scope->wave_max);
    new_scope->wave_len = SCOPE_WAVE_WIDTH;
  }
  return 0;
}

static char *ignore = " \t\r\n;";

void wire_data_push(wire_t *w) {
  if (w->data_len >= w->data_max) {
    // too much data, ignoring...
    return;
  }
  if (w->data_acc[0] != '\0') {
    float f = strtof(w->data_acc, NULL);
    // printf("# [%d] %s ~> %g\n", w->data_len, w->data_acc, f);
    w->data_acc[0] = '\0';
    w->data_acc_ptr = 0;
    w->data[w->data_len] = f;
    w->data_len++;
  }
}

int wire(char *line, wire_t *w) {
  wire_t safe = WIRE();
  if (w == NULL) w = &safe;
  size_t len = strlen(line);
  if (len == 0) return 0;

  char *ptr = line;
  char *max = line + len;

  value_t v;
  int voice = w->voice;

  int more = 1;
  int r = 0;

  char c;

  uint64_t queue_now = 0;
  float queue_float_acc = 0.0f;
  w->queued_pointer = 0;
  w->queued[0] = '\0';

  while (more) {
    // guard against over-runs...
    if (*ptr == '\0' || ptr >= max) {
      // handle the case where we only had one thing on a line
      switch (w->state) {
        case W_SCRATCH:
          w->scratch[w->scratch_pointer++] = ' ';
          w->scratch[w->scratch_pointer+1] = '\0';
          break;
        case W_DATA:
          wire_data_push(w);
          break;
      }
      break;
    }
    if (w->state == W_SCRATCH) {
      // collect chars to scratch...
      char c = *ptr;
      switch (*ptr++) {
        case '}':
          // do something with accumulated characters...
          w->scratch[w->scratch_pointer] = '\0';
          w->scratch[w->scratch_pointer+1] = '\0';
          w->state = W_PROTOCOL;
          continue;
        default:
          if (w->scratch_pointer < WIRE_SCRATCH_MAX) {
            w->scratch[w->scratch_pointer++] = c;
          }
          continue;
      }
    } else if (w->state == W_DATA) {
      // collect floats to data...
      char c = *ptr;
      switch (*ptr++) {
        case '\0':
          puts("# got null in w_data...");
          break;
        case ')':
          puts("# W_DATA -> W_PROTOCOL");
          // stop collecting data
          wire_data_push(w);
          w->state = W_PROTOCOL;
          continue;
        default:
          switch (c) {
            case '-': case '.':
            case '0': case '1': case '2': case '3': case '4':
            case '5': case '6': case '7': case '8': case '9':
              w->data_acc[w->data_acc_ptr] = c;
              w->data_acc[w->data_acc_ptr+1] = '\0';
              w->data_acc_ptr++;
              break;
            default:
              wire_data_push(w);
              break;
          }
          continue;
      }
    } else {
      // wire protocol section
      // skip whitespace and semicolons
      ptr += strspn(ptr, ignore);
      if (w->debug) printf("# [%ld] '%c' (%d)\n", ptr-line, *ptr, *ptr);
      if (queue_now) {
        // we started queue-ing, so...
        char c = *ptr;
        if (c != '~') {
          // unless we see a '~', stuff everything into a buffer...
          // TODO can check if there's a '#' and bail early...
          w->queued[w->queued_pointer++] = c;
          w->queued[w->queued_pointer] = '\0'; // make sure there's a null-terminator...
          ptr++;
          continue;
        }
      }
      r = 0;
      int verbose = 0;
      char token = *ptr++;
      switch (token) {
        case '{':
          w->state = W_SCRATCH;
          w->scratch_pointer = 0;
          continue;
        case '(':
          puts("# W_PROTOCOL -> W_DATA");
          w->state = W_DATA;
          w->data_len = 0;
          w->data_acc[0] = '\0';
          w->data_acc_ptr = 0;
          continue;
        case '[':
          voice_push(&w->stack, (float)voice);
          continue;
        case ']':
          voice = (int)voice_pop(&w->stack);
          continue;
        case '#':
          return 0;
        case '\0':
          break;
        case ':':
          switch (*ptr++) {
            case '\0': return 100;
            case 'q': return -1;
            case 'i':
              if (w->output) w->output = 0; else w->output = 1;
              break;
            case 't':
              v = parse_none(FUNC_SYS, FUNC_TRACE, w);
              c = *ptr;
              if (c == '0' || c == '1') {
                w->trace = c - '0';
                ptr++;
              } else {
                if (w->trace) w->trace = 0; else w->trace = 1;
              }
              break;
            case 'S':
              v = parse_none(FUNC_SYS, FUNC_STATS0, w);
              if (w->output) show_stats();
              if (w->output) wire_show(w);
              break;
            case 's':
              v = parse_none(FUNC_SYS, FUNC_STATS1, w);
              if (w->output) {
                system_show();
                show_threads();
                audio_show();
              }
              break;
            case 'd':
              v = parse_none(FUNC_SYS, FUNC_DEBUG, w);
              c = *ptr;
              if (c == '0' || c == '1') {
                w->debug = c - '0';
                ptr++;
              } else {
                if (w->debug) w->debug = 0; else w->debug = 1;
              }
              break;
            case 'o':
              v = parse_none(FUNC_SYS, FUNC_SCOPE, w);
              scope_enable = 1;
              // sub x for scope_cross = 1
              // sub q for scope_quit = 0
              // sub 0..VOICE_MAX-1 for scope_channel = n
              // sub -1 for scope_channel = -1 (all channels)
              break;
            case 'l':
              // :l# load exp#.patch
              v = parse(ptr, FUNC_SYS, FUNC_LOAD, 1, w);
              {
                int which;
                if (v.argc == 1) {
                  ptr += v.next;
                  which = (int)v.args[0];
                } else return ERR_PARSING;
                r = patch_load(voice, which, w->output);
              }
              break;
            case 'D':
              // :D# load data into wave slot #
              v = parse(ptr, FUNC_SYS, FUNC_DATA_READ, 1, w);
              if (v.argc == 1) {
                ptr += v.next;
                int where = (int)v.args[0];
                data_load(w, where);
              } else return ERR_PARSING;
              break;
            case 'w':
              // :w#,# load wave#.wav into wave slot #
              v = parse(ptr, FUNC_SYS, FUNC_WAVE_READ, 2, w);
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
                } else return ERR_PARSING;
                r = wave_load(which, where);
              }
              break;
            default: return 999;
          }
          break;
        case '+': case '~':
          // delay by absolute time versus delay by fraction of BPS
          {
            if (token == '+') {
            }
            int func = FUNC_DELAY;
            v = parse(ptr, func, FUNC_NULL, 1, w);
            if (v.argc == 1) {
              ptr += v.next;
              float t = v.args[0];
              if (t == 0) {
                // queue any previous items
                if (w->queued_pointer) {
                  queue_item(queue_now, w->queued, voice);
                  w->queued_pointer = 0;
                }
                // switch back to "real-time"
                queue_float_acc = 0.0f; // not sure this is what I want... revisit
                queue_now = 0;
              } else if (t > 0) {
                if (token == '+') {
                  // use t as a fraction multiplied by BPS time
                  t *= tempo_time_per_step;
                }
                queue_float_acc += t;
                uint64_t queue_new = (uint64_t)(queue_float_acc * (float)MAIN_SAMPLE_RATE);
                queue_new += synth_sample_count;
                if (queue_new != queue_now) {
                  // queue any previous items
                  if (w->queued_pointer) {
                    queue_item(queue_now, w->queued, voice);
                    w->queued_pointer = 0;
                  }
                  // start new queue-ing
                  queue_now = queue_new;
                }
                // mark synth sample count (now) + argument converted from seconds -> samples
                // and start queueing...
              }
            }
          } break;
        case 'c':
          v = parse(ptr, FUNC_CZ, FUNC_NULL, 2, w);
          if (v.argc == 2) {
            ptr += v.next;
            r = cz_set(voice, (int)v.args[0], v.args[1]);
          }
          break;
        case 'C':
          v = parse(ptr, FUNC_CZ, FUNC_NULL, 2, w);
          if (v.argc == 2) {
            ptr += v.next;
            r = cmod_set(voice, (int)v.args[0], v.args[1]);
          }
          break;
        case '\\':
          verbose = 1;
        case '?':
          v = parse_none(FUNC_HELP, FUNC_NULL, w);
          if (*ptr == '?') {
            voice_show_all(voice);
            ptr++;
          } else {
            voice_show(voice, ' ', verbose);
          }
          break;
        case 'a':
          v = parse(ptr, FUNC_AMP, FUNC_NULL, 1, w);
          if (v.argc == 1) {
            ptr += v.next;
          } else return ERR_PARSING;
          r = amp_set(voice, v.args[0]);
          break;
        case 'p':
          v = parse(ptr, FUNC_PAN, FUNC_NULL, 1, w);
          if (v.argc == 1) {
            ptr += v.next;
          } else return ERR_PARSING;
          r = pan_set(voice, v.args[0]);
          break;
        case 'h':
          v = parse(ptr, FUNC_HOLD, FUNC_NULL, 1, w);
          if (v.argc == 1) {
            ptr += v.next;
            voice_sample_hold_max[voice] = (int)v.args[0];
            //voice_sample_hold_count[voice] = 0;
          }
          break;
        case 'q':
          v = parse(ptr, FUNC_QUANT, FUNC_NULL, 1, w);
          if (v.argc == 1) {
            ptr += v.next;
          } else return ERR_PARSING;
          r = wave_quant(voice, (int)v.args[0]);
          break;
        case 'f':
          v = parse(ptr, FUNC_FREQ, FUNC_NULL, 1, w);
          if (v.argc == 1) {
            ptr += v.next;
          } else return ERR_PARSING;
          r = freq_set(voice, v.args[0]);
          break;
        case 'v':
          v = parse(ptr, FUNC_VOICE, FUNC_NULL, 1, w);
          if (v.argc == 1) {
            ptr += v.next;
          } else return ERR_PARSING;
          r = voice_set((int)v.args[0], &voice);
          if (w->output) console_voice = voice;
          break;
        case 'V':
          v = parse(ptr, FUNC_VOLUME_SET, FUNC_NULL, 1, w);
          if (v.argc == 1) {
            ptr += v.next;
          } else return ERR_PARSING;
          r = volume_set((int)v.args[0]);
          break;
        case '>':
          v = parse(ptr, FUNC_COPY, FUNC_NULL, 1, w);
          if (v.argc == 1) {
            ptr += v.next;
          } else return ERR_PARSING;
          r = voice_copy(voice, (int)v.args[0]);
          break;
        case 'w':
          v = parse(ptr, FUNC_WAVE, FUNC_NULL, 1, w);
          if (v.argc == 1) {
            ptr += v.next;
          } else return ERR_PARSING;
          r = wave_set(voice, (int)v.args[0]);
          sprintf(new_scope->wave_text, "w%d", (int)v.args[0]);
          break;
        case 'T':
          v = parse_none(FUNC_TRIGGER, FUNC_NULL, w);
          voice_trigger(voice);
          break;
        case '/': // function-specific "set last thing to default" modifier
          switch (w->last_func) {
            case FUNC_WAVE:
              v = parse_none(FUNC_WAVE_DEFAULT, FUNC_NULL, w);
              wave_default(voice);
              break;
            default:
              break;
          }
          break;
        case 'B':
          v = parse_none(FUNC_LOOP, FUNC_NULL, w);
          c = *ptr;
          if (c == '0' || c == '1') {
            wave_loop(voice, c == '1');
            ptr++;
          } else wave_loop(voice, -1);
          break;
        case 'W':
          v = parse(ptr, FUNC_WAVE_SHOW, FUNC_NULL, 1, w);
          if (v.argc == 1) {
            ptr += v.next;
          } else return ERR_PARSING;
          r = wavetable_show((int)v.args[0]);
          sprintf(new_scope->wave_text, "w%d", (int)v.args[0]);
          break;
        case 'y':
          v = parse(ptr, FUNC_PATTERN, FUNC_NULL, 1, w);
          if (v.argc == 1) {
            ptr += v.next;
            int p = (int)v.args[0];
            scope_pattern_pointer = p;
            w->pattern = p;
          }
          break;
        case '%':
          v = parse(ptr, FUNC_STEP, FUNC_NULL, 1, w);
          if (v.argc == 1) {
            ptr += v.next;
            int m = (int)v.args[0];
            seq_modulo_set(w->pattern, m);
          }
          break;
        case '!':
          v = parse(ptr, FUNC_STEP_UNMUTE, FUNC_NULL, 1, w);
          if (v.argc == 1) {
            ptr += v.next;
            int step = (int)v.args[0];
            seq_mute_set(w->pattern, step, 0);
          }
          break;
        case '.':
          v = parse(ptr, FUNC_STEP_MUTE, FUNC_NULL, 1, w);
          if (v.argc == 1) {
            ptr += v.next;
            int step = (int)v.args[0];
            seq_mute_set(w->pattern, step, 1);
          }
          break;
        case 'x':
          v = parse(ptr, FUNC_STEP, FUNC_NULL, 1, w);
          if (v.argc == 1) {
            ptr += v.next;
            int p = (int)v.args[0];
            seq_step_set(w->pattern, p, w->scratch);
          }
          break;
        case 'Z':
          v = parse(ptr, FUNC_MAIN_SEQ, FUNC_NULL, 1, w);
          if (v.argc == 1) {
            ptr += v.next;
            seq_state_all((int)v.args[0]);
          } else {
            if (w->output) for (int p = 0; p < PATTERNS_MAX; p++) pattern_show(p);
          }
          break;
        case 'z':
          v = parse(ptr, FUNC_SEQ, FUNC_NULL, 1, w);
          if (v.argc == 1) {
            ptr += v.next;
            seq_state_set(w->pattern, (int)v.args[0]);
          } else {
            if (w->output) pattern_show(w->pattern);
          }
          break;
        case 'M':
          v = parse(ptr, FUNC_METRO, FUNC_NULL, 1, w);
          if (v.argc == 1) {
            ptr += v.next;
          } else return ERR_PARSING;
          tempo_set(v.args[0]);
          if (scope_enable) sprintf(new_scope->status_text, "M%g", tempo_bpm);
          break;
        case 'm':
          v = parse_none(FUNC_MUTE, FUNC_NULL, w);
          c = *ptr;
          if (c == '0' || c == '1') {
            wave_mute(voice, c == '1');
            ptr++;
          } else wave_mute(voice, -1);
          break;
        case 'b':
          v = parse_none(FUNC_DIR, FUNC_NULL, w);
          c = *ptr;
          if (c == '0' || c == '1') {
            wave_dir(voice, c == '1');
            ptr++;
          } else wave_dir(voice, -1);
          break;
        case 'n':
          v = parse(ptr, FUNC_MIDI, FUNC_NULL, 1, w);
          if (v.argc == 1) {
            ptr += v.next;
          } else return ERR_PARSING;
          r = freq_midi(voice, v.args[0]);
          break;
        case 'A':
          v = parse(ptr, FUNC_AMP_MOD, FUNC_NULL, 2, w);
          if (v.argc == 2) {
            ptr += v.next;
            r = amp_mod_set(voice, (int)v.args[0], v.args[1]);
          } else if (v.argc == 1) {
            ptr += v.next;
            r = amp_mod_set(voice, -1, 0);
          } else return ERR_PARSING;
          break;
        case 'J':
          v = parse(ptr, FUNC_FILTER_MODE, FUNC_NULL, 2, w);
          if (v.argc == 1) {
            ptr += v.next;
            voice_filter_mode[voice] = (int)v.args[0];
            mmf_set_params(voice,
              voice_filter_freq[voice],
              voice_filter_res[voice]);
          } else return ERR_PARSING;
          break;
        case 'K':
          v = parse(ptr, FUNC_FILTER_FREQ, FUNC_NULL, 2, w);
          if (v.argc == 1) {
            ptr += v.next;
            if (v.args[0] > 0) {
              mmf_set_freq(voice, v.args[0]);
              r = 0;
            }
          } else return ERR_PARSING;
          break;
        case 'Q':
          v = parse(ptr, FUNC_FILTER_RES, FUNC_NULL, 2, w);
          if (v.argc == 1) {
            ptr += v.next;
            if (v.args[0] > 0) {
              mmf_set_res(voice, v.args[0]);
              r = 0;
            }
          } else return ERR_PARSING;
          break;
        case 'l':
          v = parse(ptr, FUNC_VELOCITY, FUNC_NULL, 1, w);
          if (v.argc == 1) {
            ptr += v.next;
          } else return ERR_PARSING;
          r = envelope_velocity(voice, v.args[0]);
          break;
        case 'E':
          v = parse(ptr, FUNC_ENVELOPE, FUNC_NULL, 4, w);
          if (v.argc == 4) {
            ptr += v.next;
          } else return ERR_PARSING;
          r = envelope_set(voice, v.args[0], v.args[1], v.args[2], v.args[3]);
          break;
        case 's':
          v = parse(ptr, FUNC_SMOOTHER, FUNC_NULL, 1, w);
          if (v.argc == 1) {
            ptr += v.next;
            if (v.args[0] <= 0.0f) {
              voice_smoother_enable[voice] = 0;
            } else {
              voice_smoother_enable[voice] = 1;
              voice_smoother_smoothing[voice] = v.args[0];
            }
          } else return ERR_PARSING;
          break;
        case 'D':
          v = parse(ptr, FUNC_DATA, FUNC_NULL, 1, w);
          if (v.argc == 1) {
            ptr += v.next;
            int n = (int)v.args[0];
            printf("# D%d allocate %d\n", n, n);
            if (w->data_max != n) {
              if (w->data) {
                printf("# free data and alloc %d\n", n);
                free(w->data);
              }
              w->data_max = n;
              w->data = (float *)malloc(n * sizeof(float));
              w->data_acc[0] = '\0';
              w->data_acc_ptr = 0;
              w->data_len = 0;
            }
          } else return ERR_PARSING;
          break;
        case 'g':
          v = parse(ptr, FUNC_GLISSANDO, FUNC_NULL, 1, w);
          if (v.argc == 1) {
            ptr += v.next;
            if (v.args[0] <= 0.0f) {
              voice_glissando_enable[voice] = 0;
            } else {
              voice_glissando_enable[voice] = 1;
              voice_glissando_speed[voice] = v.args[0];
            }
          } else return ERR_PARSING;
          break;
        case 'S':
          v = parse(ptr, FUNC_RESET, FUNC_NULL, 1, w);
          if (v.argc == 1) {
            ptr += v.next;
          } else return ERR_PARSING;
          r = wave_reset(voice, (int)v.args[0]);
          break;
        case 'F':
          v = parse(ptr, FUNC_FREQ_MOD, FUNC_NULL, 2, w);
          if (v.argc == 2) {
            ptr += v.next;
            r = freq_mod_set(voice, (int)v.args[0], v.args[1]);
          } else if (v.argc == 1) {
            ptr += v.next;
            r = freq_mod_set(voice, -1, 0);
          } else return ERR_PARSING;
          break;
        case 'P':
          v = parse(ptr, FUNC_PAN_MOD, FUNC_NULL, 2, w);
          if (v.argc == 2) {
            ptr += v.next;
            r = pan_mod_set(voice, (int)v.args[0], v.args[1]);
          } else if (v.argc == 1) {
            ptr += v.next;
            r = pan_mod_set(voice, -1, 0);
          } else return ERR_PARSING;
          break;
        //
        default:
          if (w->output) printf("# not sure\n");
          return ERR_UNKNOWN_FUNC;
          more = 0;
          break;
      }
    }
    if (r != 0) break;
    if (ptr >= max) break;
    if (*ptr == '\0') break;
  }
  // queue left-over items
  if (w->queued_pointer) {
    queue_item(queue_now, w->queued, voice);
    w->queued_pointer = 0;
  }
  w->voice = voice;
  return r;
}

int audio_show(void) {
  printf("# synth backend is running\n");
  printf("# synth total voice count %d\n", VOICE_MAX);
  int active = 0;
  for (int i = 0; i < VOICE_MAX; i++) if (voice_amp[i] != 0) active++;
  printf("# synth active voice count %d\n", active);
  printf("# synth sample count %ld\n", synth_sample_count);
  return 0;
}

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
  [ERR_PARSING] = "parsing error",
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

char *wire_err_str(int n) {
  if (n >= 0 && n <= ERR_UNKNOWN) {
    if (all_err_str[n]) {
      return all_err_str[n];
    }
  }
  return "no-string";
}

void wire_init(wire_t *w) {
  w->voice = 0;
  w->state = W_PROTOCOL;
  w->last_func = FUNC_NULL;
  w->pattern = 0;
  w->data = NULL;
  w->data_max = 0;
  w->output = 0;
  w->trace = 0;
  w->debug = 0;
}
