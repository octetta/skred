#ifndef _SYNTH_H_
#define _SYNTH_H_

#define ARRAY(type, name, size, init) extern type name[size];
#include "synth.def"
#undef ARRAY

extern int requested_synth_frames_per_callback;
extern int synth_frames_per_callback;

extern volatile uint64_t synth_sample_count;

extern float volume_user;
extern float volume_final;
extern float volume_smoother_gain;
extern float volume_smoother_smoothing;
extern float volume_threshold;
extern float volume_smoother_higher_smoothing;

void audio_rng_init(uint64_t *rng, uint64_t seed);
uint64_t audio_rng_next(uint64_t *rng);
float audio_rng_float(uint64_t *rng);
float osc_get_phase_inc(int v, float f);
void osc_set_freq(int v, float f);
float cz_phasor(int n, float p, float d, int table_size);
float osc_next(int voice, float phase_inc);
void osc_set_wave_table_index(int voice, int wave);
void osc_trigger(int voice);
float quantize_bits_int(float v, int bits);
void mmf_init(int, float, float);
void mmf_set_params(int, float, float);
int mmf_set_freq(int, float);
int mmf_set_res(int, float);
float mmf_process(int n, float input);
void envelope_init(int v, float attack_time, float decay_time,
               float sustain_level, float release_time);
void amp_envelope_trigger(int v, float f);
void amp_envelope_release(int v);
float amp_envelope_step(int v);

int volume_set(float v);

void synth(float *buffer, float *input, int num_frames, int num_channels);

#endif
