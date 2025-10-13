#ifndef _SYNTH_H_
#define _SYNTH_H_

#define ARRAY(type, name, size, init) extern type name[size];
#include "synth.def"
#undef ARRAY

void synth(float *buffer, float *input, int num_frames, int num_channels);
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

int envelope_is_flat(int v);

int cz_set(int v, int n, float f);
int cmod_set(int voice, int o, float f);

int amp_set(int v, float f);
int pan_set(int voice, float f);
int wave_quant(int voice, int n);
int freq_set(int v, float f);
int voice_set(int n, int *old_voice);
int voice_copy(int v, int n);
int wave_set(int voice, int wave);
int wave_mute(int voice, int state);
int wave_dir(int voice, int state);
int freq_midi(int voice, float f);
int amp_mod_set(int voice, int o, float f);
int envelope_velocity(int voice, float f);
int envelope_set(int voice, float a, float d, float s, float r);
int wave_reset(int voice, int n);
int freq_mod_set(int voice, int o, float f);
int pan_mod_set(int voice, int o, float f);

char *voice_format(int v, char *out, int verbose);
void voice_show(int v, char c, int debug);
int voice_show_all(int voice);
int voice_trigger(int voice);
int wave_default(int voice);
int wave_loop(int voice, int state);
int voice_copy(int v, int n);
float midi2hz(float f);
int voice_set(int n, int *old_voice);
int voice_trigger(int voice);
int wave_default(int voice);
void wave_table_init(void);
void wave_free(void);
void voice_init(void);

#endif
