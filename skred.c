/*

 voice = 
 
   wave table index
 
   freq -> phase_inc
   
   freq modulation source, freq modulation depth -> freq

   phase

   phase distortion type, phase distortion amount -> phase
   
   phase distortion modulation source, phase distortion depth -> phase distortion amount
*/

//#include <errno.h>
//#include <pthread.h>
//#include <stdint.h>
#include <stdio.h>
//#include <stdlib.h>
//#include <string.h>
//#include <sys/time.h>
//#include <time.h>
//#include <unistd.h>

#ifdef _WIN32
#include "winline.h"
#else
#include "linenoise.h"
#endif

#include "skred.h"

#include "skred-mem.h"
#include "scope-shared.h"

#include "util.h"

int scope_enable = 0;
scope_buffer_t scope_safety;
scope_buffer_t *scope = &scope_safety;

#include "miniaudio.h"

#include "synth-types.h"
#include "synth.h"

float tempo_time_per_step = 60.0f;
float tempo_bpm = 120.0f / 4.0f;
float tempo_base = 0.0f;

void tempo_set(float);

int debug = 0;
int trace = 0;

int console_voice = 0;

#include "udp.h"

int main_running = 1;

#include "wire.h"


#include "seq.h"

#if 0
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

int rec_state = 0;
long rec_ptr = 0;
float rec_sec = (float)REC_IN_SEC;
long rec_max = REC_IN_SEC * MAIN_SAMPLE_RATE * AUDIO_CHANNELS * VOICE_MAX;
float one_skred_frame[ONE_FRAME_MAX * AUDIO_CHANNELS * VOICE_MAX];
//float recording[REC_IN_SEC * MAIN_SAMPLE_RATE * AUDIO_CHANNELS * VOICE_MAX];
float *recording = NULL;

void synth_callback_init(float max_sec) {
  if (recording) free(recording);
  recording = NULL;
  rec_sec = max_sec;
  float max_samples = max_sec * (float)(MAIN_SAMPLE_RATE * AUDIO_CHANNELS * VOICE_MAX);
  rec_max = max_samples;
  recording = (float *)malloc(rec_max * sizeof(float));
}

void synth_callback_free(void) {
  if (recording) free(recording);
  recording = NULL;
  rec_max = 0;
}

void synth_callback(ma_device* pDevice, void* output, const void* input, ma_uint32 frame_count) {
  static int first = 1;
  static int num_channels = 1;
  if (first) {
    util_set_thread_name("synth");
    if (scope_enable) scope->buffer_pointer = 0;
    num_channels = (int)pDevice->playback.channels;
    first = 0;
  }
  synth((float *)output, (float *)input, (int)frame_count, (int)pDevice->playback.channels, pDevice->pUserData);
  sprintf(scope->debug_text, "%d %d %ld", frame_count, rec_state, rec_ptr);
  // copy frame buffer to shared memory?
  seq((int)frame_count);
  if (rec_state) {
    float *f = one_skred_frame;
    for (int i = 0; i < frame_count * num_channels * VOICE_MAX; i+=2) {
      if (rec_ptr < rec_max) {
        recording[rec_ptr++] = f[i];   // left
        recording[rec_ptr++] = f[i+1]; // right
      } else {
        rec_state = 0;
        break;
      }
    }
  }
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
}

void sleep_float(double seconds) {
  if (seconds < 0.0f) return; // Invalid input
  struct timespec ts;
  ts.tv_sec = (time_t)seconds; // Whole seconds
  ts.tv_nsec = (long)((seconds - (double)ts.tv_sec) * 1e9); // Fractional part to nanoseconds
  nanosleep(&ts, NULL);
}


int current_voice = 0;


#if 0
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
#endif

char my_data[] = "hello";

