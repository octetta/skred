# Skode Synthesis Tutorial

This tutorial assumes basic familiarity with synthesis concepts. If you already know
what oscillators, envelopes, filters, and LFOs are, skip ahead to **Section 2: The Basics**.
If those terms are unfamiliar, read Section 1 first — it covers just enough to make the
rest of the tutorial make sense.

-----

## 1. Synthesis 101 — The Short Version

A synthesizer makes sound by generating a signal and shaping it. In skode everything
happens in software, but the same concepts apply as in a hardware synthesizer.

### Sound, frequency and pitch

Sound is air being pushed back and forth — vibrating — at a regular rate. That rate
is **frequency**, measured in Hz (cycles per second). The faster the vibration, the
higher the pitch. 440Hz is the note A above middle C. 220Hz is one octave lower —
half the frequency, half the pitch. 880Hz is one octave higher — double the frequency.
Human hearing covers roughly 20Hz to 20,000Hz.

### Amplitude and decibels

**Amplitude** is how strongly the signal pushes — perceived as loudness. In skode
amplitude is set in **decibels (dB)**, a logarithmic scale that matches how human
hearing works.

The key reference point is **0dB = unity** — the signal passes through at full
strength. This is not silence. Louder is positive, quieter is negative:

- `a0` — full amplitude (unity, 0dB)
- `a-6` — roughly half as loud
- `a-12` — roughly a quarter as loud
- `a-60` — nearly inaudible, effectively silent
- `a6` — double amplitude, may clip if multiple voices sum together

Each -6dB step halves the perceived loudness. In practice use values between
`a-12` and `a0` for individual voices, leaving headroom when multiple voices play together.

### Waveforms and oscillators

An **oscillator** is a signal generator — it produces a repeating signal at a
set frequency. You can think of it as a motor spinning at a controllable speed,
where the speed sets the pitch.

The shape of the repeating signal is the **waveform**. If you were to draw the
signal on a graph — voltage on the vertical axis, time on the horizontal — different
waveforms have different characteristic shapes. That shape determines the tonal
character of the sound — its **timbre** — because different shapes contain different
combinations of frequencies above the fundamental, called **harmonics**:

- A **sine wave** is a smooth curve — the simplest possible waveform. It contains
  only the fundamental frequency with no harmonics. Pure and simple.
- A **square wave** alternates abruptly between two levels. It contains only odd
  harmonics (the fundamental, 3×, 5×, 7×…). Sounds hollow and buzzy.
- A **sawtooth wave** rises gradually then drops sharply. It contains all harmonics
  (the fundamental, 2×, 3×, 4×…). Bright and rich — the most common synthesis waveform.
- A **triangle wave** rises and falls in a V shape. Contains odd harmonics like the
  square wave, but they fade out quickly. Softer and rounder than square.
- **Noise** is a random signal with no repeating pattern — all frequencies at once.

This is the most important concept in the tutorial: **you cannot add harmonics with
a filter, only remove them**. Choosing the right waveform is the first and most
critical decision when designing a sound.

### Envelopes shape volume over time

When a note starts, the sound doesn’t just switch on instantly at full volume and
switch off the moment you release the key. An **envelope** controls how amplitude
changes over time. The standard four-stage shape is called ADSR:

- **Attack** — how long it takes to reach full volume after the note starts
- **Decay** — how long it takes to fall from full volume down to the sustain level
- **Sustain** — the volume level held while the note is held (0 = silence, 1 = full)
- **Release** — how long it takes to fade to silence after the note is released

A drum hit: very short attack, short decay, zero sustain. A pad: long attack, long
release. A pluck: zero attack, medium decay, zero sustain.

### Filters shape the frequency content

A **filter** selectively removes frequencies from a signal. The **cutoff frequency**
sets the boundary. A lowpass filter lets frequencies below the cutoff pass through
and removes those above — making the sound darker. A highpass filter does the
opposite. A bandpass filter only lets through a band around the cutoff.

