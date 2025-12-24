#include <float.h>
#include <string.h>
#include <stdbool.h>
#include <stdio.h>

#include "skred.h"

#include "synth-types.h"

#include "miniwav.h"

#define USE_PRE

// #undef USE_PRE

#ifdef USE_PRE

#define ARRAY(type, name, size, init) type name[size] = init;
#include "synth.def"
#undef ARRAY

#else

#define ARRAY(type, name, size, init) type *name;
#include "synth.def"
#undef ARRAY

#endif

#define ARRAY(type, name, size, init) int name##__len__ = size;
#include "synth.def"
#undef ARRAY

#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>

void synth_init(void) {

  if (debug) {
#define ARRAY(type, name, size, init) printf("%s : %d\n", #name, name##__len__);
#include "synth.def"
#undef ARRAY
  }

#ifdef USE_PRE

  printf("# synth_init :: USE_PRE\n");

#else

  printf("# synth_init :: USE_MALLOC\n");
#define ARRAY(type, name, size, init) name = (type*)malloc(size * sizeof(type));
#include "synth.def"
#undef ARRAY

#define ARRAY(type, name, size, init) if (name == NULL) printf("malloc %s failed\n", #name);
#include "synth.def"
#undef ARRAY

#endif
}

void synth_free(void) {
#ifdef USE_PRE

  printf("# synth_free :: USE_PRE\n");
  //
  
#else
  
  printf("# synth_free :: USE_FREE\n");

#define ARRAY(type, name, size, init) free(name);
#include "synth.def"
#undef ARRAY

#endif
}

int requested_synth_frames_per_callback = SYNTH_FRAMES_PER_CALLBACK;
int synth_frames_per_callback = 0;

volatile uint64_t synth_sample_count = 0;

#define SMOOTH_DEFAULT (0.02f)

float volume_user = 1.0f;
float volume_final = AMY_FACTOR;
float volume_smoother_gain = 0.0f;
float volume_smoother_smoothing = 0.002f;
float volume_threshold = 0.05f;
float volume_smoother_higher_smoothing = 0.3f;

int volume_set(float v) {
  volume_user = v;
  volume_final = v * AMY_FACTOR;
  return 0;
}

void audio_rng_init(uint64_t *rng, uint64_t seed);
float audio_rng_float(uint64_t *rng);

void audio_rng_init(uint64_t *rng, uint64_t seed) {
  *rng = seed ? seed : 1; // Ensure non-zero seed
}

// Generate next random number (full 64-bit range)
uint64_t audio_rng_next(uint64_t *rng) {
    // High-quality LCG parameters (Knuth's MMIX)
    *rng = *rng * 6364136223846793005ULL + 1442695040888963407ULL;
    return *rng;
}

// Generate random float in range [-1.0, 1.0] for audio
float audio_rng_float(uint64_t *rng) {
    uint64_t raw = audio_rng_next(rng);
    // Use upper 32 bits for better quality
    uint32_t val = (uint32_t)(raw >> 32);
    // Convert to signed float [-1.0, 1.0]
    return (float)((int32_t)val) / 2147483648.0f;
}

float osc_get_phase_inc(int v, float f) {
  // Compute the frequency in "table samples per system sample"
  // This works even if table_rate ≠ system rate
  float g = f;
  if (voice_one_shot[v]) g /= voice_offset_hz[v];
  float phase_inc = (g * (float)voice_table_size[v]) / voice_table_rate[v] * (voice_table_rate[v] / MAIN_SAMPLE_RATE);
  return phase_inc;
}

void osc_set_freq(int v, float f) {
  voice_phase_inc[v] = osc_get_phase_inc(v, f);
}

// Fast power approximation using bit manipulation
// About 10x faster than powf, ~1-2% max error for your use case
static inline float fast_pow(float a, float b) {
    // Clamp input to avoid undefined behavior
    if (a <= 0.0f) return 0.0f;
    
    union { float f; int i; } u = { a };
    u.i = (int)(b * (u.i - 1065353216) + 1065353216);
    return u.f;
}

float cz_phasor(int n, float p, float d, int table_size) {
    const float table_size_f = (float)table_size;
    float phase = p / table_size_f;
    
    // Clamp d to safe range [0, 1)
    d = (d < 0.0f) ? 0.0f : (d > 0.999f ? 0.999f : d);
    
    switch (n) {
        case 1: { // saw -> pulse
            const float inv_d = 0.5f / d;
            const float inv_1_minus_d = 0.5f / (1.0f - d);
            if (phase < d) {
                phase *= inv_d;
            } else {
                phase = 0.5f + (phase - d) * inv_1_minus_d;
            }
            break;
        }
        case 2: { // square (folded sine)
            const float half_d = d * 0.5f;
            const float scale = 0.5f / (0.5f - half_d);
            if (phase < 0.5f) {
                phase *= scale;
            } else {
                phase = 1.0f - (1.0f - phase) * scale;
            }
            break;
        }
        case 3: { // triangle
            const float half_d = d * 0.5f;
            const float scale = 0.5f / (0.5f - half_d);
            if (phase < 0.5f) {
                phase *= scale;
            } else {
                phase = 0.5f + (phase - 0.5f) * scale;
            }
            break;
        }
        case 4: { // double sine
            // phase *= 2.0f;
            // if (phase >= 1.0f) phase -= 1.0f;
            phase = fmodf(phase * 2.0f, 1.0f);
            break;
        }
        case 5: { // saw -> triangle
            const float half_d = d * 0.5f;
            const float scale1 = 0.5f / (0.5f - half_d);
            const float scale2 = 0.5f / (0.5f + half_d);
            if (phase < 0.5f) {
                phase *= scale1;
            } else {
                phase = 0.5f + (phase - 0.5f) * scale2;
            }
            break;
        }
        case 6: // resonant 1
            phase = fast_pow(phase, 1.0f + 4.0f * d);
            break;
        case 7: // resonant 2
            phase = fast_pow(phase, 1.0f + 8.0f * d);
            break;
        default:
            return p;
    }
    
    return phase * table_size_f;
}

