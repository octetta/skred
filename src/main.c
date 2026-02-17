/*

 voice = 
 
   wave table index
 
   freq -> phase_inc
   
   freq modulation source, freq modulation depth -> freq

   phase

   phase distortion type, phase distortion amount -> phase
   
   phase distortion modulation source, phase distortion depth -> phase distortion amount
*/

#include <stdio.h>
#include <unistd.h>

#include "skred.h"
#include "util.h"

#include "miniaudio.h"

#include "synth-types.h"
#include "synth.h"

int trace = 0;

#include "skode.h"

int rec_state = 0;
long rec_ptr = 0;
float rec_sec = (float)REC_IN_SEC;
long rec_max = REC_IN_SEC * MAIN_SAMPLE_RATE * AUDIO_CHANNELS * VOICE_MAX;
float one_skred_frame[ONE_FRAME_MAX * AUDIO_CHANNELS * VOICE_MAX];
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

void queue_cb(int voice, char *arg) {
  static skode_t w = SKODE_EMPTY();
  skode_consume(arg, &w);
}

void pattern_cb(int voice, char *arg) {
  static skode_t w = SKODE_EMPTY();
  skode_consume(arg, &w);
}

void synth_callback(ma_device* pDevice, void* output, const void* input, ma_uint32 frame_count) {
  static int first = 1;
  static int num_channels = 1;
  if (first) {
    util_set_thread_name("synth");
    num_channels = (int)pDevice->playback.channels;
    first = 0;
  }
  synth((float *)output, (float *)input, (int)frame_count, (int)pDevice->playback.channels, pDevice->pUserData);
  // copy frame buffer to shared memory?
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
}

void sleep_float(double seconds) {
  if (seconds < 0.0f) return; // Invalid input
  struct timespec ts;
  ts.tv_sec = (time_t)seconds; // Whole seconds
  ts.tv_nsec = (long)((seconds - (double)ts.tv_sec) * 1e9); // Fractional part to nanoseconds
  nanosleep(&ts, NULL);
}



void sload(int use_edit) {
}

char *sgets(char *prompt, int max, int edit, int store) {
  char *buffer = (char *)malloc(max);
  char *line = buffer;
  line = fgets(buffer, max, stdin);
  return line;
}

void ssave(int use_edit) {
}

int main(int argc, char *argv[]) {
  int load_patch_number = -1;
  char execute_from_start[1024] = "";
  int flag = 0; // don't resample retro waves...
  if (argc > 1) {
    for (int i=1; i<argc; i++) {
      if (argv[i][0] == '-') {
        switch (argv[i][1]) {
          case 't': trace = 1; break;
          case 'f': flag = (int)strtol(&(argv[i][2]), NULL, 0); break;
          case 'l': load_patch_number = (int)strtol(&argv[i][2], NULL, 0); break;
          case '1': requested_synth_frames_per_callback = (int)strtol(&argv[i][2], NULL, 0); break;
          case 'e': {
            printf("# %s\n", argv[i]);
            strcpy(execute_from_start, &argv[i][2]);
          } break;
          default:
            printf("# unknown switch '%s'\n", argv[i]);
            printf("# -n = no command line edit\n");
            printf("# -t = trace on\n");
            printf("# -l# = load sk patch #\n");
            printf("# -1# = set synth frames per callback to #\n");
            printf("# -e'wire' = run 'wire' at start\n");
            exit(1);
            break;
        }
      }
    }
  }

  sload(0);
#define YELL(s) {write(1, s, sizeof(s)-1);}

  YELL("#001\n");
  synth_callback_init(REC_IN_SEC);
  YELL("#002\n");
  synth_init();
  YELL("#003\n");
  wave_table_init(flag);
  YELL("#004\n");
  voice_init();

  YELL("#005\n");
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

  YELL("#006\n");
  if (load_patch_number >= 0) skode_load(NULL, 0, load_patch_number);

  skode_t w = SKODE_EMPTY();
  w.trace = trace;
  w.log_enable = 1;

  int main_running = 1;

  if (execute_from_start[0] != '\0') {
    int n = skode_consume(execute_from_start, &w);
    if (w.log_len) printf("%s", w.log);
    if (n < 0) main_running = 0;
  }

  w.flag = 1;

  char *line = NULL;

  printf("# SKRED!\n");

  while (main_running) {
    if (line) free(line); // get rid of previous malloc-ed line
    line = NULL;

    line = sgets("# ", 1024, 0, w.flag);
    if (line == NULL) {
      main_running = 0;
      break;
    }
    if (strlen(line) == 0) continue;

    int n = skode_consume(line, &w);
    if (w.log_len) printf("%s", w.log);
    if (n < 0) break; // request to stop or error
    if (n > 0) printf("# ERR:%d\n", n);
    trace = w.trace;
  }

  ssave(0);
  if (line) free(line); // get rid of previous malloc-ed line

  // turn down volume smoothly to avoid clicks
  volume_set(SILENT);
  //
  sleep_float(.5); // give a bit of time for the smoothing to apply

  // Cleanup
  perf_stop();
  sleep_float(.5); // make sure we don't crash the callback b/c thread timing and wave_data
  ma_device_uninit(&synth_device);
  sleep_float(.5); // make sure we don't crash the callback b/c thread timing and wave_data
  sleep_float(.5); // make sure we don't crash the callback b/c thread timing and wave_data

  wave_free();
  synth_free();
  synth_callback_free();

  show_threads(NULL);

  return 0;
}