**Resonance** (often called Q) boosts the frequencies right at the cutoff point,
creating a peak or whistle. High resonance at a sweeping cutoff produces the classic
synthesizer filter sweep sound.

### Modulation makes things move

A static sound with fixed pitch, volume, and filter character quickly becomes
uninteresting. **Modulation** means using one signal to control a parameter of
another — making it change over time.

A slow oscillator running at 1–5Hz (below the range of hearing) is called an
**LFO** (Low Frequency Oscillator). An LFO modulating amplitude creates tremolo.
The same LFO modulating pitch creates vibrato. An envelope modulating filter
cutoff creates the classic brightness sweep on a note attack.

In skode there are no dedicated LFOs — any voice running at a slow frequency
can modulate any other voice’s amplitude, pitch, or pan.

### Voices

In skode a **voice** is a complete sound-making unit — its own oscillator, filter,
envelope, and modulation settings. Skode has 64 voices, numbered v0 to v63.

Voices can run entirely independently, or be linked so that playing one automatically
plays others — useful for layering multiple oscillators into a single rich sound.
Each voice is configured and triggered individually, which gives you precise control
over every layer of a patch.

-----

## 2. The Basics — One Voice

### Using the REPL

Skode is controlled by typing commands into a prompt — a REPL (Read-Eval-Print Loop).
Nothing happens until you press Enter. Until then you can edit freely:

- **Arrow keys** move the cursor left and right within the line
- **Backspace/Delete** edit as normal
- **Up/down arrows** scroll through command history — useful for repeating or
  tweaking a previous command without retyping it

Commands on a line are processed left to right. You can put multiple commands
on one line separated by spaces:

```
v0 w2 a0 f 220 l1
```

Or enter them one at a time — the result is identical:

```
v0
w2
a0
f 220
l1
```

Entering `v0` alone **points** the REPL at voice 0. Every command you type after
that applies to voice 0 until you point at a different voice with `v1`, `v2`, etc.
This saves a lot of keystrokes when tweaking a single voice interactively.

All parameters are **sticky** — they keep their value until you change them.
If you set `f 220` and then type `a-6 l1`, the voice plays at 220Hz. You only
need to type what’s changing.

Commands and their arguments are flexible about spaces — `f220`, `f 220`, and
`f  220` all work the same way.

**Comments** start with `#` — everything after `#` on a line is ignored:

```
v0 w2 f 220 l1    # this is a comment, ignored by skode
```

This is useful for annotating patches you save as text files. The `\` command
(verbose voice print) uses `#` to prefix internal debug values so its output
can be pasted back into the REPL safely — the debug values are ignored and only
the skode commands take effect.

To stop a sustained note send `l0` on the current voice:

```
v0l0
```

To record output to a WAV file, arm the voice with `r1` and use `<` with a
duration in seconds:

```
v0r1<3    # record 3 seconds of voice 0
```

Start with the simplest possible sound:

```
v0 w0 a0 l1
```

You should hear a 440Hz sine wave at 0dB sustaining until you send `v0l0`.

Now try the other waveforms:

```
v0 w1 a0 l1     — square wave, buzzy, hollow
v0 w2 a0 l1     — sawtooth, bright and full
v0 w4 a0 l1     — triangle, softer than square
v0 w5 a0 l1     — noise, broadband hiss
v0 w6 a0 l1     — noise, slightly different character
```

The waveform choice is the single most important decision in patch design.
Square and triangle have only odd harmonics (f, 3f, 5f…).
Saw has all harmonics (f, 2f, 3f…). Sine has none.
You cannot add harmonics with a filter — only remove them.
Choose wrong here and no amount of processing fixes it.

Change the frequency:

```
v0 w2 a0 f 110 l1     — A2, one octave below 440Hz
v0 w2 a0 f 55 l1      — A1, sub bass territory
```

Change the amplitude:

```
v0 w2 a-6 f 110 l1    — half amplitude (-6dB)
v0 w2 a-12 f 110 l1   — quarter amplitude (-12dB)
v0 w2 a6 f 110 l1     — double amplitude (+6dB, may clip)
```

