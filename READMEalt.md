# `skred` - a live coding synthesizer

A real-time wavetable synthesizer with a compact command language designed for live performance and algorithmic composition.

## Features

- **64 polyphonic voices** with independent control
- **Wavetable synthesis** with multiple waveforms (sine, saw, square, triangle, noise, retro classics, PCM samples)
- **Modular routing** - voices can modulate each other (FM, AM, phase distortion)
- **Multi-mode filters** (lowpass, highpass, bandpass, notch, allpass)
- **ADSR envelopes** with velocity sensitivity
- **Pattern sequencer** with 16 patterns, 256 steps each
- **Sample-accurate scheduling** for precise timing
- **Live wavetable loading** from .wav files
- **Real-time recording** to .wav format
- **Casio CZ-style phase distortion** synthesis
- **UDP control** for external control and collaboration

## Quick Start

```bash
git clone https://github.com/octetta/skred.git
cd skred
make all
./skred
```

Then try your first sound:

```
v0 w1 f440 a0.5 l1
```

You should hear a 440Hz tone. Press Ctrl+C to exit (or type `/q`).

## Installation

### Requirements

- C compiler (gcc or clang)
- Standard C libraries

### Linux

```bash
make all
```

### macOS

```bash
make all
```

### Windows cross-compiled on Linux

```bash
# Install MinGW64
make win
```

---

# `skode` - a language for live synthesis control

`skode` is a compact command language designed for controlling `skred` in real-time. It's built around a simple rhythm: **commands followed by their values**, typed as fast as you can think.

## The Natural Flow

The language reads almost like speaking: "voice zero, wave one, frequency four-forty, amplitude half." But you type it tersely:

```
v0 w1 f440 a0.5
```

Because letters and numbers naturally separate from each other, you don't even need spaces most of the time. This works just as well:

```
v0w1f440a0.5
```

Your fingers learn the patterns. After a while, setting up a voice becomes muscle memory - you're not thinking about syntax, you're thinking about sound.

## Two Kinds of Things

The language has two fundamental elements that alternate:

**Atoms** (commands) - Letters and symbols that tell the synth what to change:
- `v` = voice selection
- `w` = waveform  
- `f` = frequency
- `a` = amplitude
- `n` = MIDI note (sets frequency)
- `l` = trigger note with velocity
- `p` = pan position

**Numbers** (values) - The actual settings:
- `440` for 440 Hz
- `0.5` for half volume
- `60` for middle C (MIDI note 60)
- `1` for full velocity
- `-0.3` for panning slightly left

The rhythm is always: command-number, command-number. Like a heartbeat.

## Separators Are Optional (Usually)

For single values, you can smash everything together:

```
f440        frequency 440
v3w2f220    voice 3, wave 2, frequency 220
```

When commands need multiple values, use spaces or commas to separate them - whichever feels natural:

```
t0.01,0.1,0.7,0.2    envelope: attack, decay, sustain, release
t 0.01 0.1 0.7 0.2   same thing with spaces
```

Both work identically. Use what feels right in the moment.

## Capturing Text and Data

Sometimes you need to capture chunks of text or lists of numbers.

**Strings** use curly braces to grab everything inside:

```
{hello world}
```

This is useful for storing commands in sequencer steps:

```
{v0 n60 l1} x0    step 0: play middle C on voice 0
{v0 n62 l1} x1    step 1: play D
```

Notice how the string comes *before* the step number - you're defining what to play, then assigning it to a step.

**Arrays** use parentheses to capture lists of numbers:

```
(1 2 3 4 5)
(0xFF 0x00 0x80)    can even use hex
```

These are handy for loading wavetable data or parameter sets.

## Building Up Arguments

Commands can take multiple values. The language just keeps collecting numbers until it sees a command:

```
F 0 100        frequency modulation: voice 0, depth 100
A 2 0.5        amplitude modulation: voice 2, depth 0.5
```

The command consumes however many values it needs. If you give it more, the extras just sit there waiting for the next command.

## Playing Notes

Setting a note with `n` only changes the frequency - to actually hear it, you need to trigger it with velocity:

