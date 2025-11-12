#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>

#include "skode.h"

typedef float num_t;

static int vidx(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'z') return 10 + c - 'a';
  if (c >= 'A' && c <= 'Z') return 36 + c - 'A';
  return -1;
}

int is_cmd(skode_t *p) {
  return (p->cmd[0] && p->cmd[0] != '=');
}

int is_brace(skode_t *p) {
  return (p->blen > 0);
}

int is_paren(skode_t *p) {
  return (p->nparen > 0);
}

static void emit(skode_t *p) {
  if (p->fn) p->fn(p);
}

static void pushnum(skode_t *p, int toparen) {
  if (p->nlen > 0) {
    p->nbuf[p->nlen] = 0;
    if (toparen && p->nparen < p->parenc) p->paren[p->nparen++] = atof(p->nbuf);
    else if (!toparen && p->narg < MAX_ARGS) {
      if (strlen(p->nbuf) == 1 && (p->nbuf[0] == '.' || p->nbuf[0] == '-')) {
        p->arg[p->narg++] = NAN;
      } else {
        p->arg[p->narg++] = atof(p->nbuf);
      }
    } else {
      // ignore values beyond paren size...
    }
    p->nlen = 0;
  } else {
    //puts("!!!");
  }
}

static void finish(skode_t *p) {
  pushnum(p, 0);
  if (p->cmd[0] == '=' && p->narg == 1) {
    int i = vidx(p->vname);
    if (i >= 0) {
      p->vars[i] = p->arg[0];
      p->vdef[i] = 1;
    }
  }
  emit(p);
  memset(p->cmd, 0, MAX_CMD);
  p->narg = p->blen = p->nparen = 0;
  p->state = START;
}

static void startcmd(skode_t *p, char c) {
  finish(p);
  p->cmd[0] = c;
  p->cmd[1] = 0;
  p->state = ((c == '/') || (c == ':')) ? CMD : ARG;
}

void sparse_paren_resize(skode_t *p, int len) {
  if (p->paren) free(p->paren);
  p->paren = malloc(len * sizeof(num_t));
  p->parenc = len;
  p->nparen = 0;
}

void sparse_init(skode_t *p, void (*fn)(skode_t *p)) {
  memset(p, 0, sizeof(skode_t));
  p->fn = fn;
  p->voice = 0;
  p->pattern = 0;
  sparse_paren_resize(p, MAX_PAREN);
}

void sparse_free(skode_t *p) {
  free(p->paren);
}