float osc_next(int voice, float phase_inc) {
    if (voice_finished[voice]) return 0.0f;
    
    const int table_size = voice_table_size[voice];
    const bool one_shot = voice_one_shot[voice];
    const bool loop_enabled = voice_loop_enabled[voice];
    
    if (voice_direction[voice]) phase_inc = -phase_inc;
    
    float phase = voice_phase[voice] + phase_inc;
    
    if (!isfinite(phase)) {
        voice_phase[voice] = 0.0f;
        voice_finished[voice] = one_shot;
        return 0.0f;
    }
    
    // Get loop boundaries (precomputed if available)
    const float loop_start = loop_enabled && voice_loop_valid[voice] 
        ? voice_loop_start_f[voice] : 0.0f;
    const float loop_end = loop_enabled && voice_loop_valid[voice]
        ? voice_loop_end_f[voice] : (float)table_size;
    const float loop_length = loop_end - loop_start;
    
    // Wrap phase
    if (phase >= loop_end) {
        if (one_shot && !loop_enabled) {
            phase = loop_end - 1e-6f;
            voice_finished[voice] = 1;
        } else {
            phase = loop_start + fmodf(phase - loop_start, loop_length);
        }
    } else if (phase < loop_start) {
        if (one_shot && !loop_enabled) {
            phase = loop_start;
            voice_finished[voice] = 1;
        } else {
            phase = loop_end - fmodf(loop_start - phase, loop_length);
        }
    }
    
    voice_phase[voice] = phase;
    
    // Get sample
    int idx;
    if (voice_cz_mode[voice]) {
        int dv = voice_cz_mod_osc[voice];
        float dm = (dv >= 0) ? voice_sample[dv] * voice_cz_mod_depth[voice] : 1.0f;
        idx = (int)cz_phasor(voice_cz_mode[voice], phase, 
                             voice_cz_distortion[voice] + dm, table_size);
    } else {
        idx = (int)phase;
    }
    
    if (idx >= table_size) idx = table_size - 1;
    if (idx < 0) idx = 0;
    
    return voice_table[voice][idx];
}

void osc_set_wave_table_index(int voice, int wave) {
  if (wave_table_data[wave] && wave_size[wave] && wave_rate[wave] > 0.0) {
    voice_wave_table_index[voice] = wave;
    int update_freq = 0;
    if (wave_one_shot[wave]) voice_finished[voice] = 1;
    else voice_finished[voice] = 0;
    if (
      voice_table_rate[voice] != wave_rate[wave] ||
      voice_table_size[voice] != wave_size[wave]
      ) update_freq = 1;
    voice_table_rate[voice] = wave_rate[wave];
    voice_table_size[voice] = wave_size[wave];
    voice_table[voice] = wave_table_data[wave];
    voice_one_shot[voice] = wave_one_shot[wave];
    voice_loop_start[voice] = wave_loop_start[wave];
    voice_loop_enabled[voice] = wave_loop_enabled[wave];
    voice_loop_end[voice] = wave_loop_end[wave];
    voice_midi_note[voice] = wave_midi_note[wave];
    voice_offset_hz[voice] = wave_offset_hz[wave];
    //
    int start = voice_loop_start[voice];
    int end = voice_loop_end[voice];
    voice_loop_start_f[voice] = (float)start;
    voice_loop_end_f[voice] = (float)end;
    if (end > start) {
      voice_loop_valid[voice] = 1;
      voice_loop_length[voice] = (float)(end - start);
    } else {
      voice_loop_valid[voice] = 0;
      voice_loop_length[voice] = (float)voice_table_size[voice];
    }
    //
    // voice_phase[voice] = 0; // need to decide how to sync/reset phase???
    if (update_freq) {
      osc_set_freq(voice, voice_freq[voice]);
    }
  }
}

void osc_trigger(int voice) {
    voice_finished[voice] = 0;
    
    if (voice_one_shot[voice]) {
        if (voice_direction[voice]) {
            voice_phase[voice] = (float)(voice_table_size[voice] - 1);
        } else {
            voice_phase[voice] = 0.0f;
        }
    } else {
        // Preserve direction, but start at appropriate boundary
        if (voice_direction[voice]) {
            // Backward playback: start at loop end
            voice_phase[voice] = voice_loop_enabled[voice] 
                ? (float)voice_loop_end[voice] - 1e-6f  // or voice_loop_end_f[voice]
                : (float)(voice_table_size[voice] - 1);
        } else {
            // Forward playback: start at loop start
            voice_phase[voice] = voice_loop_enabled[voice] 
                ? (float)voice_loop_start[voice]  // or voice_loop_start_f[voice]
                : 0.0f;
        }
    }
}

float quantize_bits_int(float v, int bits) {
  int levels = (1 << bits) - 1;
  int iv = (int)(v * (float)levels + 0.5);
  return (float)iv * (1.0f / (float)levels);
}

// Process a single sample through the filter - VERY FAST
// Only multiplication and addition, no transcendental functions
float mmf_process(int n, float input) {
    // Calculate output using Direct Form II - only 5 multiplies, 4 adds
    float output = voice_filter[n].b0 * input +
                  voice_filter[n].b1 * voice_filter[n].x1 +
                  voice_filter[n].b2 * voice_filter[n].x2 -
                  voice_filter[n].a1 * voice_filter[n].y1 -
                  voice_filter[n].a2 * voice_filter[n].y2;
    
    // Update delay lines
    voice_filter[n].x2 = voice_filter[n].x1;
    voice_filter[n].x1 = input;
    voice_filter[n].y2 = voice_filter[n].y1;
    voice_filter[n].y1 = output;
    
    return output;
}

