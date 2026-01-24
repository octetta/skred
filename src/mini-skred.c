#include <stdio.h>
#include <unistd.h>

#include "skred.h"
#include "util.h"
#include "miniaudio.h"
#include "synth-types.h"
#include "synth.h"
#include "seq.h"
#include "wire.h"

void queue_cb(int voice, char *arg) {
  static wire_t w = WIRE();
  wire(arg, &w);
}

void pattern_cb(int voice, char *arg) {
  static wire_t w = WIRE();
  wire(arg, &w);
}

void synth_callback(ma_device* pDevice, void* output, const void* input, ma_uint32 frame_count) {
  synth((float *)output, (float *)input, (int)frame_count, (int)pDevice->playback.channels, pDevice->pUserData);
  seq((int)frame_count, queue_cb, pattern_cb);
}

float one_skred_frame[ONE_FRAME_MAX * AUDIO_CHANNELS * VOICE_MAX];

int main(int argc, char *argv[]) {
  synth_init();
  wave_table_init(0);
  voice_init();
  tempo_set(120.0);
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

  wire_t w = WIRE();
  w.trace = 0;
  w.log_enable = 1;

  int main_running = 1;

  char line[1024];
  while (main_running) {
    line[0] = '\0';
    char *out = fgets(line, sizeof(line)-1, stdin);
    if (out == NULL) {
      main_running = 0;
      break;
    }
    if (strlen(line) == 0) continue;

    int n = wire(line, &w);
    if (w.log_len) printf("%s", w.log);
    if (n < 0) break; // request to stop or error
    if (n > 0) printf("# ERR:%d\n", n);
  }

  // turn down volume smoothly to avoid clicks
  volume_set(0);
  sleep(1);
  ma_device_uninit(&synth_device);
  wave_free();
  synth_free();

  return 0;
}
