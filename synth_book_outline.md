# Digital Synthesizer Engine in C - Book Outline

## Part I: Architecture & Design Philosophy

### Chapter 1: System Architecture Overview
**Goal:** Give readers the 10,000-foot view before diving into details

1. **The Big Picture**
   - Real-time audio constraints (callback-driven architecture)
   - Why C? (performance, control, portability)
   - The voice architecture (32 polyphonic voices)
   - Memory management strategy (pre-allocated vs. dynamic)

2. **Data Flow Diagram**
   - ASCII protocol → Parser → Voice control
   - Voice → Oscillator → Filter → Envelope → Mixer → Output
   - Modulation routing (FM, AM, pan modulation)

3. **Design Decisions**
   - Array-based state vs. object-oriented approaches
   - The `.def` file trick for managing parallel arrays
   - Trade-offs: simplicity vs. flexibility

4. **Code Organization Tour**
   ```
   synth.h       - Public API
   synth.c       - Implementation
   synth.def     - Array definitions
   synth-types.h - Struct definitions
   skred.h       - Global constants
   ```

### Chapter 2: Memory Management & Global State
**Goal:** Explain the unconventional but practical approach

1. **The Array Architecture**
   - Why parallel arrays instead of structs-of-voices?
   - Cache efficiency considerations
   - The `ARRAY()` macro pattern
   ```c
   ARRAY(float, voice_freq, VOICE_MAX, {})
   ARRAY(float, voice_amp, VOICE_MAX, {})
   ```

2. **Compile-Time vs. Run-Time Allocation**
   - `USE_PRE` flag: static vs. `malloc()`
   - Memory footprint analysis
   - Embedded system considerations

3. **The `.def` File Pattern**
   - Single source of truth for all arrays
   - How it enables three different uses:
     - Declaration (`extern type name[]`)
     - Definition (`type name[size] = init`)
     - Length tracking (`int name__len__ = size`)
   - When to use this pattern (and when not to)

## Part II: Core Audio Components

### Chapter 3: Oscillators - The Heart of Synthesis
**Goal:** From theory to implementation

1. **Wavetable Synthesis Fundamentals**
   - Why wavetables instead of real-time calculation?
   - Sample rate independence
   - Phase accumulator concept
   ```
   phase += (frequency × table_size) / sample_rate
   index = (int)phase
   output = table[index]
   ```

2. **Implementing `osc_next()`**
   - Phase increment calculation
   - Wrapping and looping logic
   - Direction control (forward/backward playback)
   - One-shot vs. looping samples

3. **Loop Points**
   - Start/end boundaries
   - Sustain loops for sampled instruments
   - The precomputed float optimization
   ```c
   voice_loop_start_f[v] = (float)start;
   voice_loop_end_f[v] = (float)end;
   voice_loop_length[v] = (float)(end - start);
   ```

4. **Handling Edge Cases**
   - Phase overflow protection
   - NaN/Infinity checks (`isfinite()`)
   - Boundary clamping
   - Why edge cases matter in real-time audio

### Chapter 4: Casio CZ Phase Distortion
**Goal:** Vintage synthesis technique, modern implementation

1. **What is Phase Distortion?**
   - Casio CZ series history (brief)
   - How it differs from FM synthesis
   - The concept: warp the phase, not the wave
   
2. **The Seven CZ Modes**
   - Saw → Pulse
   - Square (folded sine)
   - Triangle
   - Double sine
   - Saw → Triangle
   - Resonant modes 1 & 2

3. **Implementation: `cz_phasor()`**
   - Phase remapping mathematics
   - The `fast_pow()` approximation
   ```c
   // Fast power using bit manipulation
   union { float f; int i; } u = { a };
   u.i = (int)(b * (u.i - 1065353216) + 1065353216);
   return u.f;
   ```
   - Performance vs. accuracy trade-offs

4. **Modulation Depth Control**
   - Using one oscillator to control another's distortion
   - Creating evolving timbres

### Chapter 5: Multi-Mode Biquad Filter
**Goal:** DSP theory meets practical implementation

1. **Why Digital Filters?**
   - Subtractive synthesis basics
   - Frequency domain manipulation
   - Classic analog filter types

2. **The Biquad Filter Structure**
   - Direct Form II topology
   - Why it's efficient (5 multiplies, 4 adds)
   - State variables (delay lines)
   ```c
   output = b0*input + b1*x1 + b2*x2 - a1*y1 - a2*y2
   ```