// Initialize the envelope
void envelope_init(int v, float attack_time, float decay_time,
               float sustain_level, float release_time) {
    voice_amp_envelope[v].a = attack_time;
    voice_amp_envelope[v].d = decay_time;
    voice_amp_envelope[v].s = sustain_level;
    voice_amp_envelope[v].r = release_time;
    voice_amp_envelope[v].attack_time = attack_time * MAIN_SAMPLE_RATE; // convert seconds to samples
    voice_amp_envelope[v].decay_time = decay_time * MAIN_SAMPLE_RATE;
    voice_amp_envelope[v].sustain_level = fmaxf(0, fminf(1.0f, sustain_level)); // clamp 0 to 1
    voice_amp_envelope[v].release_time = release_time * MAIN_SAMPLE_RATE;
    voice_amp_envelope[v].sample_start = 0;
    voice_amp_envelope[v].sample_release = 0;
    voice_amp_envelope[v].is_active = 0;
}

// Trigger the envelope (note on)
void amp_envelope_trigger(int v, float f) {
    voice_amp_envelope[v].sample_start = synth_sample_count;
    voice_amp_envelope[v].sample_release = 0;
    voice_amp_envelope[v].velocity = f;
    voice_amp_envelope[v].is_active = 1;
}

// Release the envelope (note off)
void amp_envelope_release(int v) {
    if (voice_amp_envelope[v].is_active) {
        voice_amp_envelope[v].sample_release = synth_sample_count;
    }
}

// Get the current amplitude (0 to 1) at given sample count
float amp_envelope_step(int v) {
    if (!voice_amp_envelope[v].is_active) return 0;

    float samples_since_start = (float)(synth_sample_count - voice_amp_envelope[v].sample_start);

    // Attack phase
    if (samples_since_start < voice_amp_envelope[v].attack_time) {
        return samples_since_start / voice_amp_envelope[v].attack_time; // linear ramp up
    }

    // Decay phase
    float decay_start = voice_amp_envelope[v].attack_time;
    if (samples_since_start < decay_start + voice_amp_envelope[v].decay_time) {
        float samples_in_decay = samples_since_start - decay_start;
        float decay_progress = samples_in_decay / voice_amp_envelope[v].decay_time;
        return 1.0f - decay_progress * (1.0f - voice_amp_envelope[v].sustain_level); // linear decay to sustain
    }

    // Sustain phase
    if (voice_amp_envelope[v].sample_release == 0) {
        return voice_amp_envelope[v].sustain_level;
    }

    // Release phase
    float samples_since_release = (float)(synth_sample_count - voice_amp_envelope[v].sample_release);
    if (samples_since_release < voice_amp_envelope[v].release_time) {
        float release_progress = samples_since_release / voice_amp_envelope[v].release_time;
        return voice_amp_envelope[v].sustain_level * (1.0f - release_progress); // linear ramp down
    }

    // Envelope finished
    voice_amp_envelope[v].is_active = 0;
    return 0.0f;
}

#include <time.h>

typedef struct {
  struct timespec a;
  struct timespec b;
  int64_t diff;
  int state;
  int order;
  int frames;
} sben_t;

#define BENLEN (16)

enum { BEN_0, BEN_A, BEN_B, BEN_D };

static sben_t bench[BENLEN] = {};
static int benchp = 0;
static int64_t bencho = 0;

int64_t ts_diff_ns(const struct timespec *a, const struct timespec *b) {
  return ((int64_t)b->tv_sec  - a->tv_sec)  * 1000000000LL +
    ((int64_t)b->tv_nsec - a->tv_nsec);
}

static char _stats[65536] = "";

#define NS_TO_MS (1000000)
#define S_TO_MS (1000)

char *synth_stats(void) {
  char *ptr = _stats;
  *ptr = '\0';
  int n = 0;
  for (int i = 0; i < BENLEN; i++) {
    if (bench[i].state != BEN_B) continue;
    double maxcb = (double)bench[i].frames / (double)MAIN_SAMPLE_RATE * (double)S_TO_MS;
    double dms = ts_diff_ns(&bench[i].a, &bench[i].b) / (double)NS_TO_MS;
    n = sprintf(ptr,
      "# %d %d %gms %gms\n",
      bench[i].order,
      bench[i].frames,
      dms,
      maxcb);
    ptr += n;
    bench[i].state = BEN_0;
  }
  return _stats;
}

#define BENCH_CLOCK CLOCK_MONOTONIC
#ifdef _IS_OSX_
#define VOICE_CLOCK CLOCK_MONOTONIC
#else
#define VOICE_CLOCK CLOCK_MONOTONIC_COARSE
#endif

void synth_voice_bench(int voice) {
  voice_mark_b[voice].tv_sec = 0;
  voice_mark_b[voice].tv_nsec = 0;
  clock_gettime(VOICE_CLOCK, &voice_mark_a[voice]);
  voice_mark_go[voice] = 1;
}

