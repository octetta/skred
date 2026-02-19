/*
 * synth-alloc.c — the ONLY place that calls calloc/free on synth state.
 *
 * Rules:
 *   - synth_alloc_voices() and synth_alloc_waves() are called once,
 *     from synth_init() only.
 *   - synth_free_voices() and synth_free_waves() are called once,
 *     from synth_free() only.
 *   - The audio callback must NEVER call any function in this file.
 *   - free(NULL) is defined safe by C99+; VFREE/WFREE rely on this so
 *     partial frees after a failed init are always harmless.
 *
 * Memory is 64-byte aligned (cache-line and AVX-512 safe).
 * Voice counts are rounded up to VOICE_ALIGN before allocation;
 * extra voices are zeroed and produce silence.
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

#include "synth-features.h"
#include "synth-config.h"
#include "synth-state.h"

/* ------------------------------------------------------------------ */
/* Globals defined here                                                */
/* ------------------------------------------------------------------ */

synth_voices_t sv;
synth_waves_t  sw;
synth_config_t synth_config;

/* ------------------------------------------------------------------ */
/* Internal allocator                                                  */
/* ------------------------------------------------------------------ */

static void *synth_alloc(int n, size_t sz, const char *name) {
    if (n <= 0 || sz == 0) {
        fprintf(stderr, "synth_alloc: zero-size request for '%s' (n=%d) — "
                "did you call synth_config_defaults() before synth_init()?\n", name, n);
        exit(1);
    }

    /* calloc zeroes memory and is safe with any allocator.
     * aligned_alloc was causing heap corruption on glibc when interleaved
     * with large allocations (the recording buffer). The 64-byte alignment
     * was an optimisation hint only — calloc is correct and sufficient. */
    void *p = calloc((size_t)n, sz);
    if (!p) {
        fprintf(stderr, "synth_alloc: out of memory allocating '%s' "
                "(%d x %zu = %zu bytes)\n", name, n, sz, (size_t)n * sz);
        exit(1);
    }
    return p;
}

