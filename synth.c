#include "skred.h"

#include "synth-types.h"

#define ARRAY(type, name, size, init) type name[size] = init;
#include "synth.def"
#undef ARRAY

#include <math.h>
#include <stdint.h>

int requested_synth_frames_per_callback = SYNTH_FRAMES_PER_CALLBACK;
int synth_frames_per_callback = 0;

volatile uint64_t synth_sample_count = 0;

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
  float g = f;
  if (voice_one_shot[v]) g /= voice_offset_hz[v];
  float phase_inc = (g * (float)voice_table_size[v]) / voice_table_rate[v] * (voice_table_rate[v] / MAIN_SAMPLE_RATE);
  return phase_inc;
}

void osc_set_freq(int v, float f) {
  // Compute the frequency in "table samples per system sample"
  // This works even if table_rate ≠ system rate
  float g = f;
  if (voice_one_shot[v]) g /= voice_offset_hz[v];
  voice_phase_inc[v] = (g * (float)voice_table_size[v]) / voice_table_rate[v] * (voice_table_rate[v] / MAIN_SAMPLE_RATE);
}

float cz_phasor(int n, float p, float d, int table_size) {
  float phase = p / (float)table_size;
  switch (n) {
    case 1: // 2 :: saw -> pulse
      if (phase < d) phase *= (0.5f / d);
      else phase = 0.5f + (phase - d) * (0.5f / (1.0f - d));
      break;
    case 2: // 3 :: square (folded sine)
      if (phase < 0.5) phase *= 0.5f / (0.5f - d * 0.5f);
      else phase = 1.0f - (1.0f - phase) * (0.5f / (0.5f - d * 0.5f));
      break;
    case 3: // 4 :: triangle
      if (phase < 0.5f) phase *= (0.5f / (0.5f - d * 0.5f));
      else phase = 0.5f + (phase - 0.5f) * (0.5f / (0.5f - d * 0.5f));
      break;
    case 4: // 5 :: double sine
      phase = fmodf(phase * 2.0f, 1.0f);
      break;
    case 5: // 6 :: saw -> triangle
      if (phase < 0.5f) phase *= (0.5f / (0.5f - d * 0.5f));
      else phase = 0.5f + (phase - 0.5f) * (0.5f / (0.5f + d * 0.5f));
      break;
    case 6: // 7 :: resonant 1
      phase = powf(phase, 1.0f + 4.0f * d);
      break;
    case 7: // 8 :: resonant 2
      phase = powf(phase, 1.0f + 8.0f * d);
      break;
    default:
      return p;
  }
  return fmodf(phase * (float)table_size, (float)table_size);
  return phase * (float)table_size;
}