void synth(float *buffer, float *input, int num_frames, int num_channels, void *user) {
  static float *one_skred_frame;
  static uint64_t synth_random;
  static int first = 1;
  if (first) {
    synth_frames_per_callback = num_frames;
    audio_rng_init(&synth_random, 1);
    one_skred_frame = (float *)user;
    first = 0;
  }
  clock_gettime(BENCH_CLOCK, &bench[benchp].a);
  bench[benchp].frames = num_frames;
  bench[benchp].order = bencho;
  bench[benchp].state = BEN_A;
  if (benchp == (BENLEN-1)) {
    // compute min max here
  }
  int skred_ptr = 0;
  for (int i = 0; i < num_frames; i++) {
    synth_sample_count++;
    float sample_left = 0.0f;
    float sample_right = 0.0f;
    float f = 0.0f;
    float whiteish = audio_rng_float(&synth_random);
    for (int n = 0; n < VOICE_MAX; n++) {
      if (voice_mark_go[n]) {
        clock_gettime(VOICE_CLOCK, &voice_mark_b[n]);
        voice_mark_go[n] = 0;
      }
      if (voice_finished[n]) {
        voice_sample[n] = 0.0f;
        one_skred_frame[skred_ptr++] = 0.0f;
        one_skred_frame[skred_ptr++] = 0.0f;
        continue;
      }  
      if (voice_amp[n] == 0) {
        voice_sample[n] = 0.0f;
        one_skred_frame[skred_ptr++] = 0.0f;
        one_skred_frame[skred_ptr++] = 0.0f;
        continue;
      }
      if (voice_wave_table_index[n] == WAVE_TABLE_NOISE_ALT) {
        // bypass lots of stuff if this voice uses random source...
        // reuse the one white noise source for each sample
        f = whiteish;
      } else {
        int mod = voice_freq_mod_osc[n];
        if (mod >= 0 && mod != n) {
          // try to use modulators phase_inc instead of recalculating...
          // REQUIRES re-thinking how I'm scaling frequency modulators via wire...
          // REVISIT experiments to see if this still makes sense
          float g = voice_sample[mod] * voice_freq_mod_depth[n];
          float inc = voice_phase_inc[n] + (voice_phase_inc[mod] * voice_freq_scale[n] * g);
          f = osc_next(n, inc);
        } else {
          f = osc_next(n, voice_phase_inc[n]);
        }
      }
      if (voice_sample_hold_max[n]) {
        if (voice_sample_hold_count[n] == 0) {
          voice_sample_hold[n] = f;
        }
        voice_sample[n] = voice_sample_hold[n];
        voice_sample_hold_count[n]++;
        if (voice_sample_hold_count[n] >= voice_sample_hold_max[n]) {
          voice_sample_hold_count[n] = 0;
        }
      } else {
        voice_sample[n] = f;
      }

      // apply quantizer
      if (voice_quantize[n]) voice_sample[n] = quantize_bits_int(voice_sample[n], voice_quantize[n]);

      // apply multi-mode filter
      if (voice_filter_mode[n]) voice_sample[n] = mmf_process(n, voice_sample[n]);

      // apply amp to sample
      float amp = voice_amp[n];
      float env = 1.0f;
      if (voice_use_amp_envelope[n]) env = amp_envelope_step(n) * voice_amp_envelope[n].velocity;
      float mod = 1.0f;
      if (voice_amp_mod_osc[n] >= 0) {
        int m = voice_amp_mod_osc[n];
        mod = voice_sample[m] * voice_amp_mod_depth[n];
      }
      float final = amp * env * mod;
      if (voice_smoother_enable[n]) {
        voice_smoother_gain[n] += voice_smoother_smoothing[n] * (final - voice_smoother_gain[n]);
        final = voice_smoother_gain[n];
      }
      voice_sample[n] *= final;

      if (voice_disconnect[n] == 0) {
        // accumulate samples
        if (voice_pan_mod_osc[n] >= 0) {
          // handle pan modulation
          float q = voice_sample[voice_pan_mod_osc[n]] * voice_pan_mod_depth[n];
          voice_pan_left[n]  = (1.0f - q) / 2.0f;
          voice_pan_right[n] = (1.0f + q) / 2.0f;
        }
        float left  = voice_sample[n] * voice_pan_left[n];
        float right = voice_sample[n] * voice_pan_right[n];
        sample_left  += left;
        sample_right += right;
        one_skred_frame[skred_ptr++] = left;
        one_skred_frame[skred_ptr++] = right;
      } else {
        one_skred_frame[skred_ptr++] = 0.0f;
        one_skred_frame[skred_ptr++] = 0.0f;
      }
    }

    // Adjust to main volume: smooth it otherwise is sounds crummy with realtime changes
    volume_smoother_gain += volume_smoother_smoothing * (volume_final - volume_smoother_gain);
    float volume_adjusted = volume_smoother_gain;

    sample_left  *= volume_adjusted;
    sample_right *= volume_adjusted;

    // Write to all channels
    buffer[i * num_channels + 0] = sample_left;
    buffer[i * num_channels + 1] = sample_right;
  }
  clock_gettime(BENCH_CLOCK, &bench[benchp].b);
  bench[benchp].state = BEN_B;
  bencho++;
  benchp = ((bencho) % BENLEN);
}

int envelope_is_flat(int v) {
  if (voice_amp_envelope[v].a == 0.0f &&
    voice_amp_envelope[v].d == 0.0f &&
    voice_amp_envelope[v].s == 1.0f &&
    voice_amp_envelope[v].r == 0.0f) return 1;
  return 0;
}

int cz_set(int v, int n, float f) {
  voice_cz_mode[v] = n;
  voice_cz_distortion[v] = f;
  return 0;
}

int cmod_set(int voice, int o, float f) {
  voice_cz_mod_osc[voice] = o;
  voice_cz_mod_depth[voice] = f;
  return 0;
}

#include <stdio.h>

// maybe these should be in wire.[ch]?

static int voice_invalid(int voice) {
  if (voice < 0 || voice >= VOICE_MAX) return 1;
  return 0;
}

#define SYNTH_INVALID_VOICE (100)

