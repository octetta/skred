#ifndef SYNTH_ALLOC_H
#define SYNTH_ALLOC_H

/*
 * synth-alloc.h — declarations for the voice/wave heap allocators.
 *
 * Call order:
 *   1. synth_config_defaults()          — sets voice_max/wave_table_max to
 *                                         compiled-in defaults (rounded to
 *                                         VOICE_ALIGN).  synth_init() does
 *                                         this automatically if you forget,
 *                                         but calling it from argv parsing
 *                                         lets you override with -v/-w first.
 *   2. synth_config_set_voices(n)       — optional, from argv
 *   3. synth_init()                     — calls synth_alloc_voices/waves
 *   4. ... run ...
 *   5. synth_free()                     — calls synth_free_voices/waves
 */

void synth_config_defaults(void);
void synth_config_set_voices(int n);
void synth_config_set_waves(int n);

void synth_alloc_voices(int voice_max);
void synth_free_voices(void);

void synth_alloc_waves(int wave_table_max);
void synth_free_waves(void);

#endif /* SYNTH_ALLOC_H */
