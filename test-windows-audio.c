#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"
#include <math.h>

typedef struct {
    double phase;
} user_data;

void callback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount)
{
    user_data* ud = (user_data*)pDevice->pUserData;
    float* out = (float*)pOutput;
    double freq = 440.0;
    double sr = pDevice->sampleRate;

    for (ma_uint32 i = 0; i < frameCount; i++) {
        double sample = sin(ud->phase * 2 * M_PI);
        ud->phase += freq / sr;
        if (ud->phase >= 1.0) ud->phase -= 1.0;
        out[2 * i + 0] = (float)sample * 0.2f;
        out[2 * i + 1] = (float)sample * 0.2f;
    }
}

int main(void)
{
    ma_result result;
    ma_device_config config;
    ma_device device;
    user_data ud = {0};

    config = ma_device_config_init(ma_device_type_playback);
    config.playback.format   = ma_format_f32;
    config.playback.channels = 2;
    config.sampleRate        = 48000;
    config.dataCallback      = callback;
    config.pUserData         = &ud;

    result = ma_device_init(NULL, &config, &device);
    if (result != MA_SUCCESS) {
        printf("Failed to init device\n");
        return -1;
    }

    ma_device_start(&device);
    printf("Playing 440Hz tone... Press Ctrl+C to exit.\n");
    while (1) ma_sleep(100);
}
