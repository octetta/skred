skred has 64 oscillators (voices)

a voice has:

- waveform
  - table
    - 0 = sine
    - 1 = square
    - 2 = saw high to low
    - 3 = saw low to high
    - 4 = triangle
    - 5 = low periodic noise
    - 6 = high periodic noise
    - 32 to 62 = korg dw8000 waveforms
    - 100 to 166 = AMY's samples (drums, etc)
    - 200 to 999 = places for user loaded WAV files
  - loop?
  - direction (forward / backward)
- frequency
  - hz or midi note #
  - frequency modulator voice and depth
- amplitude
  - 0 to ???
  - amplitude modulator voice and depth
- amplitude envelope (ADSR)
- panning
  -1 left to 1 right
  - pan modulator voice and depth
- phase distortion (Casio CZ style)
  - method and distortion amount
  - cz modulator voice and depth
- multi-mode filter
  - mode
    - 0 = low pass
    - 1 = high pass
    - 2 = band pass
    - 3 = notch
    - 4 = all pass (broken)
  - cutoff in hz 0 to 44100 Hz
  - resonance (Q) 0 to 10.0 (0.707 is no res)
  - FUTURE MODULATION DESIRED BUT NOT IMPLEMENTED
- sample and hold
  - number of sample phase steps to skip
- bit crush
  - 0 to 31 = bits of resolution : 0 = full resolution
- link midi note number to one other voice
- link velocity to one other voice
- link trigger to one other voice

Anywhere a voice modulator is mentioned above is any other skred voice

Example : make a sine wave at 440Hz with volume 4

  v0 f440 a4

Now create a sine wave modulator at 1Hz with amplitude 1

  v1 m1 f1 a1

Now modulate the panning of v0 with v1 with a depth of 1

  v0 P1,1

You should now hear the 440 Hz sine wave moving from ear to ear one
time per second

Now create a sine wave frequency modulator at 5 Hz with amplitude of 1

  v2 m1 f5 a1

Now modulate the frequency of v0 with v2 with a depth of 10

  v0 F2,10

At any time you can change the parameters via a few things typed from
the keyboard, or any program that can send the same characters using
a UDP socket.

If you have an existing thing that can send parameters in realtime
(like a keyboard of controller of some kind), I can create an
adapter to convert the data coming out of those devices into
a skred parameter.

My YT videos show a simple GUI that turns slider changes into skred
messages. 

Next... load a sample and play it

Load the fish sample "wave206.wav" into slot 206

  :w206,206

Now attach slot 206 to voice 0 and set its amplitude to 20

  v0 w206 a20

Now play it

  v0 T

Now change its direction and play it

  v0 b1 T

Now change it back to forward and loop it

Now use the GUI to change the frequency live...

Now modulate the frequency like we did with the sine wave...

And change the modulator frequency with the GUI...

Now play it with a different frequency

  v0 f220 T
