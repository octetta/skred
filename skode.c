#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "skode.h"

static int vidx(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'z') return 10 + c - 'a';
  if (c >= 'A' && c <= 'Z') return 36 + c - 'A';
  return -1;
}

int is_cmd(skode_t *p) {
  return (p->cmd[0] && p->cmd[0] != '=');
}

int is_bracket(skode_t *p) {
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
    if (toparen && p->nparen < MAX_PAREN) p->paren[p->nparen++] = atof(p->nbuf);
    else if (!toparen && p->narg < MAX_ARGS) p->arg[p->narg++] = atof(p->nbuf);
    p->nlen = 0;
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

void sparse_init(skode_t *p, void (*fn)(skode_t *p)) {
  memset(p, 0, sizeof(skode_t));
  p->fn = fn;
  p->paren = malloc(MAX_PAREN * sizeof(float));
  p->parenc = MAX_PAREN;
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
        } else if (strchr("\\?>![]:/+=~", c)) {
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
        else if (isalpha(c) || strchr("\\?>![]:/+~", c)) startcmd(p, c);
        else if (c == '\n') {
          if (p->narg > 0 || p->nlen > 0 || p->cmd[0]) finish(p);
        }
        break;
    }
  }
}

void sparse_end(skode_t *p) {
  if (p->cmd[0] || p->narg > 0) finish(p);
}

#ifdef MAIN

//
int debug = 0;
int trace = 0;
//
#include "miniaudio.h"
#include "skred.h"
#include "synth-types.h"
#include "synth.h"
#include "util.h"

//
void synth_callback(ma_device* pDevice, void* output, const void* input, ma_uint32 frame_count) {
  static int first = 1;
  static int num_channels = 1;
  if (first) {
    util_set_thread_name("synth");
    //if (scope_enable) scope->buffer_pointer = 0;
    num_channels = (int)pDevice->playback.channels;
    first = 0;
  }
  synth((float *)output, (float *)input, (int)frame_count, (int)pDevice->playback.channels);
  // copy frame buffer to shared memory?
#if 0
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
//

void myemit(skode_t *p) {
  if (is_cmd(p)) {
    char major = p->cmd[0];
    char minor = p->cmd[1];
    if (p->cmd[0] == ':' || p->cmd[0] == '/') {
      printf("(%c %c)", p->cmd[0], p->cmd[1]);
    } else {
      printf("(%c)", p->cmd[0]);
    }
    //
#if 1
    static int voice = 0;
    if (major == 'f') {
      if (p->narg) freq_set(voice, p->arg[0]);
    } else if (major == 'a') {
      if (p->narg) amp_set(voice, p->arg[0]);
    } else if (major == 'w') {
      if (p->narg) wave_set(voice, p->arg[0]);
    } else if (major == 'v') {
      if (p->narg) voice_set(p->arg[0], &voice);
    }
#endif
    //
    if (p->narg) printf(" [");
    for (int i = 0; i < p->narg; i++) printf(" %g", p->arg[i]);
    printf(" ]\n");
  }
  if (is_bracket(p)) printf("{%.*s}\n", p->blen, p->brace);
  if (is_paren(p)) {
    printf("( ");
    for (int i = 0; i < p->nparen; i++) printf("%g ", p->paren[i]);
    printf(")\n");
  }
}

int main(int argc, char **argv) {
  skode_t p;
  sparse_init(&p, myemit);
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
  
  sparse_end(&p);
  sparse_free(&p);

  return 0;
}

#endif