**Experiment:** Try all five waveforms at 110Hz and listen to the harmonic differences.
The saw at 110Hz should sound noticeably richer and brighter than the sine.

-----

## 2. ADSR Envelopes

The `t` command sets Attack, Decay, Sustain, Release in seconds.
Sustain is a level from 0 to 1, not a time.

```
v0 w2 a0 t 0 0 1 0 f 110 l1     — flat, immediate on/off (default)
v0 w2 a0 t 0.1 0 1 0 f 110 l1   — 100ms attack, fades in
v0 w2 a0 t 0 0.5 0.6 0 f 110 l1 — instant on, decays to 60% over 500ms
v0 w2 a0 t 0 0.3 0 0.5 f 110 l1 — instant on, decays to silence, 500ms release tail
```

For the last one, trigger then release after a second:

```
v0 w2 a0 t 0 0.3 0 0.5 f 110 l1
(wait a moment)
v0l0
```

You should hear the 500ms release tail fade out after you send l0.

**Experiment:** Design a pluck — fast attack, medium decay, zero sustain:

```
v0 w2 a0 t 0 0.4 0 0 f 220 l1
```

And a pad — slow attack, long decay, high sustain:

```
v0 w2 a0 t 0.8 1.0 0.8 1.0 f 220 l1
```

-----

## 3. The Filter

The filter sits after the oscillator. `J` sets the mode, `K` sets cutoff in Hz, `Q` sets resonance.

Set up once, then just change K to hear the difference:

```
v0 w2 a0 f 220 J1 Q 0.7
K 500 l1    — lowpass at 500Hz, no resonance
K 800 l1    — brighter
K 300 l1    — darker
```

Critical rule: K must always be above the frequency you want to hear.
If K is below the fundamental, the fundamental itself gets cut.

```
K 150 l1    — K below 220Hz fundamental, sounds wrong/thin
```

Now raise Q to hear resonance — K stays at 150, just change Q:

```
K 800 Q 2.0 l1    — slight resonance
Q 4.0 l1          — strong resonance, peak at 800Hz audible
Q 7.0 l1          — very strong, whistling peak
```

Try bandpass on noise:

```
v0 w6 J2 K 800 Q 0.5 l1    — bandpass noise, dull metallic
Q 2.0 l1                    — tighter band, more metallic
K 2000 Q 0.4 l1             — higher pitch noise band
```

**Experiment:** Design a telephone ring — bandpass noise at high frequency:

```
v0 w6 a0 t 0 0 1 0 J2 K 3400 Q 0.3 l1
```

-----

## 4. Filter Envelope

The filter can have its own ADSR independent of the amplitude envelope.
`ft` sets the filter ADSR, `fd` sets the sweep depth in Hz above K.

The formula is: cutoff = K + (envelope × depth)

At trigger (envelope=1.0): cutoff = K + depth
At sustain (envelope=S):   cutoff = K + (S × depth)
At rest (envelope=0.0):    cutoff = K

Start with a dramatic example so you can clearly hear it:

```
v0 w2 a0 f 220 J1 K 220 Q 3.0
t 0 2 0.5 0.5 ft 0 1.5 0.2 0.5 fd 2000 l1
```

You should hear the filter sweep from 2220Hz down to 660Hz over 1.5 seconds,
then hold at 660Hz while the note sustains. Release sends it back to 220Hz.

Now try a fast pluck shape — the classic synth bass stab:

```
f 110 K 110 Q 3.5 t 0 1.5 0.3 0.5 ft 0 0.4 0.3 0.4 fd 2800 l1
```

The filter opens to 2910Hz on trigger and decays to 950Hz over 400ms.
Amplitude also decays over 1.5 seconds. Two independent envelopes shaping
brightness and volume separately.

Negative depth opens the filter downward:

```
f 440 K 2000 Q 2.0 t 0 2 0.5 1.0 ft 0 1.5 0.5 0.5 fd -1500 l1
```

Filter starts at 500Hz on trigger and opens back to 2000Hz as the note decays.

