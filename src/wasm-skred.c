#include <stdio.h>
#include <unistd.h>
#include <emscripten.h>

#include "skred.h"
#include "util.h"
#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"
#include "synth-types.h"
#include "synth.h"
#include "skode.h"

void queue_cb(int voice, char *arg) {
  static skode_t w = SKODE_EMPTY();
  skode_consumearg, &w);
}

void pattern_cb(int voice, char *arg) {
  static skode_t w = SKODE_EMPTY();
  skode_consumearg, &w);
}

void synth_callback(ma_device* pDevice, void* output, const void* input, ma_uint32 frame_count) {
  synth((float *)output, (float *)input, (int)frame_count, (int)pDevice->playback.channels, pDevice->pUserData);
}

float one_skred_frame[ONE_FRAME_MAX * AUDIO_CHANNELS * VOICE_MAX];

skode_t w;

ma_device_config synth_config;
ma_device synth_device;

int is_initialized = 0;

int main(int argc, char *argv[]) {
  emscripten_log(EM_LOG_CONSOLE, "Hello from Emscripten Log!");
  printf("[C] skred init...\n");

  skode_init(&w);
  synth_init();
  wave_table_init(0);
  voice_init();

  printf("[C] ma init...\n");

  synth_config = ma_device_config_init(ma_device_type_playback);
  synth_config.playback.format = ma_format_f32;
  synth_config.playback.channels = AUDIO_CHANNELS;
  synth_config.sampleRate = MAIN_SAMPLE_RATE;
  synth_config.dataCallback = synth_callback;
  synth_config.periodSizeInFrames = requested_synth_frames_per_callback;
  synth_config.periodSizeInMilliseconds = 0;
  synth_config.periods = 3;
  synth_config.noClip = MA_TRUE;
  synth_config.pUserData = &one_skred_frame;
  ma_device_init(NULL, &synth_config, &synth_device);
  ma_device_start(&synth_device);

  w.trace = 0;
  w.log_enable = 1;
  
  is_initialized = 1;

  printf("[C] skred running? ...\n");
  printf("[C] end of main\n");

  return 0;
}

EMSCRIPTEN_KEEPALIVE
void process_input_string(char *input) {
  if (!is_initialized) return;
  printf("[C] got %s\n", input);

  if (ma_device_get_state(&synth_device) != ma_device_state_started) {
    ma_device_start(&synth_device);
    printf("[C] audio device started\n");
  }

  int n = skode_consumeinput, &w);
  printf("[C] returned %d\n", n);
}