void sparse(skode_t *p, const char *s, int len) {
  for (int i = 0; i < len; i++) {
    char c = s[i];
    
    switch (p->state) {
      case COMMENT:
        if (c == '\n') p->state = START;
        break;
        
      case BRACE:
        if (c == '}') finish(p);
        else if (p->blen < MAX_BRACE - 1) p->brace[p->blen++] = c;
        break;
        
      case PAREN:
        if (isdigit(c) || strchr(".-eE", c)) {
          if (p->nlen < MAX_NUMBUF - 1) p->nbuf[p->nlen++] = c;
        } else {
          pushnum(p, 1);
          if (c == ')') finish(p);
        }
        break;
        
      case CMD:
        if (isalpha(c)) {
          p->cmd[1] = c;
          p->cmd[2] = 0;
          p->state = ARG;
        } else if (c == ';' || c == '\n') {
          finish(p);
        } else if (!isspace(c) && c != ',') {
          p->state = START;
        }
        break;
        
      case VAR:
        if (isalnum(c)) {
          if (p->cmd[0] == '=') {
            p->vname = c;
          } else {
            int idx = vidx(c);
            if (idx >= 0 && p->vdef[idx] && p->narg < MAX_ARGS)
              p->arg[p->narg++] = p->vars[idx];
          }
          p->state = ARG;
        } else {
          p->state = START;
          i--;
        }
        break;
        
      case ARG:
        if (isalpha(c)) {
          // Check if this could be exponential notation
          if (p->nlen > 0 && strchr("eE", c) && i > 0 && isdigit(s[i-1])) {
            // Likely exponent, treat as number char
            if (p->nlen < MAX_NUMBUF - 1) p->nbuf[p->nlen++] = c;
          } else {
            // New command
            pushnum(p, 0);
            finish(p);
            i--;
          }
        } else if (isdigit(c) || strchr(".-", c)) {
          if (p->nlen < MAX_NUMBUF - 1) p->nbuf[p->nlen++] = c;
        } else if (c == ' ' || c == '\t' || c == ',') {
          pushnum(p, 0);
        } else if (c == '$') {
          pushnum(p, 0);
          p->state = VAR;
        } else if (c == ';') {
          finish(p);
        } else if (c == '\n') {
          if (p->narg > 0 || p->nlen > 0 || p->cmd[0]) finish(p);
        } else if (c == '#') {
          finish(p);
          p->state = COMMENT;
        } else if (c == '{') {
          finish(p);
          p->state = BRACE;
        } else if (c == '(') {
          finish(p);
          p->state = PAREN;
        } else if (strchr("\\@?>![]:/+=~", c)) {
          pushnum(p, 0);
          finish(p);
          i--;
        }
        break;
        
      case START:
        if (c == '#') p->state = COMMENT;
        else if (c == ';') finish(p);
        else if (c == '{') { finish(p); p->state = BRACE; }
        else if (c == '(') { finish(p); p->state = PAREN; }
        else if (c == '$') p->state = VAR;
        else if (c == '=') {
          finish(p);
          p->cmd[0] = '=';
          p->cmd[1] = 0;
          p->state = VAR;
        }
        else if (isalpha(c) || strchr("\\@?>![]:/+~", c)) startcmd(p, c);
        else if (c == '\n') {
          if (p->narg > 0 || p->nlen > 0 || p->cmd[0]) finish(p);
        }
        break;
    }
  }
}

void sparse_complete(skode_t *p) {
  if (p->cmd[0] || p->narg > 0) finish(p);
}

#ifdef MAIN

float tempo_time_per_step = 10.0f;
float tempo_bpm = 0.0f;

//
int debug = 0;
int trace = 0;
//
#include "miniaudio.h"
#include "skred.h"
#include "synth-types.h"
#include "synth.h"
#include "util.h"

#undef USE_SCOPE

//
void synth_callback(ma_device* pDevice, void* output, const void* input, ma_uint32 frame_count) {
  static int first = 1;
#ifdef USE_SCOPE
  static int num_channels = 1;
#endif
  if (first) {
    util_set_thread_name("synth");
#ifdef USE_SCOPE
    if (scope_enable) scope->buffer_pointer = 0;
    num_channels = (int)pDevice->playback.channels;
#endif
    first = 0;
  }
  synth((float *)output, (float *)input, (int)frame_count, (int)pDevice->playback.channels);
  // copy frame buffer to shared memory?
#ifdef USE_SCOPE
  if (scope_enable) {
    float *f = (float *)output;
    for (int i = 0; i < frame_count * num_channels; i+=2) {
      scope->buffer_left[scope->buffer_pointer] = f[i];
      scope->buffer_right[scope->buffer_pointer] = f[i+1];
      scope->buffer_pointer++;
      if (scope->buffer_pointer >= SCOPE_WIDTH_IN_SAMPLES) scope->buffer_pointer = 0;
      //scope->buffer_pointer %= scope->buffer_len;
    }
  }
#endif
}

#ifdef USE_SEQ
#include "seq.h"

void seq_callback(ma_device* pDevice, void* output, const void* input, ma_uint32 frame_count) {
  static int first = 1;
  static int last_frame_count = 0;
  if (first) {
    util_set_thread_name("seq");
    seq_frames_per_callback = (int)frame_count;
    first = 0;
  }
  seq((int)frame_count);
  if ((int)frame_count != last_frame_count) {
    printf("# frame count %d -> %d\n", last_frame_count, (int)frame_count);
    last_frame_count = (int)frame_count;
  }
}
#endif

//