**Experiment:** Make the filter envelope longer than the amplitude envelope:

```
f 220 K 220 Q 4.0 t 0 0.8 0 0.5 ft 0 3.0 0 0.5 fd 3000 l1
```

-----

## 5. Bit Crush and Sample and Hold

These two commands add lo-fi digital character and work well together.
Sample and hold runs before bit crush in the signal chain.

**Bit crush** — `q[bits]`:

```
v0 w2 a0 f 110
q0 l1     — clean
q8 l1     — obvious crunch
q4 l1     — heavy crunch
q1 l1     — extreme lo-fi
```

Warning: q1–q4 on tonal voices above 800Hz produces chaotic aliasing.
Safe below 400Hz. Use with caution above that.

**Sample and hold** — `h[n]`, holds each sample for n phase increments.
Acts as a sample rate reducer — higher n = lower effective sample rate:

```
h0 q0 l1     — clean baseline
h4 l1        — mild staircase, retro texture
h8 l1        — obvious stepping, game console feel
h16 l1       — strong lo-fi
```

Combine both for classic vintage digital character:

```
h8 q8 l1     — stepped and crushed
h4 q12 l1    — subtler retro texture
```

On noise, both work well at any value:

```
v0 w6 J2 K 800 Q 0.5 h4 q8 l1    — crunchy stepped metallic noise
```

-----

## 6. Phase Distortion

Phase distortion reshapes the waveform in the style of Casio CZ synthesizers.
`c[mode],[amount]` — mode selects the algorithm (see separate documentation),
amount runs 0–1 where 0.5 is the neutral point with no distortion.

```
v0 w0 a0 f 220
c1,0.5 l1    — mode 1, neutral (same as sine)
c1,0.3 l1    — mode 1, distorted toward 0
c1,0.8 l1    — mode 1, distorted toward 1
```

The effect varies significantly by mode and amount — experiment to find useful timbres.
Phase distortion can be modulated via C:

```
v1 w0 a0 f 2 m1
v0 w0 a0 f 220 c1,0.5 C1,0.3 l1
```

v1 at 2Hz modulates the distortion amount on v0 — waveshaping tremolo effect.
Like AM and FM, any voice including one-shot wavetable modulators can drive C.

-----

## 7. Pan

```
v0 w2 a0 f 220 l1
p-1 l1    — hard left
p1 l1     — hard right
p0 l1     — centre
```

Pan modulation works like AM — a modulator voice drives the pan position:

```
v0 w0 a0 f 0.5 m1
v1 w2 a0 f 220
P0,1.0,0.0 l1
```

v0 is a 0.5Hz sine. P0,1.0,0.0 on v1 swings pan from -1 to +1 at 0.5Hz — autopan.
Narrow the swing with a smaller depth: `P0,0.4,0.0` keeps it closer to centre.

**Experiment:** Try a faster LFO rate for tremolo-like pan movement:

```
v0 f 4 l1    — 4Hz pan oscillation
```

-----

## 8. Modulator Voices — Amplitude Modulation

A modulator is a voice with `m1` — it produces a signal but is silent in the mix.
Other voices use its output to shape their amplitude (A command) or frequency (F command).

The A command syntax: `A[mod_voice],[depth],[offset]`
Formula: amp_multiplier = (mod_sample × depth) + offset

Start with a simple AM setup — sine modulator at 3Hz giving tremolo:

```
v0 w0 a0 t 0 0 1 0 f 3 m1
v1 w2 a0 f 220 A0,0.8,0.2 l1
```

Voice 0 oscillates at 3Hz. Voice 1’s amplitude is modulated by it.
depth=0.8, offset=0.2 means amplitude swings between 0.2 and 1.0 — never fully silent.

Now use the modulator as a one-shot decay envelope. Define a custom wavetable:

```
( 1 .82 .67 .54 .44 .35 .28 .22 .17 .13 .09 .07 .04 .025 .012 .003 )
/d300

v0 w300,1,1 a0 t 0 2 0 0 f 1 m1
v1 w2 a0 t 0 .900 0 0 f 540 J1 K 2500 Q 0.35 A0,1.0,0.0 H0,-1,-1,-1 l1
```