char *voice_format(int v, char *out, int verbose) {
  if (out == NULL) return "(NULL)";
  if (voice_invalid(v)) {
    out[0] = '\0';
    return out;
  }
  char *ptr = out;
  int n = 0;
  {
    n = sprintf(ptr, "v%d w%d f%g a%g",
      v,
      voice_wave_table_index[v],
      voice_freq[v],
      voice_user_amp[v]);
    ptr += n;
  }
  if (verbose || voice_midi_transpose[v]) {
    n = sprintf(ptr, " N%g", voice_midi_transpose[v]);
    ptr += n;
  }
  if (verbose || voice_link_midi_a[v] >= 0 || voice_link_midi_b[v] >= 0) {
    n = sprintf(ptr, " G%g,%g", voice_link_midi_a[v], voice_link_midi_b[v]);
    ptr += n;
  }
  if (verbose || voice_link_velo_a[v] >= 0 || voice_link_velo_b[v] >= 0) {
    n = sprintf(ptr, " H%g,%g", voice_link_velo_a[v], voice_link_velo_b[v]);
    ptr += n;
  }
  if (verbose || voice_link_trig[v] >= 0) {
    n = sprintf(ptr, " L%g", voice_link_trig[v]);
    ptr += n;
  }
  if (verbose || voice_direction[v]) {
    n = sprintf(ptr, " b%d", voice_direction[v]);
    ptr += n;
  }
  if (verbose || voice_loop_enabled[v]) {
    n = sprintf(ptr, " B%d",
      voice_loop_enabled[v]);
    ptr += n;
  }
  if (verbose || voice_pan[v]) {
    n = sprintf(ptr, " p%g", voice_pan[v]);
    ptr += n;
  }
  if (verbose || voice_note[v]) {
    n = sprintf(ptr, " n%g", voice_note[v]);
    ptr += n;
  }
  if (verbose || voice_filter_mode[v]) {
    n = sprintf(ptr, " J%d K%g Q%g",
      voice_filter_mode[v],
      voice_filter_freq[v],
      voice_filter_res[v]);
    ptr += n;
  }
  if (verbose || voice_cz_mode[v]) {
    n = sprintf(ptr, " c%d,%g", voice_cz_mode[v], voice_cz_distortion[v]);
    ptr += n;
  }
  if (verbose || voice_quantize[v]) {
    n = sprintf(ptr, " q%d", voice_quantize[v]);
    ptr += n;
  }
  if (verbose || voice_sample_hold_max[v]) {
    n = sprintf(ptr, " h%d", voice_sample_hold_max[v]);
    ptr += n;
  }
  if (verbose || (voice_amp_mod_osc[v] >= 0 && voice_amp_mod_depth[v] > 0)) {
    n = sprintf(ptr, " A%d,%g", voice_amp_mod_osc[v], voice_amp_mod_depth[v]);
    ptr += n;
  }
  if (verbose || (voice_cz_mod_osc[v] >= 0 && voice_cz_mod_depth[v] > 0)) {
    n = sprintf(ptr, " C%d,%g", voice_cz_mod_osc[v], voice_cz_mod_depth[v]);
    ptr += n;
  }
  if (verbose || (voice_freq_mod_osc[v] >= 0 && voice_freq_mod_depth[v] > 0)) {
    n = sprintf(ptr, " F%d,%g", voice_freq_mod_osc[v], voice_freq_mod_depth[v]);
    ptr += n;
  }
  if (verbose || (voice_pan_mod_osc[v] >= 0 && voice_pan_mod_depth[v] > 0)) {
    n = sprintf(ptr, " P%d,%g", voice_pan_mod_osc[v], voice_pan_mod_depth[v]);
    ptr += n;
  }
  if (verbose || voice_disconnect[v]) {
    n = sprintf(ptr, " m%d", voice_disconnect[v]);
    ptr += n;
  }
  if (verbose || voice_record[v]) {
    n = sprintf(ptr, " r%d", voice_record[v]);
    ptr += n;
  }
  if (verbose || voice_smoother_enable[v]) {
    if (voice_smoother_smoothing[v] != SMOOTH_DEFAULT) {
      n = sprintf(ptr, " s%g", voice_smoother_smoothing[v]);
      ptr += n;
    }
  }
  if (verbose || voice_glissando_enable[v]) {
    n = sprintf(ptr, " g%g", voice_glissando_speed[v]);
    ptr += n;
  }
  if (verbose || !envelope_is_flat(v)) {
    n = sprintf(ptr, " t%g,%g,%g,%g",
      voice_amp_envelope[v].a,
      voice_amp_envelope[v].d,
      voice_amp_envelope[v].s,
      voice_amp_envelope[v].r);
    ptr += n;
  }
  if (verbose) {
    n = sprintf(ptr, "\n#");
    ptr += n;
  }
  if (verbose) {
    n = sprintf(ptr, " freq_scale:%g", voice_freq_scale[v]);
    ptr += n;
  }
  if (verbose) {
    n = sprintf(ptr, " finished:%d one_shot:%d",
      voice_finished[v],
      voice_one_shot[v]);
    ptr += n;
  }
  if (verbose) {
    n = sprintf(ptr, " sample:%g", voice_sample[v]);
    ptr += n;
  }
  if (verbose) {
    n = sprintf(ptr, " smoother:%g", voice_smoother_gain[v]);
    ptr += n;
  }
  if (verbose) {
    n = sprintf(ptr, " phase:%g phase_inc:%g", voice_phase[v], voice_phase_inc[v]);
    ptr += n;
  }
  if (verbose) {
    n = sprintf(ptr, " offset_hz:%g", voice_offset_hz[v]);
    ptr += n;
  }
  if (verbose) {
    n = sprintf(ptr, " latency:%gms", (double)ts_diff_ns(&voice_mark_a[v], &voice_mark_b[v])/1000000.0);
    ptr += n;
  }
  return out;
}


void voice_show(int v, char c, int verbose) {
  char s[1024];
  char e[8] = "";
  if (c != ' ') sprintf(e, " # *");
  voice_format(v, s, verbose);
  if (strlen(s)) printf("; %s%s\n", s, e);
}

int voice_show_all(int voice, int verbose) {
  for (int i=0; i<VOICE_MAX; i++) {
    if (voice_amp[i] == 0) continue;
    char t = ' ';
    if (i == voice) t = '*';
    voice_show(i, t, verbose);
  }
  return 0;
}

int amp_set(int voice, float f) {
  if (f >= 0) {
    voice_use_amp_envelope[voice] = 0;
    voice_amp[voice] = f;
    voice_user_amp[voice] = f;
  } else return 100; // <--- LAZY!! ... ERR_AMPLITUDE_OUT_OF_RANGE;
  return 0;
}

