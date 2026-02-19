#ifndef SYNTH_STATE_H
#define SYNTH_STATE_H

/*
 * synth-state.h — heap-allocated parallel arrays for voice and wave state.
 *
 * All pointers are NULL until synth_alloc_voices() / synth_alloc_waves()
 * are called from synth_init().  They are set back to NULL by
 * synth_free_voices() / synth_free_waves() called from synth_free().
 *
 * Layout is struct-of-arrays (SoA).  This is intentional:
 *   - Contiguous same-type data lets the auto-vectorizer load full SIMD
 *     registers across voices (e.g. 4 or 8 floats at once).
 *   - Array-of-structs (AoS) would interleave fields and defeat this.
 *
 * Access:  sv.phase[v],  sw.data[w]
 * This is a direct drop-in for the old globals voice_phase[v], wave_table_data[w].
 *
 * The audio callback must NEVER call malloc/free.  These pointers are
 * populated once at init time and then treated as plain C arrays for the
 * lifetime of the engine.
 *
 * Voice counts are always rounded to a multiple of VOICE_ALIGN (see
 * synth-config.h).  Extra voices beyond the user-requested count are
 * zeroed by calloc and produce silence without any special-casing here.
 */

#include "synth-features.h"
#include "synth-config.h"
#include "synth-types.h"   /* mmf_t, envelope_t, wave_name_t */

#include <time.h>          /* struct timespec (voice_mark_a/b) */

/* ------------------------------------------------------------------ */
/* Voice state                                                         */
/* ------------------------------------------------------------------ */

typedef struct {

    /* --- oscillator / playback core (hot path, vectorize across voices) --- */
    float  * restrict phase;
    float  * restrict phase_inc;
    float ** restrict table;        /* per-voice pointer to wave data */
    int    * restrict table_size;
    float  * restrict table_rate;
    int    * restrict one_shot;
    int    * restrict finished;
    int    * restrict direction;
    int    * restrict loop_enabled;
    int    * restrict loop_start;
    int    * restrict loop_end;
    float  * restrict loop_start_f;
    float  * restrict loop_end_f;
    int    * restrict loop_valid;
    int    * restrict loop_length;

    /* --- amplitude / pan (hot path) --- */
    float  * restrict amp;
    float  * restrict user_amp;
    float  * restrict pan;
    float  * restrict pan_left;
    float  * restrict pan_right;

    /* --- per-voice output sample --- */
    float  * restrict sample;

    /* --- pitch / tuning --- */
    float  * restrict freq;
    float  * restrict note;
    float  * restrict midi_note;
    float  * restrict last_midi_note;
    float  * restrict midi_transpose;
    float  * restrict midi_cents;
    float  * restrict offset_hz;
    float  * restrict freq_scale;
    float  * restrict link_midi_a;
    float  * restrict link_midi_b;
    float  * restrict link_velo_a;
    float  * restrict link_velo_b;
    float  * restrict link_trig;

    /* --- config flags --- */
    int    * restrict wave_table_index;
    int    * restrict disconnect;
    int    * restrict record;
    int    * restrict interpolate;
    int    * restrict phase_reset;

    /* --- timing marks --- */
    int             * restrict mark_go;
    struct timespec * restrict mark_a;
    struct timespec * restrict mark_b;

    /* ----------------------------------------------------------------
     * Optional features — gated by synth-features.h
     * Disabled fields are absent from the struct entirely; any code
     * referencing them must be wrapped in the matching #ifdef.
     * ---------------------------------------------------------------- */

#ifdef SYNTH_FEATURE_QUANTIZE
    int    * restrict quantize;
#endif

#ifdef SYNTH_FEATURE_PHASE_DISTORTION
    int    * restrict cz_mode;
    float  * restrict cz_distortion;
    int    * restrict cz_mod_osc;
    float  * restrict cz_mod_depth;
#endif

#ifdef SYNTH_FEATURE_FILTER
    float  * restrict filter_freq;
    float  * restrict filter_res;
    int    * restrict filter_mode;
    mmf_t  * restrict filter;
#endif

#ifdef SYNTH_FEATURE_AMP_ENVELOPE
    envelope_t * restrict amp_envelope;
    int        * restrict amp_envelope_mode;
    int        * restrict use_amp_envelope;
#endif

#ifdef SYNTH_FEATURE_GLISSANDO
    int    * restrict glissando_enable;
    float  * restrict glissando_speed;
    float  * restrict glissando_target;
#endif

#ifdef SYNTH_FEATURE_SMOOTHER
    int    * restrict smoother_enable;
    float  * restrict smoother_gain;
    float  * restrict smoother_smoothing;
#endif

#ifdef SYNTH_FEATURE_SAMPLE_HOLD
    float  * restrict sample_hold;
    int    * restrict sample_hold_count;
    int    * restrict sample_hold_max;
#endif

#ifdef SYNTH_FEATURE_MODULATION
    int    * restrict freq_mod_osc;
    float  * restrict freq_mod_depth;
    float  * restrict freq_mod_adder;
    int    * restrict pan_mod_osc;
    float  * restrict pan_mod_depth;
    float  * restrict pan_mod_adder;
    int    * restrict amp_mod_osc;
    float  * restrict amp_mod_depth;
    float  * restrict amp_mod_adder;
#endif

} synth_voices_t;

/* ------------------------------------------------------------------ */
/* Wave table state                                                    */
/* ------------------------------------------------------------------ */

typedef struct {

    float      ** restrict data;
    int         * restrict size;
    float       * restrict rate;
    int         * restrict one_shot;
    int         * restrict loop_enabled;
    int         * restrict loop_start;
    int         * restrict loop_end;
    float       * restrict midi_note;
    float       * restrict offset_hz;
    float       * restrict direction;
    int         * restrict is_miniwav;
    int         * restrict refcount;
    int         * restrict readonly;
    wave_name_t * restrict name;

} synth_waves_t;

/* ------------------------------------------------------------------ */
/* Globals — defined in synth-alloc.c                                 */
/* ------------------------------------------------------------------ */

extern synth_voices_t sv;
extern synth_waves_t  sw;

#endif /* SYNTH_STATE_H */
