/*

 voice = 
 
   wave table index
 
   freq -> phase_inc
   
   freq modulation source, freq modulation depth -> freq

   phase

   phase distortion type, phase distortion amount -> phase
   
   phase distortion modulation source, phase distortion depth -> phase distortion amount
*/

#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

#include "linenoise.h"
#include "skred.h"

#include "scope-shared.h"

int scope_enable = 0;
scope_buffer_t safety;
scope_buffer_t *new_scope = &safety;

#include "miniaudio.h"

#include "synth-types.h"
#include "synth.h"

float tempo_time_per_step = 10.0f;
float tempo_bpm = 0.0f;

void tempo_set(float);

int debug = 0;
int trace = 0;

int console_voice = 0;

#include "udp.h"

int main_running = 1;

#include "wire.h"


#include "seq.h"

void seq_callback(ma_device* pDevice, void* output, const void* input, ma_uint32 frame_count) {
  static int first = 1;
  if (first) {
    pthread_setname_np(pthread_self(), "seq");
    seq_frames_per_callback = (int)frame_count;
    first = 0;
  }
  seq((int)frame_count);
}

void synth_callback(ma_device* pDevice, void* output, const void* input, ma_uint32 frame_count) {
  static int first = 1;
  if (first) {
    pthread_setname_np(pthread_self(), "synth");
    first = 0;
  }
  synth((float *)output, (float *)input, (int)frame_count, (int)pDevice->playback.channels);
  // copy frame buffer to shared memory?
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
  if (argc > 1) {
    for (int i=1; i<argc; i++) {
      if (argv[i][0] == '-') {
        switch (argv[i][1]) {
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
  
  linenoiseHistoryLoad(HISTORY_FILE);
  
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
  ma_device synth_device;
  ma_device_init(NULL, &synth_config, &synth_device);
  ma_device_start(&synth_device);

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

  if (audio_show() != 0) return 1;
  system_show();

  pthread_setname_np(pthread_self(), "repl");

  udp_start(udp_port);

  if (load_patch_number >= 0) patch_load(0, load_patch_number, 0);

  scope_share_t shared;
  new_scope = scope_setup(&shared, "w");
  new_scope->buffer_len = SCOPE_WIDTH_IN_SAMPLES;
  sprintf(new_scope->status_text, "n/a");

  wire_t w = WIRE();
  w.output = 1;
  w.debug = debug;
  w.trace = trace;

  if (execute_from_start[0] != '\0') {
    int n = wire(execute_from_start, &w);
    if (n < 0) main_running = 0;
  }

  new_scope->voice_text[0] = '\0';

  while (main_running) {
    voice_format(current_voice, new_scope->voice_text, 0);
    char *line = linenoise("# ");
    if (line == NULL) {
      main_running = 0;
      break;
    }
    if (strlen(line) == 0) continue;
    linenoiseHistoryAdd(line);
    int n = wire(line, &w);
    linenoiseFree(line);
    if (n < 0) break; // request to stop or error
    if (n > 0) {
      char *s = wire_err_str(n);
      printf("# %s ERR:%d\n", s, n);
    }
    //linenoiseFree(line);
  }
  linenoiseHistorySave(HISTORY_FILE);

  // turn down volume smoothly to avoid clicks
  volume_set(0);
  //
  sleep_float(.5); // give a bit of time for the smoothing to apply

  // Cleanup
  udp_stop();
  ma_device_uninit(&seq_device);
  sleep_float(.5); // make sure we don't crash the callback b/c thread timing and wave_data
  ma_device_uninit(&synth_device);
  sleep_float(.5); // make sure we don't crash the callback b/c thread timing and wave_data
  sleep_float(.5); // make sure we don't crash the callback b/c thread timing and wave_data

  wave_free();
  synth_free();

  show_threads();

  return 0;
}

