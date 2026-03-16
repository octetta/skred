```
gcc -o ma-bench ma-bench.c -lm -lpthread
./ma-bench 10 # run each combo for 10 seconds
```

# Interpreting ma-bench Output

The output has the following columns per test:

| Column      | Meaning |
|-------------|---------|
| Format      | f32 or s16, sample format used |
| SampleRate  | Playback rate in Hz |
| Period      | Callback buffer size (frames) |
| Callbacks/sec | Number of callbacks per second |
| SineHz      | Sine wave frequency (for audible confirmation) |

Below each row, you’ll see:

- **exec** → Callback execution time (ms) per frame buffer:
  - min / max across all callbacks
  - histogram (UTF-8) shows frequency distribution of callback durations
- **jitter** → Deviation of callback start time from expected period
  - min / max
  - histogram shows consistency
- **DSP load avg** → Percent of time CPU spends in callback vs. buffer period
  - `DSP load = avg_exec_ms / period_ms * 100%`
  - Shows CPU overhead

### Interpreting Differences

1. **Machine / OS differences**
   - Faster CPUs → lower avg exec / lower DSP load
   - OS scheduling affects jitter, smaller buffers are more sensitive
2. **Buffer size**
   - Smaller buffers → more callbacks → higher callback count
   - Too small → higher jitter, audible crackle even if XRuns = 0
3. **Format**
   - f32 usually slightly cheaper than s16 because no int conversion is needed
   - s16 can introduce clipping if sine frequency is too high (fixed here)
4. **Histograms**
   - Wide spread → inconsistent callback times → potential audio glitches
   - Tall / narrow → consistent callback timing → smooth audio

### Tips

- **Use the sine tone** to confirm audio is actually outputting
- **Compare DSP load %** for different formats, rates, and periods
- **Look at jitter histogram** for timing stability
- **Longer test durations** reduce noise in avg/min/max
