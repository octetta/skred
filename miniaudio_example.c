#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"
#include <emscripten.h>
#include <math.h>
#include <stdio.h>

// Audio context structure to hold state
typedef struct {
    double phase;
    double frequency;
    double sample_rate;
    int channels;
} AudioContext;

// Data callback function - this is where you generate audio
void data_callback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount)
{
    AudioContext* pContext = (AudioContext*)pDevice->pUserData;
    float* pOutputF32 = (float*)pOutput;
    
    (void)pInput; // Unused parameter
    
    // Generate a simple sine wave
    for (ma_uint32 i = 0; i < frameCount; ++i) {
        float sample = (float)(sin(pContext->phase) * 0.3); // 0.3 for volume control
        
        // Output to all channels
        for (int channel = 0; channel < pContext->channels; ++channel) {
            pOutputF32[i * pContext->channels + channel] = sample;
        }
        
        // Advance phase
        pContext->phase += 2.0 * M_PI * pContext->frequency / pContext->sample_rate;
        if (pContext->phase >= 2.0 * M_PI) {
            pContext->phase -= 2.0 * M_PI;
        }
    }
}

// Global variables
ma_device device;
AudioContext audioContext = {0};

// Function to start audio (called from JavaScript or button click)
EMSCRIPTEN_KEEPALIVE
int start_audio() {
    ma_device_config deviceConfig;
    ma_result result;
    
    // Initialize audio context
    audioContext.phase = 0.0;
    audioContext.frequency = 440.0; // A4 note
    audioContext.sample_rate = 44100.0;
    audioContext.channels = 2; // Stereo
    
    // Configure the device
    deviceConfig = ma_device_config_init(ma_device_type_playback);
    deviceConfig.playback.format   = ma_format_f32;
    deviceConfig.playback.channels = audioContext.channels;
    deviceConfig.sampleRate        = (ma_uint32)audioContext.sample_rate;
    deviceConfig.dataCallback      = data_callback;
    deviceConfig.pUserData         = &audioContext;
    
    // Initialize the device
    result = ma_device_init(NULL, &deviceConfig, &device);
    if (result != MA_SUCCESS) {
        printf("Failed to initialize playback device: %d\n", result);
        return -1;
    }
    
    // Start the device
    result = ma_device_start(&device);
    if (result != MA_SUCCESS) {
        printf("Failed to start playback device: %d\n", result);
        ma_device_uninit(&device);
        return -2;
    }
    
    printf("Audio started successfully!\n");
    return 0;
}

// Function to stop audio
EMSCRIPTEN_KEEPALIVE
void stop_audio() {
    ma_device_stop(&device);
    ma_device_uninit(&device);
    printf("Audio stopped.\n");
}

// Function to change frequency
EMSCRIPTEN_KEEPALIVE
void set_frequency(double freq) {
    audioContext.frequency = freq;
    printf("Frequency set to: %.2f Hz\n", freq);
}

int main() {
    printf("Miniaudio Emscripten example ready.\n");
    printf("Call start_audio() to begin audio generation.\n");
    
    // Keep the main thread alive
    emscripten_set_main_loop(NULL, 0, 0);
    
    return 0;
}
