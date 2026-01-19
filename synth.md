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