/* Allocate one parallel array into a struct field */
#define VALLOC(field, type) \
    sv.field = synth_alloc(v, sizeof(type), #field)

#define WALLOC(field, type) \
    sw.field = synth_alloc(w, sizeof(type), #field)

/* Free and NULL one field — free(NULL) is safe so partial frees are fine */
#define VFREE(field) do { free(sv.field); sv.field = NULL; } while (0)
#define WFREE(field) do { free(sw.field); sw.field = NULL; } while (0)

/* ------------------------------------------------------------------ */
/* synth_config                                                        */
/* ------------------------------------------------------------------ */

void synth_config_defaults(void) {
    synth_config.voice_max      = VOICE_ALIGN_UP(VOICE_MAX_DEFAULT);
    synth_config.wave_table_max = WAVE_TABLE_MAX_DEFAULT;
}

void synth_config_set_voices(int n) {
    if (n < 1)                    n = 1;
    if (n > VOICE_MAX_HARD_LIMIT) n = VOICE_MAX_HARD_LIMIT;
    int rounded = VOICE_ALIGN_UP(n);
    if (rounded != n)
        fprintf(stderr, "# voices rounded %d -> %d (VOICE_ALIGN=%d)\n",
                n, rounded, VOICE_ALIGN);
    synth_config.voice_max = rounded;
    printf("# %d\n", synth_config.voice_max);
}

void synth_config_set_waves(int n) {
    if (n < 1)                         n = 1;
    if (n > WAVE_TABLE_MAX_HARD_LIMIT) n = WAVE_TABLE_MAX_HARD_LIMIT;
    synth_config.wave_table_max = n;
}

/* ------------------------------------------------------------------ */
/* Voice allocation                                                    */
/* ------------------------------------------------------------------ */

void synth_alloc_voices(int voice_max) {
    int v = voice_max;   /* already VOICE_ALIGN-rounded by synth_config_set_voices */

    /* oscillator / playback core */
    VALLOC(phase,            float);
    VALLOC(phase_inc,        float);
    VALLOC(table,            float *);
    VALLOC(table_size,       int);
    VALLOC(table_rate,       float);
    VALLOC(one_shot,         int);
    VALLOC(finished,         int);
    VALLOC(direction,        int);
    VALLOC(loop_enabled,     int);
    VALLOC(loop_start,       int);
    VALLOC(loop_end,         int);
    VALLOC(loop_start_f,     float);
    VALLOC(loop_end_f,       float);
    VALLOC(loop_valid,       int);
    VALLOC(loop_length,      int);

    /* amplitude / pan */
    VALLOC(amp,              float);
    VALLOC(user_amp,         float);
    VALLOC(pan,              float);
    VALLOC(pan_left,         float);
    VALLOC(pan_right,        float);

    /* per-voice output sample */
    VALLOC(sample,           float);

    /* pitch / tuning */
    VALLOC(freq,             float);
    VALLOC(note,             float);
    VALLOC(midi_note,        float);
    VALLOC(last_midi_note,   float);
    VALLOC(midi_transpose,   float);
    VALLOC(midi_cents,       float);
    VALLOC(offset_hz,        float);
    VALLOC(freq_scale,       float);
    VALLOC(link_midi_a,      float);
    VALLOC(link_midi_b,      float);
    VALLOC(link_velo_a,      float);
    VALLOC(link_velo_b,      float);
    VALLOC(link_trig,        float);

    /* config flags */
    VALLOC(wave_table_index, int);
    VALLOC(disconnect,       int);
    VALLOC(record,           int);
    VALLOC(interpolate,      int);
    VALLOC(phase_reset,      int);

    /* timing marks */
    VALLOC(mark_go,          int);
    VALLOC(mark_a,           struct timespec);
    VALLOC(mark_b,           struct timespec);

    /* optional features */

#ifdef SYNTH_FEATURE_QUANTIZE
    VALLOC(quantize,         int);
#endif

#ifdef SYNTH_FEATURE_PHASE_DISTORTION
    VALLOC(cz_mode,          int);
    VALLOC(cz_distortion,    float);
    VALLOC(cz_mod_osc,       int);
    VALLOC(cz_mod_depth,     float);
#endif

#ifdef SYNTH_FEATURE_FILTER
    VALLOC(filter_freq,      float);
    VALLOC(filter_res,       float);
    VALLOC(filter_mode,      int);
    VALLOC(filter,           mmf_t);
#endif

#ifdef SYNTH_FEATURE_AMP_ENVELOPE
    VALLOC(amp_envelope,      envelope_t);
    VALLOC(amp_envelope_mode, int);
    VALLOC(use_amp_envelope,  int);
#endif

#ifdef SYNTH_FEATURE_GLISSANDO
    VALLOC(glissando_enable,  int);
    VALLOC(glissando_speed,   float);
    VALLOC(glissando_target,  float);
#endif

#ifdef SYNTH_FEATURE_SMOOTHER
    VALLOC(smoother_enable,    int);
    VALLOC(smoother_gain,      float);
    VALLOC(smoother_smoothing, float);
#endif

#ifdef SYNTH_FEATURE_SAMPLE_HOLD
    VALLOC(sample_hold,        float);
    VALLOC(sample_hold_count,  int);
    VALLOC(sample_hold_max,    int);
#endif

#ifdef SYNTH_FEATURE_MODULATION
    VALLOC(freq_mod_osc,    int);
    VALLOC(freq_mod_depth,  float);
    VALLOC(pan_mod_osc,     int);
    VALLOC(pan_mod_depth,   float);
    VALLOC(amp_mod_osc,     int);
    VALLOC(amp_mod_depth,   float);
#endif
}

void synth_free_voices(void) {
    VFREE(phase);            VFREE(phase_inc);       VFREE(table);
    VFREE(table_size);       VFREE(table_rate);      VFREE(one_shot);
    VFREE(finished);         VFREE(direction);       VFREE(loop_enabled);
    VFREE(loop_start);       VFREE(loop_end);        VFREE(loop_start_f);
    VFREE(loop_end_f);       VFREE(loop_valid);      VFREE(loop_length);
    VFREE(amp);              VFREE(user_amp);        VFREE(pan);
    VFREE(pan_left);         VFREE(pan_right);
    VFREE(sample);
    VFREE(freq);             VFREE(note);            VFREE(midi_note);
    VFREE(last_midi_note);   VFREE(midi_transpose);  VFREE(midi_cents);
    VFREE(offset_hz);        VFREE(freq_scale);
    VFREE(link_midi_a);      VFREE(link_midi_b);
    VFREE(link_velo_a);      VFREE(link_velo_b);     VFREE(link_trig);
    VFREE(wave_table_index); VFREE(disconnect);      VFREE(record);
    VFREE(interpolate);      VFREE(phase_reset);
    VFREE(mark_go);          VFREE(mark_a);          VFREE(mark_b);

#ifdef SYNTH_FEATURE_QUANTIZE
    VFREE(quantize);
#endif
#ifdef SYNTH_FEATURE_PHASE_DISTORTION
    VFREE(cz_mode);          VFREE(cz_distortion);
    VFREE(cz_mod_osc);       VFREE(cz_mod_depth);
#endif
#ifdef SYNTH_FEATURE_FILTER
    VFREE(filter_freq);      VFREE(filter_res);
    VFREE(filter_mode);      VFREE(filter);
#endif
#ifdef SYNTH_FEATURE_AMP_ENVELOPE
    VFREE(amp_envelope);     VFREE(amp_envelope_mode); VFREE(use_amp_envelope);
#endif
#ifdef SYNTH_FEATURE_GLISSANDO
    VFREE(glissando_enable); VFREE(glissando_speed); VFREE(glissando_target);
#endif
#ifdef SYNTH_FEATURE_SMOOTHER
    VFREE(smoother_enable);  VFREE(smoother_gain);   VFREE(smoother_smoothing);
#endif
#ifdef SYNTH_FEATURE_SAMPLE_HOLD
    VFREE(sample_hold);      VFREE(sample_hold_count); VFREE(sample_hold_max);
#endif
#ifdef SYNTH_FEATURE_MODULATION
    VFREE(freq_mod_osc);     VFREE(freq_mod_depth);
    VFREE(pan_mod_osc);      VFREE(pan_mod_depth);
    VFREE(amp_mod_osc);      VFREE(amp_mod_depth);
#endif
}

/* ------------------------------------------------------------------ */
/* Wave table allocation                                               */
/* ------------------------------------------------------------------ */

void synth_alloc_waves(int wave_table_max) {
    int w = wave_table_max;

    WALLOC(data,         float *);
    WALLOC(size,         int);
    WALLOC(rate,         float);
    WALLOC(one_shot,     int);
    WALLOC(loop_enabled, int);
    WALLOC(loop_start,   int);
    WALLOC(loop_end,     int);
    WALLOC(midi_note,    float);
    WALLOC(offset_hz,    float);
    WALLOC(direction,    float);
    WALLOC(is_miniwav,   int);
    WALLOC(refcount,     int);
    WALLOC(readonly,     int);
    WALLOC(name,         wave_name_t);
}

void synth_free_waves(void) {
    WFREE(data);         WFREE(size);         WFREE(rate);
    WFREE(one_shot);     WFREE(loop_enabled); WFREE(loop_start);
    WFREE(loop_end);     WFREE(midi_note);    WFREE(offset_hz);
    WFREE(direction);    WFREE(is_miniwav);   WFREE(refcount);
    WFREE(readonly);     WFREE(name);
}