```
v0 n60 l1      set MIDI note 60 (middle C), trigger with full velocity
v0 n62 l0.8    set MIDI note 62 (D), trigger at 80% velocity
v0 l0          note off (velocity zero stops the note)
```

The `n` command positions the pitch, `l` makes it sound.

## Time Travel with Defer

The `+` symbol schedules commands to happen later, measured in musical time (quarter notes):

```
v0 n60 l1 +1 v0 n62 l1 +1 v0 n64 l1 +1 v0 n65 l1
```

This plays middle C now, then D a quarter note later, then E, then F. You're writing a melody in time.

The semicolon `;` marks the end of a thought - execute everything before it, then reset for the next line. It also resets the defer "anchor time" to "now".


## Variables for Live Tweaking

Ten variables (`$0` through `$9`) let you store values you'll reuse or want to change on the fly:

```
=0 440        store 440 in variable 0
v0 f$0        use variable 0 as frequency
v1 f$0        another voice at the same frequency

=0 880        change the variable
v0 f$0        now both voices update to 880
```

## Rhythm in Practice

Here's a complete patch. Notice the pattern:

```
v0 w1 f440 a0.5 t0.01,0.1,0.7,0.2
```

Voice zero, wave one, frequency 440, amplitude 0.5, envelope with fast attack and release. It flows. You can type it without thinking once you've done it a few times.

Or go even more compact:

```
v0w1f440a0.5t0.01,0.1,0.7,0.2
```

Same thing. Your fingers just dance across the keys.

## Building Sequences

The sequencer commands have their own rhythm. Remember that the string (what to play) comes before the step assignment:

```
y0 # select pattern 0
{v0 n60 l1} x0
{v0 n62 l1} x1
# step 0 plays C
# step 1 plays D
{v0 n64 l1} x2
{v0 n65 l1} x3
# step 2 plays E
# step 3 plays F
%4 # set pattern modulus to 4 (step every 4 clocks)
z1 # start playing
```

The `%` command tells the pattern how many clocks to count before going to the next step
Or compress it:

```
y0;{v0 n60 l1}x0{v0 n62 l1}x1{v0 n64 l1}x2{v0 n65 l1}x3%4;z1
```

## Why It Works

The language isn't trying to be readable like prose. It's trying to be **typeable** - fast, rhythmic, memorable. Commands are short because you'll type them hundreds of times in a session. The grammar is flexible because when you're performing live, you don't want to fight syntax.

The alternating pattern of letters and numbers creates a natural cadence. After a while, you stop translating in your head. You just hear a sound you want, and your fingers know how to make it happen.

Think of it like playing an instrument: the first time you learn a chord, you think about where each finger goes. Eventually, you just think "C major" and your hand forms the shape. Skode works the same way - it becomes muscle memory.

---

## Quick Reference

### Voice Control
| Command | Args | Description | Example |
|---------|------|-------------|---------|
| `v` | voice | Select voice (0-31) | `v0` |
| `w` | wave | Set waveform | `w1` |
| `f` | hz | Set frequency in Hz | `f440` |
| `n` | note | Set MIDI note (0-127) | `n60` |
| `l` | velocity | Trigger with velocity (0-1) | `l1` |
| `a` | amp | Set amplitude (0-1) | `a0.5` |
| `p` | pan | Set pan (-1 to 1) | `p-0.3` |
| `t` | a,d,s,r | Set ADSR envelope | `t0.01,0.1,0.7,0.2` |
| `T` | - | Re-trigger note | `T` |
| `S` | voice | Reset voice to defaults | `S0` |
| `/` | - | Reset to default frequency | `/` |
| `?` | - | Show current voice | `?` |
| `??` | - | Show all active voices | `??` |

### Modulation
| Command | Args | Description | Example |
|---------|------|-------------|---------|
| `A` | osc,depth | Amplitude modulation | `A1,0.5` |
| `F` | osc,depth | Frequency modulation (FM) | `F2,100` |
| `P` | osc,depth | Pan modulation | `P3,1.0` |
| `C` | osc,depth | CZ distortion modulation | `C4,0.8` |