int pan_set(int voice, float f) {
  if (f >= -1.0f && f <= 1.0f) {
    voice_pan[voice] = f;
    voice_pan_left[voice] = (1.0f - f) / 2.0f;
    voice_pan_right[voice] = (1.0f + f) / 2.0f;
  } else {
    return 100; // <--- LAZY! needs ERR_PAN_OUT_OF_RANGE;
  }
  return 0;
}

int wave_quant(int voice, int n) {
  voice_quantize[voice] = n;
  return 0;
}

int freq_set(int voice, float f) {
  if (f >= 0 && f < (double)MAIN_SAMPLE_RATE) {
    voice_freq[voice] = f;
    osc_set_freq(voice, f);
    return 0;
  }
  return 101; // <--- LAZY needs ERR_FREQUENCY_OUT_OF_RANGE;
}


int wave_mute(int voice, int state) {
  if (state < 0) {
    if (voice_disconnect[voice] == 0) state = 1;
    else state = 0;
  }
  voice_disconnect[voice] = state;
  return 0;
}

int wave_dir(int voice, int state) {
  if (state < 0) {
    if (voice_direction[voice] == 0) state = 1;
    else state = 0;
  }
  voice_direction[voice] = state;
  return 0;
}

int pan_mod_set(int voice, int o, float f) {
  if (voice_invalid(voice) || voice_invalid(o)) return SYNTH_INVALID_VOICE;
  voice_pan_mod_osc[voice] = o;
  voice_pan_mod_depth[voice] = f;
  return 0;
}

int wave_set(int voice, int wave) {
  if (wave >= 0 && wave < WAVE_TABLE_MAX) {
    osc_set_wave_table_index(voice, wave);
    // AUGGGHHHH... i love the scope, but this needs fixing in a better way...
    // if (scope_enable) scope_wave_update(voice_table[voice], voice_table_size[voice]);
  } else return 100; // <-- more LAZY!!! ERR_INVALID_WAVE;
  return 0;
}

int amp_mod_set(int voice, int o, float f) {
  if (voice_invalid(voice) || voice_invalid(o)) return SYNTH_INVALID_VOICE;
  voice_amp_mod_osc[voice] = o;
  voice_amp_mod_depth[voice] = f;
  return 0;
}

int freq_mod_set(int voice, int o, float f) {
  if (voice_invalid(voice) || voice_invalid(o)) return SYNTH_INVALID_VOICE;
  voice_freq_mod_osc[voice] = o;
  voice_freq_mod_depth[voice] = f;
  voice_freq_scale[voice] = (float)voice_table_size[voice] / (float)voice_table_size[o];
  return 0;
}

int wave_loop(int voice, int state) {
  if (state < 0) {
    if (voice_loop_enabled[voice] == 0) state = 1;
    else state = 0;
  }
  voice_loop_enabled[voice] = state;
  return 0;
}


int envelope_set(int voice, float a, float d, float s, float r) {
  envelope_init(voice, a, d, s, r);
  return 0;
}

// Set parameters - only recalculates coefficients if values changed
void mmf_set_params(int n, float f, float resonance) {
    // Only recalculate if parameters changed
    if (
      f == voice_filter[n].last_freq &&
      resonance == voice_filter[n].last_resonance &&
      voice_filter_mode[n] == voice_filter[n].last_mode) {
        return;  // No work needed!
    }

    voice_filter[n].last_freq = f;
    voice_filter[n].last_resonance = resonance;
    voice_filter[n].last_mode = voice_filter_mode[n];

    // Calculate filter coefficients (expensive operations only done here)
    float omega = 2.0f * (float)M_PI * f / (float)MAIN_SAMPLE_RATE;
    float sin_omega = sinf(omega);
    float cos_omega = cosf(omega);
    float alpha = sin_omega / (2.0f * resonance);

    float a0, b0, b1, b2, a1, a2;

    switch (voice_filter_mode[n]) {
      case 0:
        return;
      default:
      case FILTER_LOWPASS:
          b0 = (1.0f - cos_omega) / 2.0f;
          b1 = 1.0f - cos_omega;
          b2 = (1.0f - cos_omega) / 2.0f;
          a0 = 1.0f + alpha;
          a1 = -2.0f * cos_omega;
          a2 = 1.0f - alpha;
          break;
      case FILTER_HIGHPASS:
          b0 = (1.0f + cos_omega) / 2.0f;
          b1 = -(1.0f + cos_omega);
          b2 = (1.0f + cos_omega) / 2.0f;
          a0 = 1.0f + alpha;
          a1 = -2.0f * cos_omega;
          a2 = 1.0f - alpha;
          break;

      case FILTER_BANDPASS:
          b0 = alpha;
          b1 = 0.0f;
          b2 = -alpha;
          a0 = 1.0f + alpha;
          a1 = -2.0f * cos_omega;
          a2 = 1.0f - alpha;
          break;

      case FILTER_NOTCH:
          b0 = 1.0f;
          b1 = -2.0f * cos_omega;
          b2 = 1.0f;
          a0 = 1.0f + alpha;
          a1 = -2.0f * cos_omega;
          a2 = 1.0f - alpha;
          break;

      case FILTER_ALL_PASS:
          b0 = 1.0f - alpha;
          b1 = -2.0f * cos_omega;
          b2 = 1.0f + alpha;
          a0 = 1.0f + alpha;
          a1 = -2.0f * cos_omega;
          a2 = 1.0f - alpha;
          break;
    }

    // Normalize coefficients
    voice_filter[n].b0 = b0 / a0;
    voice_filter[n].b1 = b1 / a0;
    voice_filter[n].b2 = b2 / a0;
    voice_filter[n].a1 = a1 / a0;
    voice_filter[n].a2 = a2 / a0;

    voice_filter_freq[n] = f;
    voice_filter_res[n] = resonance;
}