3. **Filter Types Implementation**
   - Lowpass (remove highs)
   - Highpass (remove lows)
   - Bandpass (keep middle)
   - Notch (remove middle)
   - Allpass (phase only)

4. **Coefficient Calculation**
   - RBJ Audio EQ Cookbook formulas
   - Frequency and resonance parameters
   - Caching: only recalculate when parameters change
   ```c
   if (freq == last_freq && res == last_res) return;
   ```

5. **Preventing Clicks and Pops**
   - Smooth parameter changes
   - Avoiding discontinuities

### Chapter 6: ADSR Envelope Generator
**Goal:** Shape amplitude over time

1. **The ADSR Model**
   - Attack, Decay, Sustain, Release
   - Why this model? (origins in analog synths)
   - Typical use cases

2. **Time-Based Implementation**
   - Converting seconds to samples
   ```c
   attack_samples = attack_seconds × SAMPLE_RATE
   ```
   - Sample-accurate timing
   - Using global sample counter

3. **State Machine Design**
   - Four phases + idle state
   - Trigger (note on) vs. Release (note off)
   - Linear ramps (vs. exponential alternatives)

4. **Velocity Sensitivity**
   - MIDI velocity scaling
   - Envelope × velocity
   - Why this matters for expressive playing

5. **Edge Cases**
   - Retriggering during attack
   - Releasing during decay
   - Zero-length stages

## Part III: Signal Flow & Modulation

### Chapter 7: The Voice Processing Chain
**Goal:** Follow a single sample through the system

1. **Per-Voice Processing Order**
   ```
   Oscillator → Sample & Hold → Quantizer → 
   Filter → Envelope → Amp Modulation → 
   Pan → Mixer
   ```

2. **Sample & Hold**
   - Creating stepped/quantized modulation
   - Use cases: retro video game sounds

3. **Bit Quantization**
   - Lo-fi effect simulation
   - Bit depth reduction mathematics
   ```c
   levels = (1 << bits) - 1
   quantized = round(value × levels) / levels
   ```

4. **Amplitude Smoothing**
   - Why sudden changes cause clicks
   - One-pole lowpass filter
   ```c
   smoothed += α × (target - smoothed)
   ```
   - Choosing smoothing coefficients

### Chapter 8: Modulation Routing
**Goal:** How oscillators talk to each other

1. **The Modulation Matrix**
   - Voice as modulator, voice as carrier
   - Modulation depth control
   - Why indices instead of pointers?

2. **Frequency Modulation (FM)**
   - Classic FM synthesis
   - Phase increment modulation
   - Ratio scaling between carrier/modulator
   ```c
   scale = carrier_table_size / modulator_table_size
   ```

3. **Amplitude Modulation (AM)**
   - Ring modulation effect
   - Tremolo
   - Creating complex timbres

4. **Pan Modulation**
   - Auto-panning effects
   - Stereo width control
   - Real-time pan calculation
   ```c
   pan_left = (1.0 - mod) / 2.0
   pan_right = (1.0 + mod) / 2.0
   ```

5. **Avoiding Feedback Loops**
   - Processing order matters
   - Preventing infinite recursion
   - One-frame delay implicit in design

### Chapter 9: The Main Audio Callback
**Goal:** Real-time constraints and optimization

1. **Real-Time Audio Fundamentals**
   - Callback-driven architecture
   - Frame size and latency
   - Never block, never allocate

2. **Walking Through `synth()`**
   ```c
   for each frame:
       for each voice:
           generate_sample()
           apply_effects()
           mix_to_output()
       apply_master_volume()
       write_to_buffer()
   ```

3. **Optimization Techniques**
   - Early exit for silent voices
   - Shared white noise generator
   - Loop unrolling considerations
   - Why simpler can be faster

4. **Stereo Mixing**
   - Accumulation without clipping
   - Pan law (constant power vs. linear)
   - Master volume smoothing

5. **The Sample Count**
   - Global time reference
   - Envelope timing
   - Why `volatile`?

## Part IV: Control & Management

### Chapter 10: Voice Control API
**Goal:** High-level interface for ASCII protocol

1. **Separation of Concerns**
   - Low-level (oscillator functions)
   - Mid-level (voice control functions)
   - High-level (protocol parsing - not covered here)