int main(int argc, char *argv[]) {
  int load_patch_number = -1;
  int udp_port = UDP_PORT;
  char execute_from_start[1024] = "";
  int use_edit = 1;
  if (argc > 1) {
    for (int i=1; i<argc; i++) {
      if (argv[i][0] == '-') {
        switch (argv[i][1]) {
          case 'n': use_edit = 0; break;
          case 'd': debug = 1; break;
          case 't': trace = 1; break;
          case 'p': udp_port = (int)strtol(&(argv[i][2]), NULL, 0); break;
          case 'l': load_patch_number = (int)strtol(&argv[i][2], NULL, 0); break;
          case '1': requested_synth_frames_per_callback = (int)strtol(&argv[i][2], NULL, 0); break;
          case '2': requested_seq_frames_per_callback = (int)strtol(&argv[i][2], NULL, 0); break;
          case 'e': {
            printf("# %s\n", argv[i]);
            strcpy(execute_from_start, &argv[i][2]);
          } break;
          default:
            printf("# unknown switch '%s'\n", argv[i]);
            exit(1);
            break;
        }
      }
    }
  }
  
  show_threads();
  

#ifdef _WIN32
  if (winlineInit() < 0) {
    use_edit = 0;
  } else {
    winlineHistoryLoad(HISTORY_FILE);
    winlineHistorySetMaxLen(100);
  }
#else
  linenoiseHistoryLoad(HISTORY_FILE);
#endif
  

  perf_start();

  synth_callback_init(REC_IN_SEC);
  synth_init();
  wave_table_init();
  voice_init();
  seq_init();

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
  synth_config.pUserData = &one_skred_frame;
  ma_device synth_device;
  ma_device_init(NULL, &synth_config, &synth_device);
  ma_device_start(&synth_device);

#if 0
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
#endif

  if (audio_show() != 0) return 1;

  util_set_thread_name("repl");

  if (udp_port != 0) {
    int r = udp_start(udp_port);
    if (r != udp_port) udp_port = 0;
  }

  system_show();

  if (load_patch_number >= 0) sk_load(0, load_patch_number, 0);

  if (scope_enable) {
    scope->buffer_len = SCOPE_WIDTH_IN_SAMPLES;
    sprintf(scope->status_text, "n/a");
  }

  //float bogus[44100 * 2];
  skred_mem_t scope_shared;
#define SKRED_SCOPE_NAME "skred-o-scope.001"
  if (skred_mem_create(&scope_shared, SKRED_SCOPE_NAME, sizeof(scope_buffer_t)) != 0) {
    printf("# did not create scope shared memory %s\n", SKRED_SCOPE_NAME);
    scope_enable = 0;
  } else {
    printf("# scope buffer ready\n");
    scope = (scope_buffer_t *)scope_shared.addr;
    scope_enable = 1;
    sprintf(scope->status_text, "n/a");
  }

  wire_t w = WIRE();
  w.output = 1;
  w.debug = debug;
  w.trace = trace;

  if (execute_from_start[0] != '\0') {
    int n = wire(execute_from_start, &w);
    if (n < 0) main_running = 0;
  }

  if (scope_enable) scope->voice_text[0] = '\0';

  while (main_running) {
    if (scope_enable) {
      voice_format(current_voice, scope->voice_text, 0);
    }

    char *line = NULL;

    if (use_edit) {
#ifdef _WIN32
      line = winlineReadLine("# ");
#else
      line = linenoise("# ");
#endif
    } else {
      char buffer[1024];
      printf("# ");
      line = fgets(buffer, sizeof(buffer), stdin);
    }
    if (line == NULL) {
      main_running = 0;
      break;
    }
    if (strlen(line) == 0) continue;
    if (use_edit) {
#ifdef _WIN32
    winlineHistoryAdd(line);
#else
    linenoiseHistoryAdd(line);
#endif
    }
    int n = wire(line, &w);
    if (use_edit) {
#ifdef _WIN32
      free(line);
#else
      linenoiseFree(line);
#endif
    }
    if (n < 0) break; // request to stop or error
    if (n > 0) printf("# ERR:%d\n", n);
  }
  if (use_edit) {
#ifdef _WIN32
    winlineHistorySave(HISTORY_FILE);
#else
    linenoiseHistorySave(HISTORY_FILE);
#endif
  }

  // turn down volume smoothly to avoid clicks
  volume_set(0);
  //
  sleep_float(.5); // give a bit of time for the smoothing to apply

  // Cleanup
  perf_stop();
  if (udp_port != 0) udp_stop();
#if 0
  ma_device_uninit(&seq_device);
#endif
  sleep_float(.5); // make sure we don't crash the callback b/c thread timing and wave_data
  ma_device_uninit(&synth_device);
  sleep_float(.5); // make sure we don't crash the callback b/c thread timing and wave_data
  sleep_float(.5); // make sure we don't crash the callback b/c thread timing and wave_data

  wave_free();
  synth_free();
  synth_callback_free();

  show_threads();

  return 0;
}

