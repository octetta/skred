<img src="skred.svg" width="200">

# skred

*wavetables gone rogue — snap together voices like LEGO,
then poke 'em with cheeky ASCII spells for instant
sonic mischief*

## description

`skred` is a polyphonic wavetable synthesizer built for
flexibility and live performance.

Instead of fixed, hardwired signal paths, it lets you
freely interconnect voices in a modular playground —
route anything to anything, and reshape sounds on the fly.

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
- [synth](synth.md) skred's sound engine
- [skode](skode.md) the skred live code language
- [skode reference](skode-ref.md) details about skode commands and values
- [seq](seq.md) the skred pattern system


## seq

## build
```
make skred
```

## run
```
./skred
```

# sk8r
<img src="sk8r/icon.svg" width="100">

realtime parameter sliders

# sk8-pad
<img src="sk8-pad/icon.svg" width="100">

flexible drum machine like pads

# midi-sk8
<img src="midi-sk8/icon.svg" width="100">

flexible MIDI to `skode` for mapping NOTE ON/OFF and pitch bend events

----

# skred-o-scope

This is a real-time visualizer designed to give you a "living" view of your audio. It behaves like a classic laboratory oscilloscope, letting you see the shape, phase, and character of your sounds as they happen.

## The "Analog" Color Experience
The scope uses a special light-mixing trick to show you what’s happening in the stereo field:
* **Panned Left**: Your waveform appears **Yellow** (Red + Green).
* **Panned Right**: Your waveform appears **Cyan** (Blue + Green).
* **Centered (Mono)**: The colors stack and blend into a bright **Electric Green**.

This makes it easy to spot phase issues or see exactly how wide your sound is just by looking at the color. 



## Essential Controls

| Key | Category | Action |
| :--- | :--- | :--- |
| **`S`** | **View** | **Toggle Small Mode**: Snaps to a tiny corner view. Resets display for HiDPI sharpness. |
| **`1` / `2`** | **Height** | **Vertical Gain**: Makes the wave look "louder" or "quieter" on screen. |
| **`Left` / `Right`** | **Zoom** | **Timebase**: Zoom in for cycle details; zoom out for overall wave shape. |
| **`P`** | **Ghosting** | **Persistence**: Toggle trails that simulate phosphor "glow." |
| **`Up` / `Down`** | **Ghosting** | **Fade Speed**: Controls how long the "ghost" trails last. |
| **`X`** | **Trigger** | **Smart Lock**: Anchors the wave to keep the image steady (Hysteresis). |
| **`Z`** | **Trigger** | **Zero Cross**: Basic wave anchoring. |
| **`B`** | **Trigger** | **Free Run**: Disables anchoring for raw, chaotic movement. |

## Reading the Display

* **The Lock Indicator**: Located in the top-right box. If **LOCK: YES** is green, the scope has successfully "grabbed" the signal and the image will stay still.
* **FB (Heartbeat)**: A ticking counter that confirms the scope is receiving a live data stream from the audio engine.
* **Wave Envelopes**: The faint green outlines and dots show the absolute peaks of the signal over time.
* **Status Labels**: The text at the bottom reflects the current synth voice or debug messages from your instrument.

## Automatic Memory

The scope remembers your setup. It automatically saves your window position, vertical magnification, and horizontal zoom level to a local config file, so it opens exactly where you left it.

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