### Filter
| Command | Args | Description | Example |
|---------|------|-------------|---------|
| `J` | mode | Filter mode (0=off, 1=LP, 2=HP, 3=BP, 4=notch, 5=AP) | `J1` |
| `K` | freq | Filter cutoff frequency | `K1000` |
| `Q` | res | Filter resonance | `Q2.0` |

### Effects
| Command | Args | Description | Example |
|---------|------|-------------|---------|
| `q` | bits | Bit quantization (1-16) | `q8` |
| `h` | rate | Sample & hold rate | `h100` |
| `c` | mode,dist | CZ phase distortion | `c3,0.5` |
| `s` | smooth | Amplitude smoothing | `s0.02` |
| `g` | speed | Glissando/portamento | `g0.1` |

### Sequencer
| Command | Args | Description | Example |
|---------|------|-------------|---------|
| `y` | pattern | Select pattern (0-15) | `y0` |
| `x` | step | Set step number | `x0` |
| `z` | state | Start/stop pattern (0/1) | `z1` |
| `Z` | state | Start/stop all patterns | `Z1` |
| `%` | steps | Set pattern modulus | `%16` |
| `M` | bpm | Set tempo (quarter notes) | `M120` |
| `!` | step | Unmute step | `!5` |
| `@` | step | Mute step | `@5` |

### Voice Linking
| Command | Args | Description | Example |
|---------|------|-------------|---------|
| `G` | v1,v2 | Link MIDI note to voices | `G1,2` |
| `H` | v1,v2 | Link velocity to voices | `H3,4` |
| `L` | voice | Link trigger to voice | `L5` |

### Special
| Command | Args | Description | Example |
|---------|------|-------------|---------|
| `N` | semi | MIDI transpose | `N12` |
| `b` | dir | Playback direction (0/1) | `b1` |
| `B` | state | Enable looping | `B1` |
| `m` | state | Mute voice | `m1` |
| `r` | state | Record voice to wav | `r1` |
| `>` | voice | Copy current voice to target | `>5` |
| `V` | vol | Master volume | `V0.8` |

### Variables
| Command | Args | Description | Example |
|---------|------|-------------|---------|
| `=` | var,val | Set variable ($0-$9) | `=0 440` |
| `$` | var | Recall variable | `f$0` |

### System
| Command | Args | Description | Example |
|---------|------|-------------|---------|
| `/l` | n | Load patch n.sk | `/l5` |
| `/w` | n,slot,ch | Load n.wav to slot | `/w10,200,0` |
| `/s` | - | Show system stats | `/s` |
| `/t` | - | Toggle trace mode | `/t` |
| `/v` | - | Toggle verbose mode | `/v` |
| `/q` | - | Quit | `/q` |
| `<` | sec | Start recording | `<10` |
| `*` | - | Save recording | `*` |

### Timing
| Command | Args | Description | Example |
|---------|------|-------------|---------|
| `+` | beats | Schedule command (quarter notes) | `+1 n62` |
| `;` | - | End chunk / execute | `;` |

---

## Examples

### Simple Tone
```
v0 w1 f440 a0.5 l1
```

### FM Synthesis

```
v0 w0 f5 a1 m1 l1     # LFO (5 Hz, muted)
v1 w1 f440 a0.5 F0,100  # Carrier with FM
v1 l1                 # Trigger
```

### Drum Pattern

```
v0 w101 a20 # set v0 to kick
v1 w102 a20 # set v1 to snare
y0
{v0 l1} x0         # kick
{v1 l0.5} x1       # snare
{v0 l0.8} x2       # kick
{v1 l0.5} x3       # snare
z1                 # 4-step loop, start
```

### Melody with Envelope

```
v0 w1 t0.01,0.1,0.7,0.2   # Setup voice with envelope
v0 n60 l1; +1 n62 l1; +1 n64 l1; +1 n65 l1  # C D E F
```

### Filtered Bass

```
v0 w2 f110 a0.8           # Saw wave, low frequency
v0 J1 K300 Q3.0           # Lowpass, resonant
v0 t0.001,0.1,0.5,0.2     # Punchy envelope
v0 l1                     # Trigger
```

### Live Coding Session

