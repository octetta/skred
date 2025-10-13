#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "miniwav.h"

int mw_put(char *name, int16_t *capture, int frames) {
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
        fwrite(&wave, 1, sizeof(wave), out);
        fwrite(capture, 1, frames * sizeof(int16_t), out);
        fclose(out);
    } else {
        perror("! fopen");
        return -1;
    }
    return frames;
}

int mw_frames(char *name) {
    FILE *in = fopen(name, "rb");

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

float *mw_get(char *name, int *frames_out, wav_t *w) {
    wav_t wav;
    wav_t *this = &wav;
    if (w) this = w;
    float *table = NULL;
    FILE *in = mw_header(name, this);
    if (in) {
      int frames_current = 0;
      int frames_total = this->DataSubchunkSize / this->Channels / (this->BitsPerSample / 8);
      table = (float *)malloc(frames_total * sizeof(float));
      int16_t frameBlock[2];
      while (frames_current < frames_total) {
        int n = fread(frameBlock, sizeof(int16_t) * this->Channels, 1, in);
        if (n < 0) break;
        if (n == 0) break;
        int32_t sample = frameBlock[0];
        if (this->Channels == 2) {
          sample += frameBlock[1];
          sample /= 2;
        }
        if (sample > 32767) sample = 32767;
        if (sample < -32767) sample = -32767;
        float f = (float)sample / 32767;
        table[frames_current] = f;
        frames_current++;
      }
      fclose(in);
      if (frames_out) *frames_out = frames_current;
    }
    return table;
}
