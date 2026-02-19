#ifndef SYNTH_CONFIG_H
#define SYNTH_CONFIG_H

#include "synth-types.h"   /* VOICE_MAX, WAVE_TABLE_MAX */

/*
 * synth-config.h — runtime sizing parameters
 *
 * VOICE_MAX_DEFAULT and WAVE_TABLE_MAX_DEFAULT are the fallback values
 * used when the user does not pass -v / -w on the command line.
 *
 * VOICE_MAX_HARD_LIMIT and WAVE_TABLE_MAX_HARD_LIMIT are compile-time
 * sanity caps that argv parsing will clamp to.  They do not affect
 * memory use directly — only the runtime values do.
 *
 * VOICE_ALIGN controls SIMD-friendliness.  Voice counts are always
 * rounded up to the next multiple of VOICE_ALIGN by synth_config_set_voices()
 * and synth_config_defaults().  Extra voices beyond what the user requested
 * are zeroed by calloc and produce silence — no special-casing needed in
 * the audio callback.
 *
 * VOICE_ALIGN 4  = safe minimum (SSE, 128-bit: 4 floats per register)
 * VOICE_ALIGN 8  = AVX (256-bit)
 * VOICE_ALIGN 16 = AVX-512 (512-bit)
 *
 * Keeping it at 4 with -march=native is usually enough: the compiler
 * will combine multiple 4-wide iterations into one 8-wide AVX instruction
 * automatically once it knows the count has no remainder.
 */

#define VOICE_ALIGN              4
#define VOICE_ALIGN_UP(n)        (((n) + VOICE_ALIGN - 1) & ~(VOICE_ALIGN - 1))

/* Defaults match the compile-time constants in synth-types.h.
 * WAVE_TABLE_MAX_DEFAULT must equal the WAVE_TABLE_MAX enum value (1299)
 * because wave_table_init() loops up to WAVE_TABLE_MAX unconditionally.
 * VOICE_MAX_DEFAULT matches the original VOICE_MAX compile-time constant. */
#define VOICE_MAX_DEFAULT        VOICE_MAX
#define WAVE_TABLE_MAX_DEFAULT   WAVE_TABLE_MAX

#define VOICE_MAX_HARD_LIMIT     256
#define WAVE_TABLE_MAX_HARD_LIMIT WAVE_TABLE_MAX

typedef struct {
    int voice_max;       /* always a multiple of VOICE_ALIGN */
    int wave_table_max;
} synth_config_t;

extern synth_config_t synth_config;

/*
 * Call synth_config_defaults() before synth_init().
 * synth_config_set_voices() / synth_config_set_waves() may be called
 * from argv parsing after synth_config_defaults() and before synth_init().
 * Must NOT be called after synth_init() without a matching synth_free() first.
 */
void synth_config_defaults(void);
void synth_config_set_voices(int n);
void synth_config_set_waves(int n);

/*
 * Use this everywhere you need the voice count inside the audio callback.
 * Loads from a local, applies an alignment hint so the compiler can emit
 * unconditional vector code without a scalar remainder loop.
 */
static inline int synth_voice_count(void) {
    int n = synth_config.voice_max;
    /* Alignment hint — cost: zero at runtime on already-aligned data. */
#if defined(__clang__)
    __builtin_assume(n % VOICE_ALIGN == 0);
#elif defined(__GNUC__) && __GNUC__ >= 13
    __attribute__((assume(n % VOICE_ALIGN == 0)));
#else
    /* Mask forces low bits to zero; compiler infers no remainder loop needed. */
    n = n & ~(VOICE_ALIGN - 1);
#endif
    return n;
}

#endif /* SYNTH_CONFIG_H */