```
# Setup voices
v0w1a0.5t0.01,0.1,0.7,0.2
v1w2a0.3t0.001,0.05,0.6,0.1

# Create pattern
y0
{v0n60l1}x0{v1n36l1}x1{v0n62l1}x2{v1n36l0.8}x3
{v0n64l1}x4{v1n36l1}x5{v0n62l1}x6{v1n36l0.8}x7
%8;M120;z1

# Tweak while playing
v0 K500                   # Change filter
v1 F0,50                  # Add FM to bass
M140                      # Speed up tempo
```

---

## Architecture

### Synthesis Engine (`synth.c`)
- 64-voice polyphonic wavetable synthesis
- Per-voice oscillators with phase accumulation
- Multi-mode biquad filters
- ADSR envelopes
- Modular routing matrix

### Command Parser (`skode.c`)
- State machine parser
- Character-by-character processing
- Accumulator-based argument collection
- Callback-driven execution

### Command Router (`wire.c`)
- Maps parsed commands to synth functions
- Manages voice context
- Handles scheduling and patterns
- File I/O for patches and samples

### Sequencer (`seq.c`)
- 16 patterns, 256 steps each
- Sample-accurate timing
- Per-step muting
- Tempo control

---

## Advanced Features

### Wavetable Loading

Load external .wav files into the synthesizer:

```
/w 10 200 0    # Load 10.wav into slot 200, left channel
/w 11 201 1    # Load 11.wav into slot 201, right channel
/w 12 202 -1   # Load 12.wav into slot 202, mix to mono
```

### Patch Management

Save and load complete voice configurations:

```
# Create 5.sk with your patch
v0 w1 f440 a0.5 t0.01,0.1,0.7,0.2

# Later, load it back
/l 5
```

### Recording

Record individual voices or entire mix:

```
v0 r1          # Enable recording for voice 0
v1 r1          # Enable recording for voice 1
<10            # Record for 10 seconds
# ... play something ...
*              # Save to timestamped .wav file
```

### UDP Control

Send commands over UDP for network control:

```bash
echo "v0 n60 l1;" | nc -u localhost 9999
```

---

## Tips & Tricks

### Muscle Memory Shortcuts

Common voice setup becomes one gesture:

```
v0w1f440a0.5    # Voice, wave, freq, amp - no spaces needed
```

### Variable Tricks

Use variables for quick parameter exploration:

```
=0 500         # Store cutoff frequency
v0 K$0         # Apply to filter
=0 1000        # Change it
v0 K$0         # Re-apply new value
```

### Copy Voice Settings

Set up one voice perfectly, then duplicate:

```
v0 w1 f440 a0.5 t0.01,0.1,0.7,0.2 J1 K1000 Q2.0
v0 > 1         # Copy voice 0 to voice 1
v1 f880        # Just change the frequency
```

### Defer Chains

Create rhythmic patterns with deferred commands:

```
v0 n60 l1; +0.5 l0; +0.5 n62 l1; +0.5 l0
```

### Pattern Variations

Build variations by muting steps:

```
y0 %16 z1      # Base pattern playing
@8 @12         # Mute steps 8 and 12
!8             # Unmute step 8
```

---

## Troubleshooting

### No Sound
- Check master volume: `V1`
- Check voice amplitude: `v0 a1`
- Verify you're triggering: `v0 l1`
- Check if voice is muted: `v0 m0`

### Clicks/Pops
- Increase envelope attack: `t0.01,0.1,0.7,0.2`
- Enable amplitude smoothing: `s0.02`

### CPU Usage High
- Reduce active voices
- Simplify modulation routing
- Lower pattern complexity

### Commands Not Working
- Enable trace mode: `/t0`
- Verify voice is selected: `v0`

---

## Contributing

Contributions welcome! Please:

1. Fork the repository
2. Create a feature branch
3. Make your changes
4. Add tests if applicable
5. Submit a pull request

## License

[Your License Here]

## Credits

- `skred` synthesizer by octetta / Joseph Stewart
- AMY by xxx provided inspiration for `skode` via their *wire protocol*
- Retro waveform data fr
- miniaudio library

## Links

- [GitHub Repository](https://github.com/octetta/skred)
- [Discord/Forum](https://discord.gg/octetta)

---

**Happy live coding!** 🎵
