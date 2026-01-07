# skred
![skred logo](skred.png)

```wavetables gone rogue — snap together voices like LEGO, then poke 'em with cheeky ASCII spells for instant sonic mischief```

## description

`skred` is a polyphonic wavetable synthesizer built for flexibility and live performance.

Instead of fixed, hardwired signal paths, it lets you freely interconnect voices in a modular playground — route anything to anything, and reshape sounds on the fly.

It skips ultra-polished features like pristine interpolation in favor of raw responsiveness and real-time control.

You command it the same way everywhere: terse ASCII messages sent over wire(less) or typed directly into the console.

Simple pattern playback keeps the grooves rolling while you twist the knobs — or the code.

In short: a lightweight, hackable synth that feels alive under your fingers, whether you're performing live or scripting chaos from a terminal.


# quick start

# behind the scenes

## components
- synth
- seq
- wire

## synth

### Voice Architecture

* Polyphonic: Up to 64 simultaneous voices

* Each voice is independent with
  * Oscillator state (phase, frequency, wave-table)
  * Amplitude envelope (ADSR)
  * Filter state
  * Modulation routing
  * Pan position
  * One-shot mode
  * Reverse playback mode
  * Casio CZ-style phase distortion

* A voice can chain MIDI note and trigger/velocity level to one or two other voices

* Signal flow per voice

```mermaid
%%{init:{'flowchart': {'nodePadding': 20},{'rankSpacing': 20}}}%%
graph TD
w(waveform)
v(voice)
f("frequency<br>&rarr;mod")
c("phase distortion<br>&rarr;mod")
h("sample & hold (down-sampling)")
q("quantizer (bit-reduction)")
J("multi-mode filter (LP/HP/BP/Notch/AP)")
a("amplitude<br>&rarr;mod<br>ADSR&rarr;")
X((mod))
p("panner<br>&rarr;mod")
v-->w;
w-->f;
f-->c;
c-->h;
h-->q;
q-->J;
J-->a;
a-->X;
X-->p;
p-->L & R;
```

## seq

## wire

# skred usage

## getting started

```
# from a shell (Terminal.app, cmd.exe, bash, etc.)
./skred
```

### start-up options

```
-n # don't use command line history
-p #
-1 #
-2 #
```

## basic voice settings

| symbol | description | example |
| :--- | :--- | :--- |
| v | sets current voice (0-63) | v0 |
| a | amplitude | a4 |
| l | velocity / trigger (0 to ???) | l1 |
| f | frequency | f440.5 |
| w | waveform | w2 |
| p | pan | p-1 |
| c | phase distortion mode (0-6) and amount (0.0 to 1.0) |c1,.5|
| J | filter mode (0 to 5) | J1 |
| K | filter cutoff (0 to 44100 Hz) | |
| Q | filter resonance (0 to 1000 | |
| n | frequency by MIDI note # | |
| b | waveform playback forward/backward (0,1) | b1 |
| B | waveform looping off/on (0,1) | B1 |
| h | sample and hold (0 to ?) | h10 |
| q | lower bit rate (0 to 31) | q4 |

## advanced voice settings

| symbol | description | example |
| :--- | :--- | :--- |
| A | amplitude modulation (mod-voice, depth) | A1,.5 |
| F | frequency modulation (mod-voice, depth) | F1,.5 |
| P | pan modulation (mod-voice, depth) | P1,.5 |
| C | phase distortion modulation (mod-voice, depth) | C1,.5 |
| t | amplitude ADSR | t0,1,1,0 |
| m | disable voice from audio out (0,1) | m1 |
| G | set another voice(s) on MIDI note # (up to two other voices | G16,32 |
| H | trigger another voice(s) on velocity (up to two other voices | H16,32 |

## parameters

### basic waveforms
| name | description |
| :--- | :--- |
| 0 | sine |
| 1 | square |
| 2 | saw down |
| 3 | saw up |
| 4 | triangle |
| 5 | low-period noise |
| 6 | high-period noise |

### instrument harmonic waveforms

| name | description |
| :--- | :--- |
| 32 | brass, strings, and analog synths |
| 33 | clarinet and analog synths |
| 34 | acoustic piano |
| 35 | electric piano |
| 36 | electric piano (hard) |
| 37 | clavi |
| 38 | organ |
| 39 | brass |
| 40 | saxophone |
| 41 | violin |
| 42 | acoustic guitar |
| 43 | guitar (distorted) |
| 44 | electric bass |
| 45 | digital bass |
| 46 | bell |
| 47 | organ and whistle |

| name | description |
| :--- | :--- |
| 48 to 62 | expansion waves |

| name | description |
| :--- | :--- |
| 100 to 166 | basic samples |

| name | description |
| :--- | :--- |
| 200 to 999 | user wave slots |

| filter mode | description |
| :--- | :--- |
| 0 | off |
| 1 | low-pass |
| 2 | high-pass |
| 3 | band-pass |
| 4 | notch |
| 5 | all-pass |

| phase distortion mode | description |
| :--- | :--- |
| 0 | off |
| 1 | saw -> pulse (PWM-like) |
| 2 | square (resonant) |
| 3 | triangle shaping |
| 4 | double sine (octave up) |
| 5 | saw -> triangle |
| 6 | resonant (ephasize harmonics) |

### ADSR
```
Trigger
  │
  ↓
Attack ──→ Decay ──→ Sustain ───┐
  /\         \          ___     │ (hold until release)
 /  \         \        /   \    │
/    \         \      /     \   ↓
      \         \____/       Release
       \                      \
        \                      \____
```

## build
```
make skred
```

## run
```
./skred
```

# sk8r
![sk8r logo](sk8r/icon.png)

realtime parameter sliders

## build
```
make sk8r-linux
```
## run
```
./build/sk8r-linux
```

# sk8-pad
![sk8-pad logo](sk8-pad/icon.png)

flexible drum machine like pads
## build
```
make pad-linux
```
## run
```
./build/sk8-pad-linux
```

# midi-sk8
![midi-sk8 logo](midi-sk8/icon.png)

flexible MIDI to `skode` for mapping NOTE ON/OFF and pitch bend events
## build
```
make midi-linux
```
## run
```
./build/midi-sk8-linux
```

## credits
- miniaudio
- AMY
- PureData
- ChucK
- SonicPi
## homage
- Robert Moog
- Dave Smith
## background

- needs asound on linux
  - sudo apt install libasound2-dev