The wavetable is 16 points from 1.0 down to near zero — an exponential decay curve.
`w300,1,1` means one-shot with interpolation. `f 1` means play over 1 second.
`A0,1.0,0.0` — full depth, no floor — pure exponential decay shape.
`H0,-1,-1,-1` on v1 — when v1 fires, also trigger v0. One-shot wavetables
only start when triggered this way via H.

You should hear the 540Hz saw decay with an exponential curve over 900ms.

**Important:** The ADSR on v1 sets the duration. The wavetable on v0 sets the curve shape.
`f 1` on v0 makes the wavetable play over 1 second — longer than the 900ms ADSR —
so the curve is sampled across the full duration smoothly.

**Experiment:** Try different curve shapes. A faster decay:

```
( 1 .75 .54 .37 .25 .16 .10 .06 .035 .018 .009 .004 .001 0 0 0 )
/d301

v0 w301,1,1 a0 t 0 2 0 0 f 1 m1
v1 w2 a0 t 0 .400 0 0 f 540 J1 K 2500 Q 0.35 A0,1.0,0.0 H1,0,-1,-1 l1
```

-----

## 9. Frequency Modulation — FF1 Pitch Sweeps

Standard FM (`F` command) is designed for classic FM synthesis where the modulator
frequency is part of the sound character. For pitch sweeps on percussion — where you
want an exact Hz-accurate drop from one frequency to another — use FF1 mode.

FF1 formula: frequency = (mod_sample × depth) + offset

- offset = settled frequency in Hz
- depth = how many Hz above offset at trigger

Define a linear sweep wavetable and use it for a pitch sweep:

```
( 1 .87 .74 .62 .51 .40 .30 .21 .14 .08 .04 .015 .004 0 0 0 )
/d302

v1 w302,1,1 a0 t 0 1 0 0 f 20 m1
v0 w0 a0 t 0 0.5 0 0 f 100 FF1 F1,100,50 H1,-1,-1,-1 l1
```

At trigger: freq = (1.0 × 100) + 50 = 150Hz
After sweep: freq = (0.0 × 100) + 50 = 50Hz
`f 20` on v1 means the sweep takes 1/20 = 50ms.

You should hear the sine drop from 150Hz to 50Hz over 50ms.

Change the sweep speed by changing `f` on the modulator:

```
v1 f 10 l1    — 100ms sweep
f 33 l1       — 30ms sweep (good for 808 kick)
f 5 l1        — 200ms sweep (slow, melodic)
```

Change the sweep range by changing the depth parameter in F on v0:

```
v0 FF1 F1,50,50 l1     — 50Hz sweep (100Hz → 50Hz)
F1,200,50 l1           — 200Hz sweep (250Hz → 50Hz)
```

**Experiment:** A pitch sweep on a filtered sine — starting point for a kick drum:

```
( 1 .87 .74 .62 .51 .40 .30 .21 .14 .08 .04 .015 .004 0 0 0 )
/d302

v1 w302,1,1 a0 t 0 1 0 0 f 33 m1
v0 w0 a0 t 0.022 0.200 0 0 f 49 FF1 F1,40,49 J1 K 120 Q 3.5 H1,-1,-1,-1 l1
```

This is the 808 kick pitch sweep — 89Hz dropping to 49Hz over 30ms.

-----

## 10. Trigger Chains — H and G

`H` fires slave voices atomically when the master fires.
`G` forwards MIDI note number to slave voices for pitch tracking.

Build a two-layer drum hit — noise click plus tonal body:

```
v1 w6 a4 t 0 .010 0 0 J1 K 3000 Q 0.3
v0 w0 a0 t 0.005 0.300 0 0 f 80 J1 K 140 Q 4.0 H1,-1,-1,-1 l1
```

v0 is the sine body, master voice. v1 is the noise click, slave.
H1,-1,-1,-1 on v0 means: when v0 gets l1, also trigger v1.
You should hear a brief click followed by a tonal thud.

