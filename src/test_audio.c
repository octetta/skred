#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"
#include <stdio.h>
#include <math.h>
#include <emscripten.h>

#define SAMPLE_RATE 48000
#ifndef M_PI
    #define M_PI 3.14159265358979323846
#endif

float g_phase = 0.0f;

void data_callback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount) {
    float* pOut = (float*)pOutput;
    float frequency = 440.0f;
    float phaseStep = (2.0f * M_PI * frequency) / SAMPLE_RATE;

    for (ma_uint32 i = 0; i < frameCount; ++i) {
        float sample = sinf(g_phase);
        
        // Fill both channels (stereo)
        for (ma_uint32 channel = 0; channel < pDevice->playback.channels; ++channel) {
            pOut[i * pDevice->playback.channels + channel] = sample * 0.2f; // Volume at 20%
        }

        g_phase += phaseStep;
        if (g_phase > 2.0f * M_PI) g_phase -= 2.0f * M_PI;
    }
    (void)pInput;
}

ma_device g_device;

int main() {
    printf("[C] WASM Audio Test Initialized. Waiting for JS call...\n");
    
    ma_device_config config = ma_device_config_init(ma_device_type_playback);
    config.playback.format   = ma_format_f32;
    config.playback.channels = 2;
    config.sampleRate        = SAMPLE_RATE;
    config.dataCallback      = data_callback;

    if (ma_device_init(NULL, &config, &g_device) != MA_SUCCESS) {
        printf("[C] Failed to init device\n");
        return -1;
    }
    return 0;
}

EMSCRIPTEN_KEEPALIVE
void start_audio() {
    if (ma_device_get_state(&g_device) != ma_device_state_started) {
        ma_result result = ma_device_start(&g_device);
        if (result == MA_SUCCESS) {
            printf("[C] Sine Wave Playing!\n");
        } else {
            printf("[C] Failed to start: %d\n", result);
        }
    }
}
