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

// interesting chunk that i might look for one day
typedef struct {
    char      RIFFChunkID[4];
    uint32_t  RIFFChunkSize;
    uint32_t  Manufacturer;
    uint32_t  Product;
    uint32_t  SamplePeriod;
    uint32_t  MIDIUnityNote;
    uint32_t  MIDIPitchFraction;
    uint32_t  SMPTEFormat;
    uint32_t  SMPTEOffset;
    uint32_t  SampleLoops;
    uint32_t  SamplerData;
    // hardcoded for one loop but SampleLoops by
    // the standard supports more than one
    // typedef struct {
    uint32_t Identifier;
    uint32_t Type;
    uint32_t Start;
    uint32_t End;
    uint32_t Fraction;
    uint32_t PlayCount;
    // } SampleLoop;
} sampler_t;

FILE *mw_header(char *name, wav_t *wav);

float *mw_get(char *name, int *frames_out, wav_t *w);

#endif