For melodic voices, G ensures all oscillators track the played note:

```
v1 w2 a0 t 0.008 1.0 0.7 0.3 f 220 G2,3,-1,-1 H2,3,-1,-1 l1
v2 w2 a0 t 0.008 1.0 0.7 0.3 N0,7
v3 w2 a0 t 0.008 1.0 0.7 0.3 N0,-7
```

`G2,3` forwards the MIDI note to v2 and v3.
`N0,7` on v2 = +7 cents detune. `N0,-7` on v3 = -7 cents detune.
All three oscillators track the same note but v2/v3 are slightly detuned —
creating slow chorus beating.

Try triggering on different notes:

```
v1n48l2    — C3
v1n52l2    — E3
v1n55l2    — G3
```

All three oscillators move together maintaining the detuning relationship.

**N command reference:**

- N0,0 — no detune (same as master)
- N0,7 — +7 cents sharp
- N0,-7 — -7 cents flat
- N-12,0 — one octave down (sub)
- N7,0 — perfect fifth above
- semitone formula: 12 × log2(f2/f1)

-----

-----

## 11. Loading WAV Files

WAV files are loaded from a `wav/` directory relative to where skred was started.
The command `/w[n],[slot]` loads `n.wav` into a wavetable slot:

```
/w1,500     — loads wav/1.wav into slot 500
/w22,501    — loads wav/22.wav into slot 501
```

Then use the slot like any wavetable:

```
v0 w500 a0 l1        — play at natural speed. 440Hz = native sample pitch.
f 880 l1             — play at double speed, one octave up
f 220 l1             — half speed, one octave down
```

Looping is off by default. Turn it on with B1:

```
B1 l1                — loops continuously until l0
B0                   — back to one-shot
```

Reverse playback with b1:

```
b1 l1                — plays backwards
b0                   — back to forward
```

Loaded samples work as one-shot modulators exactly like custom wavetables:

```
/w1,500
v0 w500,1,1 a0 t 0 2 0 0 f 1 m1    — one-shot sample as AM modulator
```

**Experiment:** Load a short drum sample and pitch it up and down:

```
/w1,500
v0 w500 a0
f 440 l1     — natural pitch
f 660 l1     — up a fifth
f 330 l1     — down a fifth
```

-----

## 12. Building a Drum Kit

### Kick Drum

```
( 1 .87 .74 .62 .51 .40 .30 .21 .14 .08 .04 .015 .004 0 0 0 )
/d301

v1 w301,1,1 a0 t 0 1 0 0 f 33 m1
v2 w6 a4 t 0 .008 0 0 q1 J1 K 2500 Q 0.30
v0 w0 a2 t 0.022 0.200 0 0 f 49 FF1 F1,40,49 J1 K 120 Q 3.5 H1,2,-1,-1 l1
```

- v0: sine body, 49Hz, pitch sweeps from 89Hz over 30ms, resonant LPF
- v1: one-shot wavetable modulator controlling the pitch sweep
- v2: noise click for beater transient, 8ms, dies before the sine takes over

Expected: a thumpy 808-style kick with a click on the attack and a clear
pitch drop from high to low in the first 30ms.

### Hihat (Closed)

```
v5 w6 a-2 t 0 .040 0 0 q2 J1 K 8000 Q 0.4 l1
```

Short noise burst, highpass-ish character from high K, 40ms decay.
Expected: a tight closed hihat click.

### Hihat (Open)

```
v5 w6 a-2 t 0 .300 0 0 q2 J1 K 8000 Q 0.4 l1
```

Same voice, longer decay. Retriggering v5 with the closed hihat naturally chokes
the open — the envelope restarts cutting off the previous tail.

### Snare

```
v6 w6 a2  t 0 .120 0 0 q2 J2 K 250 Q 0.4 l1
```

Bandpass noise at 250Hz — the snare body frequency range.
Expected: a snappy noise burst with some body. Adjust K for snare pitch character.

### Cowbell

