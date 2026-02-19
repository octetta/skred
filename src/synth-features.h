#ifndef SYNTH_FEATURES_H
#define SYNTH_FEATURES_H

/*
 * synth-features.h — compile-time feature switches
 *
 * Comment out a define here, or pass -USYNTH_FEATURE_XXX in CFLAGS,
 * to strip that feature from the build entirely.
 * The makefile takes precedence if you define/undef there.
 *
 * Example minimal build (Makefile):
 *   CFLAGS += -USYNTH_FEATURE_FILTER
 *   CFLAGS += -USYNTH_FEATURE_PHASE_DISTORTION
 *   CFLAGS += -USYNTH_FEATURE_QUANTIZE
 */

#ifndef SYNTH_FEATURE_PHASE_DISTORTION
#define SYNTH_FEATURE_PHASE_DISTORTION
#endif

#ifndef SYNTH_FEATURE_FILTER
#define SYNTH_FEATURE_FILTER
#endif

#ifndef SYNTH_FEATURE_AMP_ENVELOPE
#define SYNTH_FEATURE_AMP_ENVELOPE
#endif

#ifndef SYNTH_FEATURE_GLISSANDO
#define SYNTH_FEATURE_GLISSANDO
#endif

#ifndef SYNTH_FEATURE_SMOOTHER
#define SYNTH_FEATURE_SMOOTHER
#endif

#ifndef SYNTH_FEATURE_SAMPLE_HOLD
#define SYNTH_FEATURE_SAMPLE_HOLD
#endif

#ifndef SYNTH_FEATURE_MODULATION
#define SYNTH_FEATURE_MODULATION
#endif

#ifndef SYNTH_FEATURE_QUANTIZE
#define SYNTH_FEATURE_QUANTIZE
#endif

#ifndef SYNTH_FEATURE_RECORDING
#define SYNTH_FEATURE_RECORDING
#endif

#ifndef SYNTH_FEATURE_SCOPE
#define SYNTH_FEATURE_SCOPE
#endif

#endif /* SYNTH_FEATURES_H */
