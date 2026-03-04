#ifndef _SYNTH_H_
#define _SYNTH_H_

#include "synth-features.h"
#include "synth-config.h"
#include "synth-state.h"

#include "portable_atomic.h"

//extern atomic_uint64_t synth_sample_count;
#define SAMPLE_COUNT_PUT(n) atomic_store_uint64(&synth_sample_count, n)
#define SAMPLE_COUNT_GET() atomic_load_uint64(&synth_sample_count)
#define SAMPLE_COUNT_ADD(n) atomic_fetch_add_uint64(&synth_sample_count, n)

void synth(float *buffer, float *input, int num_frames, int num_channels, void *user);
void synth_init(void);
void synth_free(void);

extern int requested_synth_frames_per_callback;
extern int synth_frames_per_callback;

extern volatile uint64_t synth_sample_count;

extern float volume_user;
extern float volume_final;
extern float volume_smoother_gain;
extern float volume_smoother_smoothing;
extern float volume_threshold;
extern float volume_smoother_higher_smoothing;

      #ifdef SYNTH_FEATURE_QUANTIZE
      int wave_quant(int voice, int n);
      float quantize_bits_int(float v, int bits);
      #endif

      #ifdef SYNTH_FEATURE_FILTER
      void mmf_init(int, float, float);
      void mmf_set_params(int, float, float);
      int  mmf_set_freq(int, float);
      int  mmf_set_res(int, float);
      float mmf_process(int n, float input);
      #endif

      #ifdef SYNTH_FEATURE_PHASE_DISTORTION
      float cz_phasor(int n, float p, float d, int table_size);
      int   cz_set(int v, int n, float f);
      int   cmod_set(int voice, int o, float f);
      #endif

      #ifdef SYNTH_FEATURE_AMP_ENVELOPE
      void  envelope_init(int v, float a, float d, float s, float r);
      void  amp_envelope_trigger(int v, float f);
      void  amp_envelope_release(int v);
      float amp_envelope_step(int v);
      int   envelope_is_flat(int v);
      int   envelope_velocity(int voice, float f);
      int   envelope_set(int voice, float a, float d, float s, float r);
      #endif

void audio_rng_init(uint64_t *rng, uint64_t seed);
uint64_t audio_rng_next(uint64_t *rng);
float audio_rng_float(uint64_t *rng);
float osc_get_phase_inc(int v, float f);
void osc_set_freq(int v, float f);
float osc_next(int voice, float phase_inc);
void osc_set_wave_table_index(int voice, int wave);
void osc_trigger(int voice);

int volume_set(float v);

int amp_set(int v, float f);
int pan_set(int voice, float f);
int freq_set(int v, float f);
int voice_set(int n, int *old_voice);
int voice_copy(int v, int n);
int wave_set(int voice, int wave);
int wave_mute(int voice, int state);
int wave_dir(int voice, int state);
int freq_midi(int voice, float note);
int amp_mod_set(int voice, int o, float f, float a);
int wave_reset(int voice, int n);
int freq_mod_set(int voice, int o, float f, float a);
int pan_mod_set(int voice, int o, float f, float a);

char *voice_format(int v, char *out, int verbose);
//void voice_show(int v, char c, int verbose);
//int voice_show_all(int voice, int verbose);
int voice_trigger(int voice);
int wave_default(int voice);
int wave_loop(int voice, int state);
int voice_copy(int v, int n);
float midi2hz(float f);
int voice_set(int n, int *old_voice);
int voice_trigger(int voice);
int wave_default(int voice);
void wave_table_init(int flag);
void wave_free(void);
void wave_free_one(int i);
void voice_init(void);

char *synth_stats(void);
void synth_voice_bench(int voice);

void normalize_preserve_zero(float *data, int length);

void envelope_init_e(envelope_t *e, float a, float d, float s, float r);
void envelope_trigger_e(envelope_t *e, float f);
void envelope_release_e(envelope_t *e);
float envelope_step_e(envelope_t *e);

#endif