2. **Parameter Setting Functions**
   ```c
   freq_set()    - Set frequency in Hz
   freq_midi()   - Set via MIDI note number
   amp_set()     - Amplitude control
   pan_set()     - Stereo positioning
   wave_set()    - Select waveform
   ```

3. **Error Handling Strategy**
   - Return codes
   - Parameter validation
   - Graceful degradation

4. **Voice Management**
   - `voice_reset()` - Return to defaults
   - `voice_copy()` - Duplicate voice settings
   - `voice_trigger()` - Start playback
   - Voice allocation strategies (not shown, but important)

5. **State Inspection**
   - `voice_format()` - Serialize voice state
   - `voice_show()` - Debug output
   - Why this matters for the ASCII protocol

### Chapter 11: Wave Table Management
**Goal:** Loading and managing sample data

1. **Wave Table Architecture**
   - Global table vs. per-voice tables
   - Indices and indirection
   - Multiple sample rates

2. **Built-in Waveforms**
   - Sine, square, saw, triangle
   - White noise generation (LCG)
   - Phase-aligned generation

3. **The Korg Waves**
   - Retro waveforms from classic synths
   - Licensing and attribution
   - Integration approach

4. **AMY PCM Samples**
   - One-shot samples
   - Loop points in samples
   - MIDI note tuning
   - Normalization strategy

5. **Loading External Samples**
   - File format considerations
   - Resampling needs
   - Memory management

6. **Wave Table Best Practices**
   - Size selection (power of 2?)
   - Interpolation (none, linear, cubic)
   - Anti-aliasing considerations

## Part V: Advanced Topics

### Chapter 12: Numerical Considerations
**Goal:** Why the code works (or breaks)

1. **Floating-Point Phase Accumulation**
   - Why float, not double?
   - Precision loss over time
   - When it matters (long loops)
   - Mitigation strategies

2. **Fixed-Point Alternative**
   - When to use integer math
   - Q notation (e.g., Q16.16)
   - Trade-offs: speed vs. flexibility

3. **Fast Approximations**
   - `fast_pow()` explained in detail
   - Error analysis
   - When good enough is good enough

4. **Denormals and Performance**
   - What are denormal numbers?
   - CPU penalty
   - Flush-to-zero strategies

5. **Preventing Overflow**
   - Clipping vs. limiting
   - Soft clipping curves
   - Why mixing matters

### Chapter 13: Performance Optimization
**Goal:** Making it fast without sacrificing clarity

1. **Profiling First**
   - Measure, don't guess
   - Hot spots in this code
   - Tools: gprof, perf, Instruments

2. **Optimization Techniques Used**
   - Loop invariant hoisting
   ```c
   const int table_size = voice_table_size[v];
   ```
   - Precomputed values (loop boundaries)
   - Branch prediction friendly code

3. **What NOT to Optimize**
   - Premature optimization pitfalls
   - Readability vs. performance
   - Compiler optimization levels (-O2, -O3)

4. **SIMD Possibilities**
   - Auto-vectorization opportunities
   - Manual SIMD (SSE, NEON, AVX)
   - When it's worth the complexity

5. **Multi-Threading Considerations**
   - Per-voice parallelism
   - Lock-free approaches
   - Why this code is single-threaded

### Chapter 14: Testing & Debugging
**Goal:** Ensuring correctness

1. **Unit Testing Strategies**
   - Testing oscillators in isolation
   - Known waveform verification
   - Frequency accuracy tests

2. **Audio Debugging Techniques**
   - Writing to WAV files
   - Spectrum analysis
   - Phase analysis
   - Visual debugging (waveform plots)

3. **Common Bugs**
   - Click detection
   - DC offset
   - Phase discontinuities
   - Index out-of-bounds

4. **Stress Testing**
   - All 32 voices active
   - Extreme modulation depths
   - Rapid parameter changes
   - Edge case frequencies (very low, very high)

5. **Validation Tools**
   - Reference implementations
   - Test signal generation
   - Difference testing

### Chapter 15: Porting & Platform Considerations
**Goal:** From laptop to embedded device

1. **Platform Requirements**
   - CPU requirements
   - Memory footprint analysis
   - Real-time OS needs

2. **Endianness**
   - Why it matters for samples
   - Portable code patterns

3. **Compiler Differences**
   - GCC, Clang, MSVC
   - `#pragma` portability
   - Standard library availability

