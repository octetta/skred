# sk8r
<img src="../src/sk8r/icon.svg" width="100">

realtime parameter sliders

# sk8-pad
<img src="../src/sk8-pad/icon.svg" width="100">

flexible drum machine like pads

# midi-sk8
<img src="../src/midi-sk8/icon.svg" width="100">

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

