<img src="docs/skred.svg" width="200">

# skred

*wavetables gone rogue — snap together voices like LEGO,
then poke 'em with cheeky ASCII spells for instant
sonic mischief*

## description

`skred` is a polyphonic wavetable synthesizer built for
flexibility and live performance.

Instead of fixed, hardwired signal paths, it lets you
freely interconnect voices in a modular playground —
route (nearly) anything to anything, and reshape sounds on the fly.

It skips ultra-polished features like pristine interpolation
in favor of raw responsiveness and real-time control.

You command it the same way everywhere: terse ASCII messages
sent over wire or air or typed directly into the console.

Simple pattern playback keeps the grooves rolling while you
twist the knobs — or the code.

In short: a lightweight, hackable synth that feels alive under
your fingers, whether you're performing live or scripting chaos
from a terminal.


# quick start

```
./skred       # or .\skred.exe on Windows 
v0w0f440a4l1  # start a 440Hz sine wave on voice 0
v1m1a1f1l1    # start a 1Hz modulator on voice 1
v0F1,1        # use v1 modulator to change v0' frequency 
??            # show the running voices
```

# behind the scenes

## components
- [synth](docs/synth.md) skred's sound engine
- [skode](docs/skode.md) the skred live code language
- [skode reference](docs/skode-ref.md) details about skode commands and values
- [seq](docs/seq.md) the skred pattern system
- [tools](docs/tools.md) various tools for learning and controlling skred


## seq

## build
```
make skred
```

## run
```
./skred
```
----

## credits
- miniaudio
- bestline

## inspiration
- Pokey, SID, AMY
- PureData
- ChucK
- SonicPi

## heroes
- Robert Moog
- Wendy Carlos
- Chuck Moore

## background

- needs asound on linux
  - sudo apt install libasound2-dev