4. **Hardware-Specific Optimizations**
   - ARM NEON
   - x86 SSE/AVX
   - DSP processors

5. **Embedded Constraints**
   - No malloc (static only)
   - Limited stack
   - No floating-point hardware
   - Fixed-point conversions

## Part VI: Integration & Extension

### Chapter 16: The ASCII Protocol Interface
**Goal:** How commands become sound

1. **Protocol Design Philosophy**
   - Human-readable commands
   - One command per line
   - Parameter naming conventions

2. **Example Commands**
   ```
   v0 w0 f440 a0.5      # Voice 0: sine, 440Hz, 50% volume
   v1 w1 n60 E0.1,0.2,0.7,0.3  # Voice 1: square, C4, ADSR envelope
   v0 F1,0.5            # Voice 0 FM by voice 1, depth 0.5
   ```

3. **Parser Integration**
   - Tokenization
   - Parameter parsing
   - Command dispatch
   - Error reporting

4. **Queued Commands**
   - Sample-accurate timing
   - Event scheduling
   - The `queued_t` structure

5. **MIDI Integration**
   - MIDI to ASCII translation
   - Voice allocation
   - Note on/off mapping

### Chapter 17: Building a Drum Machine
**Goal:** Extending the synth for percussion

1. **Percussion Synthesis Techniques**
   - Kick: sine sweep + noise
   - Snare: noise + tone
   - Hi-hat: filtered noise
   - Using existing oscillators creatively

2. **Sample-Based Drums**
   - One-shot samples
   - Velocity layers
   - Round-robin sampling

3. **Sequencer Integration**
   - Step sequencer
   - Pattern programming
   - Tempo and timing

4. **Drum Voice Allocation**
   - Dedicated voices vs. dynamic
   - Priority systems
   - Voice stealing

### Chapter 18: Future Enhancements
**Goal:** Where to go from here

1. **Additional Oscillator Types**
   - FM operators
   - Physical modeling
   - Additive synthesis
   - Granular synthesis

2. **More Filter Types**
   - State variable filters
   - Moog ladder filter
   - Formant filters
   - Multi-stage filters

3. **Effects Processing**
   - Delay/echo
   - Reverb
   - Chorus/flanger
   - Distortion/overdrive

4. **Polyphonic Features**
   - Unison/detune
   - Chord memory
   - Arpeggiator

5. **Advanced Modulation**
   - LFOs (dedicated low-frequency oscillators)
   - Modulation matrix
   - Envelope followers
   - Step sequencers as mod sources

## Part VII: Appendices

### Appendix A: Complete Code Listings
- Annotated full source
- Build instructions
- Makefile examples

### Appendix B: Mathematics Reference
- Frequency/MIDI conversion tables
- Filter coefficient formulas
- Modulation mathematics
- Decibel calculations

### Appendix C: Audio Theory Primer
- Sample rate and Nyquist
- Aliasing
- Bit depth
- Analog vs. digital

### Appendix D: Optimization Checklist
- Before you start
- Profiling tools
- Common patterns
- Platform-specific tips

### Appendix E: Resources
- Books on DSP
- Online communities
- Open source synths to study
- Datasets (sample libraries)

---

## Pedagogical Notes

### For Each Chapter:
1. **Start with "Why"** - Motivation before implementation
2. **Show the math** - But explain it intuitively first
3. **Code snippets** - Short, focused examples before full functions
4. **Common pitfalls** - What goes wrong and how to fix it
5. **Exercises** - Hands-on challenges
6. **Further reading** - References for deep dives

### Progression:
- **Chapters 1-2:** Architecture (can skim if eager to code)
- **Chapters 3-6:** Core components (essential, read in order)
- **Chapters 7-9:** Integration (how pieces fit together)
- **Chapters 10-11:** Control (practical usage)
- **Chapters 12-15:** Advanced (can read out of order based on interest)
- **Chapters 16-18:** Applications (concrete projects)

### Tone:
- **Practical, not academic** - Working code over perfect theory
- **Honest about trade-offs** - No solution is perfect
- **Encouraging experimentation** - "Try changing this and listen"
- **War stories welcome** - Real debugging experiences

### Code Style:
- **Commented thoroughly** in the book
- **Progressive revelation** - Simple version first, optimizations later
- **Complete examples** - Nothing that won't compile
- **Accompanying audio files** - Let readers hear the results