float osc_next(int voice, float phase_inc) {
    if (voice_finished[voice]) return 0.0f;
    if (voice_direction[voice]) phase_inc *= -1.0f;

    // Step phase
    voice_phase[voice] += phase_inc;

    // NaN check
    if (!isfinite(voice_phase[voice])) {
      voice_phase[voice] = 0.0f;
      voice_finished[voice] = voice_one_shot[voice];
      return 0.0f;
    }

    int table_size = voice_table_size[voice];

    // Clamp loop boundaries to valid range
    float loop_start = voice_loop_enabled[voice] ? (float)voice_loop_start[voice] : 0.0f;
    float loop_end   = voice_loop_enabled[voice] ? (float)voice_loop_end[voice]   : (float)table_size;
    if (loop_end <= loop_start) {
      loop_start = 0.0f;
      loop_end = (float)table_size;
    }

    // Handle forward playback
    if (phase_inc >= 0.0f) {
        // One-shot forward
        if (voice_one_shot[voice] && voice_phase[voice] >= loop_end) {
          if (voice_loop_enabled[voice]) {
            voice_phase[voice] = voice_loop_start[voice];
            // puts("##3a");
          } else {
            voice_phase[voice] = loop_end - 1e-6f;
            voice_finished[voice] = 1;
            // puts("##3b"); // make work for loop points
          }
        }
        // Loop forward
        else if (voice_loop_enabled[voice] && voice_phase[voice] >= loop_end) {
            voice_phase[voice] = loop_start + fmodf(voice_phase[voice] - loop_start, loop_end - loop_start);
            //puts("##4");
        }
        // Wrap full table
        else if (!voice_loop_enabled[voice] && voice_phase[voice] >= (float)table_size) {
            voice_phase[voice] -= (float)table_size;
        }
    }
    // Handle backward playback
    else {
        // One-shot backward
        if (voice_one_shot[voice] && voice_phase[voice] < loop_start) {
          if (voice_loop_enabled[voice]) {
            voice_phase[voice] = loop_end;
            //puts("##5a");
          } else {
            voice_phase[voice] = loop_start;
            voice_finished[voice] = 1;
            //puts("##5b"); // make work for loop points
          }
        }
        // Loop backward
        else if (voice_loop_enabled[voice] && voice_phase[voice] < loop_start) {
            voice_phase[voice] = loop_end - fmodf(loop_start - voice_phase[voice], loop_end - loop_start);
            //puts("##6");
        }
        // Wrap full table
        else if (!voice_loop_enabled[voice] && voice_phase[voice] < 0.0f) {
            voice_phase[voice] += (float)voice_table_size[voice];
        }
    }

    int idx;
    if (voice_cz_mode[voice]) {
      int dv = voice_cz_mod_osc[voice];
      float dm = 1.0f;
      if (dv >= 0) dm = voice_sample[dv] * voice_cz_mod_depth[voice];
      idx = (int)cz_phasor(voice_cz_mode[voice], voice_phase[voice], voice_cz_distortion[voice] + dm, table_size);
      //idx = (int)cz_phasor(cz[voice], voice_phase[voice], czd[voice], table_size);
    } else {
      idx = (int)voice_phase[voice];
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
    // voice_phase[voice] = 0; // need to decide how to sync/reset phase???
    if (update_freq) {
      osc_set_freq(voice, voice_freq[voice]);
    }
  }
}

void osc_trigger(int voice) {
  if (voice_one_shot[voice]) {
    if (voice_direction[voice]) {
      voice_phase[voice] = (float)(voice_table_size[voice] - 1);
    } else {
      voice_phase[voice] = 0;
    }
    voice_finished[voice] = 0;
  } else {
    voice_phase[voice] = 0;
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

void synth(float *buffer, float *input, int num_frames, int num_channels) {
  static uint64_t synth_random;
  static int first = 1;
  if (first) {
    synth_frames_per_callback = num_frames;
    audio_rng_init(&synth_random, 1);
    first = 0;
  }
  for (int i = 0; i < num_frames; i++) {
    synth_sample_count++;
    float sample_left = 0.0f;
    float sample_right = 0.0f;
    float f = 0.0f;
    float whiteish = audio_rng_float(&synth_random);
    for (int n = 0; n < VOICE_MAX; n++) {
      if (voice_finished[n]) {
        voice_sample[n] = 0.0f;
        continue;
      }  
      if (voice_amp[n] == 0) {
        voice_sample[n] = 0.0f;
        continue;
      }
      if (voice_wave_table_index[n] == WAVE_TABLE_NOISE_ALT) {
        // bypass lots of stuff if this voice uses random source...
        // reuse the one white noise source for each sample
        f = whiteish;
      } else {
        if (voice_freq_mod_osc[n] >= 0) {
          // try to use modulators phase_inc instead of recalculating...
          // REQUIRES re-thinking how I'm scaling frequency modulators via wire...
          // REVISIT experiments to see if this still makes sense
          int mod = voice_freq_mod_osc[n];
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
#if 0
    if (scope_enable) { // make writing to scope optional
      new_scope->buffer_left[new_scope->buffer_pointer]  = sample_left;
      new_scope->buffer_right[new_scope->buffer_pointer] = sample_right;
      new_scope->buffer_pointer++;
      new_scope->buffer_pointer %= new_scope->buffer_len;
      sprintf(new_scope->status_text, "%g %g %g %g", volume_user, volume_final, volume_smoother_gain, volume_smoother_smoothing);
    }
#endif
  }
}
