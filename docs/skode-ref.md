# `skode` reference

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
| 10 | (1) sawtooth : brass, strings, and fat synths leads |
| 11 | (2) square : woodwinds (clarinets) and classic "hollow" synth sounds |
| 12 | (3) deep sawtooth |
| 13 | (4) narrow pulse : thin, nasally sounds; oboes and harpsichords |
| 14 | (5) electric piano (hard) |
| 15 | (6) clavi |
| 16 | (7) organ |
| 17 | (8) brass |
| 18 | (9) saxophone |
| 19 | (10) violin |
| 20 | (11) acoustic guitar |
| 21 | (12) guitar (distorted) |
| 22 | (13) electric bass |
| 23 | (14) digital bass |
| 24 | (15) bell |
| 25 | (16) organ and whistle |

| name | description |
| :--- | :--- |
| 26 to 41 | kw-expansion waves |

| name | description |
| :--- | :--- |
| 50 to 127 | kw-expansion waves |

| name | description |
| :--- | :--- |
| 200 to 266 | basic samples |

| name | description |
| :--- | :--- |
| 300 to 999 | user wave slots |

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