// Initialize the filter with frequency and resonance
// freq: cutoff frequency in Hz
// resonance: resonance factor (0.1 to 10.0, where 0.707 is no resonance)
// sample_rate: audio sample rate in Hz
void mmf_init(int n, float f, float resonance) {
    // Clear delay lines
    voice_filter[n].x1 = voice_filter[n].x2 = 0.0f;
    voice_filter[n].y1 = voice_filter[n].y2 = 0.0f;

    // Store parameters
    voice_filter[n].last_freq = -1.0f;  // Force coefficient calculation
    voice_filter[n].last_resonance = -1.0f;
    voice_filter[n].last_mode = -1;

    voice_filter_freq[n] = f;
    voice_filter_res[n] = resonance;

    // Calculate initial coefficients
    mmf_set_params(n, f, resonance);
}


int voice_copy(int v, int n) {
  wave_set(n, voice_wave_table_index[v]);
  amp_set(n, voice_user_amp[v]);
  freq_set(n, voice_freq[v]);
  pan_set(n, voice_pan[v]);
  amp_mod_set(n, voice_amp_mod_osc[v], voice_amp_mod_depth[v]);
  freq_mod_set(n, voice_freq_mod_osc[v], voice_freq_mod_depth[v]);
  pan_mod_set(n, voice_pan_mod_osc[v], voice_pan_mod_depth[v]);
  wave_loop(n, voice_loop_enabled[v]);
  wave_dir(n, voice_direction[v]);
  wave_quant(n, voice_quantize[v]);
  voice_sample_hold_max[n] = voice_sample_hold_max[v];
  voice_sample_hold_count[n] = voice_sample_hold_count[v];
  voice_sample_hold[n] = voice_sample_hold[v];
  envelope_set(n, voice_amp_envelope[v].a, voice_amp_envelope[v].d, voice_amp_envelope[v].s, voice_amp_envelope[v].r);
  cz_set(n, voice_cz_mode[v], voice_cz_distortion[v]);
  cmod_set(n, voice_cz_mod_osc[v], voice_cz_mod_depth[v]);
  voice_filter_mode[n] = voice_filter_mode[v];
  mmf_init(n, voice_filter_freq[v], voice_filter_res[v]);
  // TODO stuff is missing from here...
  return 0;
}

float midi2hz(float f) {
  float g = 440.0f * powf(2.0f, (f - 69.0f) / 12.0f);
  return g;
}

int voice_set(int n, int *old_voice) {
  if (voice_invalid(n)) return SYNTH_INVALID_VOICE;
  if (old_voice) *old_voice = n;
  return 0;
}

int voice_trigger(int voice) {
  osc_trigger(voice);
  return 0;
}

int wave_default(int voice) {
  float g = midi2hz((float)voice_midi_note[voice]);
  voice_freq[voice] = g;
  voice_note[voice] = (float)voice_midi_note[voice];
  osc_set_freq(voice, g);
  // FIX FIX FIX scope_wave_update(voice_table[voice], voice_table_size[voice]);
  return 0;
}

int freq_midi(int voice, float f) {
  if (f >= 0.0 && f <= 127.0) {
    if (voice_midi_transpose[voice]) f += voice_midi_transpose[voice];
    float g = midi2hz(f);
    return freq_set(voice, g);
  }
  return 100; // <-- LAZY  ERR_INVALID_MIDI_NOTE;
}

void voice_reset(int i) {
  voice_wave_table_index[i] = 0;
  voice_table_rate[i] = 0;
  voice_table_size[i] = 0;
  voice_sample[i] = 0;
  voice_amp[i] = 0;
  voice_user_amp[i] = 0;
  voice_pan[i] = 0;
  voice_pan_left[i] = 0.5f;
  voice_pan_right[i] = 0.5f;
  // pan smoothing?
  voice_use_amp_envelope[i] = 0;
  voice_amp_mod_osc[i] = -1;
  voice_freq_mod_osc[i] = -1;
  voice_freq_mod_depth[i] = 0.0f;
  voice_freq_scale[i] = 1.0f;
  voice_pan_mod_osc[i] = -1;
  voice_disconnect[i] = 0;
  voice_quantize[i] = 0;
  voice_direction[i] = 0;
  envelope_init(i, 0.0f, 0.0f, 1.0f, 0.0f);
  voice_freq[i] = 440.0f;
  voice_midi_note[i] = 69.0f;
  voice_midi_transpose[i] = 0;
  voice_link_midi_a[i] = -1;
  voice_link_midi_b[i] = -1;
  voice_link_velo_a[i] = -1;
  voice_link_velo_b[i] = -1;
  voice_link_trig[i] = -1;
  osc_set_wave_table_index(i, WAVE_TABLE_SINE);
  voice_filter_mode[i] = 0;
  mmf_init(i, 8000.0f, 0.707f);
  //
  voice_smoother_enable[i] = 1;
  voice_smoother_gain[i] = 0.0f;
  voice_smoother_smoothing[i] = SMOOTH_DEFAULT;
  //
  voice_glissando_enable[i] = 0;
  voice_glissando_speed[i] = 0.0f;
  voice_glissando_target[i] = voice_freq[i];

  voice_record[i] = 0;
}

void voice_init(void) {
  for (int i=0; i<VOICE_MAX; i++) {
    voice_reset(i);
  }
}

int wave_reset(int voice, int n) {
  if (voice_invalid(n)) voice_init();
  else voice_reset(n);
  return 0;
}

int envelope_velocity(int voice, float f) {
  if (voice_invalid(voice)) return SYNTH_INVALID_VOICE;
  if (f == 0) {
    amp_envelope_release(voice);
  } else {
    voice_use_amp_envelope[voice] = 1;
    if (voice_one_shot[voice]) {
      osc_trigger(voice);
    }
    //osc_trigger(voice);
    amp_envelope_trigger(voice, f);
  }
  return 0;
}

