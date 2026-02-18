<img src="docs/skred.svg" width="200">

# skred

*wavetables gone rogue — snap together voices like LEGO,
then poke them with short ASCII incantations for instant
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
install zig 0.13.0
zig build
./zig-out/bin/skred # or skred.exe on Windows 
v0 w0 f440 a0 l1    # start a 440Hz sine wave on voice 0
v1 m1 a0 f1 l1      # start a 1Hz modulator on voice 1
v0 F1,1             # use v1 modulator to change v0's frequency 
??                  # show the running voices
```

# behind the scenes

## components
- [synth](docs/synth.md) skred's sound engine
- [skode](docs/skode.md) the skred live code language
- [skode reference](docs/skode-ref.md) details about skode commands and values
- [tools](docs/tools.md) various tools for learning and controlling skred

----

## credits
- [miniaudio](https://github.com/mackron/miniaudio) *audio library by mackron*
- [bestline](https://github.com/jart/bestline) *command session by jart*

## inspiration
- [POKEY](https://en.wikipedia.org/wiki/POKEY)
- [SID](https://en.wikipedia.org/wiki/MOS_Technology_6581)
- [PureData](https://msp.ucsd.edu/) *make sounds with visual coding*
- [ChucK](https://chuck.cs.princeton.edu/) *music programming language with time at the center*
- [SonicPi](https://sonic-pi.net/) *a free open-source live coding environment*
- [AMY](https://github.com/shorepine/amy) *A high-performance fixed-point Music synthesizer librarY for microcontrollers* 

## heroes
- [Robert Moog](https://en.wikipedia.org/wiki/Robert_Moog)
- [Wendy Carlos](https://en.wikipedia.org/wiki/Wendy_Carlos)
- [Laurie Spiegel](https://en.wikipedia.org/wiki/Laurie_Spiegel)
- [Chuck Moore](https://en.wikipedia.org/wiki/Charles_H._Moore)