//
void handle(skode_t *p);
int patch_load(int voide, int n, int output) {
  char file[1024];
  sprintf(file, "exp%d.patch", n);
  FILE *in = fopen(file, "r");
  int r = 0;
  if (in) {
    //wire_t w = WIRE();
    skode_t p;
    sparse_init(&p, handle);
    char line[1024];
    while (fgets(line, sizeof(line), in) != NULL) {
      size_t len = strlen(line);
      if (len > 0 && line[len-1] == '\n') line[len-1] = '\0';
      if (output) printf("# %s\n", line);
      //r = wire(line, &w);
      sparse(&p, line, len);
      if (r != 0) {
        if (output) printf("# error in patch\n");
        break;
      }
    }
    fclose(in);
  }
  return r;
}
#include "miniwav.h"
#include "wire.h"
#define SAVE_WAVE_LEN (8)
static float *save_wave_list[SAVE_WAVE_LEN]; // to keep from crashing the synth, have a place to store free-ed waves
static int save_wave_ptr = 0;
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
//

void handle_cmd(skode_t *p) {
  if (p->narg == 0) {
    switch (p->major) {
      case '?': voice_show_all(p->voice); break;
      case 'T': voice_trigger(p->voice); break;
    }
  } else switch (p->major) {
    case 'a': amp_set(p->voice, p->arg[0]); break;
    case 'A': amp_mod_set(p->voice, p->arg[0], p->arg[1]); break;
    case 'b': wave_dir(p->voice, p->arg[0]); break;
    case 'B': wave_loop(p->voice, p->arg[0]); break;
    case 'c': cz_set(p->voice, p->arg[0], p->arg[1]); break;
    case 'C': cmod_set(p->voice, p->arg[0], p->arg[1]); break;
    case 'D': {
      int n = (int)p->arg[0];
      if (n != p->parenc) sparse_paren_resize(p, n);
    } break;
    case 'E': envelope_set(p->voice, p->arg[0], p->arg[1], p->arg[2], p->arg[3]); break;
    case 'f': freq_set(p->voice, p->arg[0]); break;
    case 'F': freq_mod_set(p->voice, p->arg[0], p->arg[1]); break;
    case 'h': {
      voice_sample_hold_max[p->voice] = p->arg[0];
    } break;
    case 'J': {
      voice_filter_mode[p->voice] = p->arg[0];
      mmf_set_params(p->voice,
        voice_filter_freq[p->voice],
        voice_filter_res[p->voice]);
    } break;
    case 'K': mmf_set_freq(p->voice, p->arg[0]); break;
    case 'l': envelope_velocity(p->voice, p->arg[0]); break;
    case 'm': wave_mute(p->voice, p->arg[0]); break;
#ifdef USE_SEQ
    case 'M': tempo_set(p->arg[0]); break;
#endif
    case 'n': freq_midi(p->voice, p->arg[0]); break;
    case 'p': pan_set(p->voice, p->arg[0]); break;
    case 'P': pan_mod_set(p->voice, p->arg[0], p->arg[1]); break;
    case 'q': wave_quant(p->voice, p->arg[0]); break;
    case 'Q': mmf_set_res(p->voice, p->arg[0]); break;
    case 's': {
      voice_smoother_enable[p->voice] = p->arg[0];
      voice_smoother_smoothing[p->voice] = p->arg[1];
    } break;
    case 'S': wave_reset(p->voice, p->arg[0]); break;
    case 'v': voice_set(p->arg[0], &p->voice); break;
    case 'V': volume_set(p->arg[0]); break;
    case 'w': wave_set(p->voice, p->arg[0]); break;
#ifdef USE_SCOPE
    case 'W': wavetable_show(p->voice, p->arg[0]); break;
#endif
#ifdef USE_SEQ
    case 'x': seq_step_set(p->pattern, p->arg[0], p->brace); break;
    case 'y': {
      p->pattern = (int)p->arg[0];
    } break;
    case 'z': {
      seq_state_set(p->pattern, p->arg[0]);
      // need to handle showing... above
    } break;
    case 'Z': {
      seq_state_all(p->arg[0]);
      // need to handle showing... above
    } break;
    case '%': break;
    case '!': break;
    case '@': break;
    case '+': break;
    case '~': break;
#endif
    case '?': voice_show(p->arg[0], ' ', 0); break;
    case '>': voice_copy(p->voice, p->arg[0]); break;
    case '\\': break;
    case '[': break;
    case ']': break;
    case '/': switch (p->minor) {
      case 'd': p->debug = p->arg[0]; break;
      case 'D': break;
      case 'l': patch_load(p->voice, p->arg[0], 1); break;
      case 'q': break;
      case 's': break;
      case 'S': break;
      case 't': p->trace = p->arg[0]; break;
      case 'w': {
        int which = p->arg[0];
        int where = 200;
        if (p->narg == 2) where = p->arg[1];
        wave_load(which, where); 
      }break;
    } break;
    default: break;
  }
}