int mmf_set_freq(int n, float f) {
  mmf_set_params(n, f, voice_filter_res[n]);
  return 0;
}

int mmf_set_res(int n, float res) {
  if (res > 0) mmf_set_params(n, voice_filter_freq[n], res);
  return 0;
}

#define SIZE_SINE (4096)
#include "retro/korg.h"
#include "amysamples.h"

void normalize_preserve_zero(float *data, int length) {
  if (length == 0) return;

  // Find the maximum absolute value
  float max_abs = 0.0f;
  for (int i = 0; i < length; i++) {
    float abs_val = fabsf(data[i]);
    if (abs_val > max_abs) {
      max_abs = abs_val;
    }
  }

  // Avoid division by zero
  if (max_abs == 0.0) {
    return;  // All values are zero, nothing to normalize
  }

  // Scale all values by the same factor
  float scale_factor = 1.0f / max_abs;
  for (int i = 0; i < length; i++) {
    data[i] *= scale_factor;
  }
}

void wave_table_init(void) {
  float *table;

  for (int i = 0 ; i < WAVE_TABLE_MAX; i++) {
    wave_table_data[i] = NULL;
    wave_size[i] = 0;
    wave_is_miniwav[i] = 0;
  }

  uint64_t white_noise;
  audio_rng_init(&white_noise, 1);
  for (int w = WAVE_TABLE_SINE; w <= WAVE_TABLE_NOISE_ALT; w++) {
    int size = SIZE_SINE;
    char *name = "?";
    switch (w) {
      case WAVE_TABLE_SINE:  name = "sine"; break;
      case WAVE_TABLE_SQR:   name = "square"; break;
      case WAVE_TABLE_SAW_DOWN: name = "saw-down"; break;
      case WAVE_TABLE_SAW_UP: name = "saw-up"; break;
      case WAVE_TABLE_TRI:   name = "triangle"; break;
      case WAVE_TABLE_NOISE: name = "noise"; break;
      case WAVE_TABLE_NOISE_ALT: name = "noise-alt"; break; // not used, here for laziness in experiment
      default: name = "?"; break;
    }
    printf("# make w%d %s\n", w, name);
    wave_table_data[w] = (float *)malloc(size * sizeof(float));
    wave_size[w] = size;
    wave_rate[w] = MAIN_SAMPLE_RATE;
    wave_one_shot[w] = 0;
    wave_loop_start[w] = 0;
    wave_loop_end[w] = size-1;
    int off = 0;
    float phase = 0;
    float delta = 1.0f / (float)size;
    while (phase < 1.0f) {
      float sine = sinf(2.0f * (float) M_PI * phase);
      float f;
      switch (w) {
        case WAVE_TABLE_SINE: f = sine; break;
        case WAVE_TABLE_SQR: f = (phase < 0.5) ? 1.0f : -1.0f; break;
        case WAVE_TABLE_SAW_DOWN: f = 2.0f * phase - 1.0f; break;
        case WAVE_TABLE_SAW_UP: f = 1.0f - 2.0f * phase; break;
        case WAVE_TABLE_TRI: f = (phase < 0.5f) ? (4.0f * phase - 1.0f) : (3.0f - 4.0f * phase); break;
        case WAVE_TABLE_NOISE: f = audio_rng_float(&white_noise); break;
        case WAVE_TABLE_NOISE_ALT: f = audio_rng_float(&white_noise); break;
        default: f = 0; break;
      }
      wave_table_data[w][off++] = f;
      phase += delta;
    }
  }

  printf("# load retro waves (%d to %d)\n", WAVE_TABLE_KRG1, WAVE_TABLE_KRG32-1);

  korg_init();

  for (int i = WAVE_TABLE_KRG1; i < WAVE_TABLE_KRG32; i++) {
    int k = i - WAVE_TABLE_KRG1;
    int s = kwave_size[k];
    table = malloc(s * sizeof(float));
    for (int j = 0 ; j < s; j++) {
      table[j] = (float)kwave[k][j] / (float)32767;
    }
    wave_table_data[i] = table;
    wave_size[i] = s;
    wave_rate[i] = MAIN_SAMPLE_RATE;
    wave_one_shot[i] = 0;
    wave_loop_start[i] = 0;
    wave_loop_end[i] = s-1;
  }

  // load AMY samples
  int j = AMY_SAMPLE_99;
  for (int i = 0; i < PCM_SAMPLES; i++) {
    j = i + AMY_SAMPLE_00;
    if (j > AMY_SAMPLE_99-1) {
      printf("# too many PCM samples... exit early\n");
      break;
    }
    table = malloc(pcm_map[i].length * sizeof(float));
    for (int k = 0; k < pcm_map[i].length; k++) {
      table[k] = (float)pcm[pcm_map[i].offset + k] / 32767.0f;
    }
    normalize_preserve_zero(table, (int)pcm_map[i].length);
    wave_table_data[j] = table;
    wave_size[j] = (int)pcm_map[i].length;
    wave_rate[j] = PCM_AMY_SAMPLE_RATE;
    wave_one_shot[j] = 1;
    wave_loop_enabled[j] = 0;
    wave_loop_start[j] = (int)pcm_map[i].loopstart;
    wave_loop_end[j] = (int)pcm_map[i].loopend;
    wave_midi_note[j] = (int)pcm_map[i].midinote;
    wave_offset_hz[j] = midi2hz((float)pcm_map[i].midinote);
  }
  printf("# load AMY samples (%d to %d)\n", AMY_SAMPLE_00, j);
}

void wave_free(void) {
  for (int i = 0; i < WAVE_TABLE_MAX; i++) {
    if (wave_table_data[i]) {
      if (wave_is_miniwav[i]) {
        mw_free(wave_table_data[i]);
      } else {
        free(wave_table_data[i]);
      }
      wave_size[i] = 0;
    }
  }
}