```
( 1 .82 .67 .54 .44 .35 .28 .22 .17 .13 .09 .07 .04 .025 .012 .003 )
/d300

v19 w300,1,1 a0 t 0 2 0 0 f 1 m1

v23 w2 a-10 t 0 .900 0 0 f 540 J1 K 2500 Q 0.35 A19,1.0,0.0 H24,25,19,-1
v24 w1 a2   t 0 .900 0 0 f 812 J1 K 1100 Q 0.40 A19,1.0,0.0
v25 w6 a6   t 0 .015 0 0 q1 J0 l1
```

Two oscillators at inharmonic frequencies (540Hz and 812Hz) give the metallic character.
Wavetable AM shapes the exponential decay. Noise click adds the attack transient.
Expected: a bright metallic cowbell decaying over 900ms.

-----

## 13. Building a Fat Analog Bass

Put everything together — three detuned oscillators, shared AM modulator,
per-voice filter envelopes, sub octave.

```
v0 w2 a0 t 0 1 0 0 f 1 m1

v1 w2 a0 t 0.008 1.5 0.7 0.4 J1 K 110 Q 3.5 ft 0 0.4 0.3 0.4 fd 2800 A0,0.9,0.1 G2,3,4,-1 H2,3,4,-1 l1
v2 w2 a0 t 0.008 1.5 0.7 0.4 J1 K 110 Q 3.5 ft 0 0.4 0.3 0.4 fd 2800 A0,0.9,0.1 N0,7
v3 w2 a0 t 0.008 1.5 0.7 0.4 J1 K 110 Q 3.5 ft 0 0.4 0.3 0.4 fd 2800 A0,0.9,0.1 N0,-7
v4 w0 a0 t 0.008 1.5 0.7 0.4 J1 K 180 Q 2.0 ft 0 0.6 0.4 0.4 fd 800  A0,0.9,0.1 N-12,0
```

Now play it on different notes:

```
v1n36l2    — C2
v1n40l2    — E2
v1n43l2    — G2
v1n36l0    — release
```

What’s happening:

- v0 shapes amplitude across all four oscillators with a 1-second sine decay
- v1/v2/v3 are three detuned saws — ±7 cents gives slow warm chorus
- v4 is a sub sine one octave down adding weight
- Each voice has its own filter envelope — brightness decays with the note
- G on v1 forwards the MIDI note to v2/v3/v4 so all oscillators track together

**Experiment:** Change the detune amount. N0,15 / N0,-15 for a wider spread.
More detune = more obvious chorus but less focused pitch. Find your sweet spot.

**Experiment:** Change the filter envelope decay. `ft 0 0.1 0.3 0.4` for a faster
filter close — more aggressive pluck character. `ft 0 2.0 0.5 0.4` for a slow
filter sweep — more pad-like.

**Experiment:** Remove the sub oscillator (comment out v4) and compare.
Then add it back. The difference in low-end weight is significant.

-----

## Key Principles to Remember

**Oscillator choice is irreversible.** You cannot add harmonics with a filter.
If the reference sound has even harmonics, use saw. If odd only, use square.
Get this right before touching anything else.

**K must be above your fundamental.** Always. Setting K below the fundamental
cuts the fundamental itself and the sound collapses.

**ADSR sets duration, wavetable sets curve shape.** When using one-shot
wavetable modulators, the ADSR on the target voice controls how long the
sound lasts. The wavetable controls the shape of the modulation within that time.
Set `f` on the modulator to be slightly longer than the target ADSR duration.

**FF1 is for pitch sweeps.** Standard FM (F without FF1) is coupled to the
modulator frequency and is not suitable for Hz-accurate pitch sweeps on percussion.
Always use FF1 with a one-shot wavetable for kick/tom pitch drops.

**One-shot wavetables need H.** A one-shot modulator will never fire unless
it is in the H chain of a triggered voice. This is the most common mistake
when building percussion patches.

**Filter envelope floor is K.** The `ft` and `fd` commands sweep above K.
K never changes — it is always the resting cutoff. Set K to where you want
the filter to sit when fully at rest.