void handle(skode_t *p) {
  if (is_cmd(p)) {
    p->last_major = p->major;
    p->last_minor = p->minor;
    p->major = p->cmd[0];
    if (p->major == ':') p->major = '/';
    p->minor = p->cmd[1];
    if (p->debug) {
      if (p->major == ':' || p->major == '/') {
        printf("(%c %c)", p->major, p->minor);
      } else {
        printf("(%c)", p->major);
      }
      if (p->narg) printf(" [");
      for (int i = 0; i < p->narg; i++) printf(" %g", p->arg[i]);
      if (p->narg) printf(" ]\n");
    }
    handle_cmd(p);
  }
  if (p->debug) {
    if (is_brace(p)) printf("{%.*s}\n", p->blen, p->brace);
    if (is_paren(p)) {
      printf("( ");
      for (int i = 0; i < p->nparen; i++) printf("%g ", p->paren[i]);
      printf(")\n");
    }
    if (p->narg == 0) printf("\n");
  }
}

int main(int argc, char **argv) {
  skode_t p;
  sparse_init(&p, handle);
  //
  synth_init();
  wave_table_init();
  voice_init();
  //seq_init();

  // miniaudio's synth device setup
  ma_device_config synth_config = ma_device_config_init(ma_device_type_playback);
  synth_config.playback.format = ma_format_f32;
  synth_config.playback.channels = AUDIO_CHANNELS;
  synth_config.sampleRate = MAIN_SAMPLE_RATE;
  synth_config.dataCallback = synth_callback;
  synth_config.periodSizeInFrames = requested_synth_frames_per_callback;
  synth_config.periodSizeInMilliseconds = 0;
  synth_config.periods = 3;
  synth_config.noClip = MA_TRUE;
  ma_device synth_device;
  ma_device_init(NULL, &synth_config, &synth_device);
  ma_device_start(&synth_device);

  // seq setup
  // miniaudio's seq device setup
  ma_device_config seq_config = ma_device_config_init(ma_device_type_playback);
  seq_config.playback.format = ma_format_f32;
  seq_config.playback.channels = AUDIO_CHANNELS;
  seq_config.sampleRate = MAIN_SAMPLE_RATE;
  seq_config.dataCallback = seq_callback;
  seq_config.periodSizeInFrames = requested_seq_frames_per_callback;
  seq_config.periodSizeInMilliseconds = 0;
  seq_config.periods = 2; // examples say "3"... trying something different
  seq_config.noClip = MA_TRUE;
  ma_device seq_device;
  ma_device_init(NULL, &seq_config, &seq_device);
  ma_device_start(&seq_device);

  //

  //
  const char *tests[] = {
    "f100\n", "f 100\n", "/f 10 20\n", "g 1 2 3\n", "{text}\n", 
    "(1 2 3)\n", "=a 10\n", "g $a\n", "m\n", "200 150\n", "ae1,2,3\n",
    "g1e-5\n", "f1.5e3x100\n"
  };
  
  if (argc > 1) {
    char *name = argv[1];
    FILE *f = stdin;
    if (strcmp(name, "-") != 0) f = fopen(name, "r");
    if (f) {
      char line[1024];
      while (fgets(line, 1024, f)) sparse(&p, line, strlen(line));
      fclose(f);
    }
  } else {
    for (int i = 0; i < sizeof(tests)/sizeof(tests[0]); i++)
      sparse(&p, tests[i], strlen(tests[i]));
  }
  
  sparse_complete(&p);
  sparse_free(&p);

  return 0;
}

#endif
