#ifndef _MINIWAV_H_
#define _MINIWAV_H_
#include <stdint.h>

typedef struct {
    char      RIFFChunkID[4];
    uint32_t  RIFFChunkSize;
    char      Format[4];
    char      FormatSubchunkID[4];
    uint32_t  FormatSubchunkSize;
    uint16_t  AudioFormat;
    uint16_t  Channels;
    uint32_t  SamplesRate;
    uint32_t  ByteRate;
    uint16_t  BlockAlign;
    uint16_t  BitsPerSample;
    char      DataSubchunkID[4];
    uint32_t  DataSubchunkSize;
    // double* Data; 
} wav_t;

FILE *mw_header(char *name, wav_t *wav);

float *mw_get(char *name, int *frames_out);

#endif
