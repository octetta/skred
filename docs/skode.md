# `skode` - A Language for Live Synthesis Control

`skode` is a compact command language designed for controlling synthesizers in real-time. It's built around a simple rhythm: **commands followed by their values**, typed as fast as you can think.

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
v0 n60 l1; +1 n62 l1; +1 n64 l1; +1 n65 l1
```

This plays middle C now, then D a quarter note later, then E, then F. You're writing a melody in time.

The semicolon `;` marks the end of a thought - execute everything before it, then reset for the next line.

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
v0 w1 f440 a0.5 t0.01,0.1,0.7,0.2;
```

Voice zero, wave one, frequency 440, amplitude 0.5, envelope with fast attack and release. It flows. You can type it without thinking once you've done it a few times.

Or go even more compact:
```
v0w1f440a0.5t0.01,0.1,0.7,0.2;
```

Same thing. Your fingers just dance across the keys.

## Building Sequences

The sequencer commands have their own rhythm. Remember that the string (what to play) comes before the step assignment:

```
y0;                                   select pattern 0
{v0 n60 l1} x0 {v0 n62 l1} x1        step 0 plays C, step 1 plays D
{v0 n64 l1} x2 {v0 n65 l1} x3        step 2 plays E, step 3 plays F
%4;                                   set step modulus to 4 (loop every 4 steps)
z1;                                   start playing
```

The `%` command tells the pattern how many steps to count before wrapping back to the beginning. A modulus of 4 means steps 0, 1, 2, 3, then back to 0.

Or compress it:

```
y0;{v0 n60 l1}x0{v0 n62 l1}x1{v0 n64 l1}x2{v0 n65 l1}x3%4;z1;
```

## Why It Works

The language isn't trying to be readable like prose. It's trying to be **typeable** - fast, rhythmic, memorable. Commands are short because you'll type them hundreds of times in a session. The grammar is flexible because when you're performing live, you don't want to fight syntax.

The alternating pattern of letters and numbers creates a natural cadence. After a while, you stop translating in your head. You just hear a sound you want, and your fingers know how to make it happen.

Think of it like playing an instrument: the first time you learn a chord, you think about where each finger goes. Eventually, you just think "C major" and your hand forms the shape. Skode works the same way - it becomes muscle memory.