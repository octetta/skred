#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "miniwav.h"

// typedef struct {
//     char      RIFFChunkID[4];
//     uint32_t  RIFFChunkSize;
//     uint32_t  Manufacturer;
//     uint32_t  Product;
//     uint32_t  SamplePeriod;
//     uint32_t  MIDIUnityNote;
//     uint32_t  MIDIPitchFraction;
//     uint32_t  SMPTEFormat;
//     uint32_t  SMPTEOffset;
//     uint32_t  SampleLoops;
//     uint32_t  SamplerData;
//     // hardcoded for one loop but SampleLoops by
//     // the standard supports more than one
//     // typedef struct {
//     uint32_t Identifier;
//     uint32_t Type;
//     uint32_t Start;
//     uint32_t End;
//     uint32_t Fraction;
//     uint32_t PlayCount;
//     // } SampleLoop;
// } sampler_t;

//void capture_to_wav(char *name) {
int mw_put(char *name, int16_t *capture, int frames) {
    // int16_t *capture = amy_captured();
    // int frames = amy_frames();
    wav_t wave = {
        .RIFFChunkID = {'R', 'I', 'F', 'F'},
        .RIFFChunkSize = frames + 36,
        .Format = {'W', 'A', 'V', 'E'},
        .FormatSubchunkID = {'f', 'm', 't', ' '},
        .FormatSubchunkSize = 16,
        .AudioFormat = 1,
        .Channels = 1,
        .SamplesRate = 44100,
        .ByteRate = 44100,
        .BlockAlign = 2,
        .BitsPerSample = 16,
        .DataSubchunkID = {'d', 'a', 't', 'a'},
        .DataSubchunkSize = frames,
    };
    FILE *out = NULL;
    out = fopen(name, "wb+");
    if (out) {
        int n;
        n = fwrite(&wave, 1, sizeof(wave), out);
        n = fwrite(capture, 1, frames * sizeof(int16_t), out);
        fclose(out);
    } else {
        perror("! fopen");
        return -1;
    }
    return frames;
}

int mw_frames(char *name) {
    FILE *in = fopen(name, "rb");
    int length = 0;

    if (in) {
        wav_t wav;
        int frames = -1;
        int n = fread(&wav, sizeof(wav_t), 1, in);
        if (n > 0) {
            while (1) {
                if (strncmp(wav.RIFFChunkID, "RIFF", 4) != 0) break;
                if (strncmp(wav.Format, "WAVE", 4) != 0) break;
                if (strncmp(wav.FormatSubchunkID, "fmt ", 4) != 0) break;
                if (wav.Channels > 2) break;
                if (wav.SamplesRate != 44100) break;
                if (wav.BitsPerSample != 16) break;
                if (strncmp(wav.DataSubchunkID, "data", 4) != 0) break;
                frames = wav.DataSubchunkSize / wav.Channels / (wav.BitsPerSample / 8);
                break;
            }
        }
        fclose(in);
        return frames;
    }
    return -1;    
}

FILE *mw_header(char *name, wav_t *wav) {
  if (!wav) return NULL;
  FILE *in = fopen(name, "rb");
  if (in) {
    int n = fread(wav, sizeof(wav_t), 1, in);
    if (n > 0) {
      while (1) {
        if (strncmp(wav->RIFFChunkID, "RIFF", 4) != 0) break;
        if (strncmp(wav->Format, "WAVE", 4) != 0) break;
        if (strncmp(wav->FormatSubchunkID, "fmt ", 4) != 0) break;
        if (wav->Channels > 2) break;
        if (wav->SamplesRate != 44100) break;
        if (wav->BitsPerSample != 16) break;
        if (strncmp(wav->DataSubchunkID, "data", 4) != 0) break;
        return in;
      }
      fclose(in);
    }
  }
  return NULL;
}

float *mw_get(char *name, int *frames_out) {
    wav_t wav;
    int r = -1;
    float *table = NULL;
    FILE *in = mw_header(name, &wav);
    if (in) {
      int frames_current = 0;
      int frames_total = wav.DataSubchunkSize / wav.Channels / (wav.BitsPerSample / 8);
      table = (float *)malloc(frames_total * sizeof(float));
      int16_t frameBlock[2];
      while (frames_current < frames_total) {
        int n = fread(frameBlock, sizeof(int16_t) * wav.Channels, 1, in);
        if (n < 0) break;
        if (n == 0) break;
        int32_t sample = frameBlock[0];
        if (wav.Channels == 2) {
          sample += frameBlock[1];
          sample /= 2;
        }
        if (sample > 32767) sample = 32767;
        if (sample < -32767) sample = -32767;
        float f = (float)sample / 32767;
        table[frames_current] = f;
        // double rate = 1000.0 / (double)wav.SamplesRate;
        // double msec = rate * (double)frames;
        // int16_t *dest;
        frames_current++;
      }
      fclose(in);
      if (frames_out) *frames_out = frames_current;
    }
    return table;
}
