# WindLab 0.2 test report

## Executed here

- Built `tests/smoke.cpp` with system C++17 compiler (`-O2 -Wall -Wextra -pedantic`).
- Passed DSP smoke test for six instruments at 44.1, 48, and 96 kHz.
- Confirmed staccato self-release, swell onset, CC11 expression and CC2 breath gain, signal bounds, extreme MIDI notes, sample-rate handling and bend stability.
- Built `tests/render-demo.cpp` with system C++17 compiler and rendered `Preview/WindLab_0.2_demo.wav`.

## Important not-yet-tested items

- JUCE 9.0.4 itself is **not installed here**; networking to the upstream source repository was unavailable.
- No AU, VST3, or Standalone plugin binary built or opened on macOS.
- No automated DAW compatibility tests in Ableton Live, Studio One, Logic, Cubase, FL Studio, Reaper, or Bitwig.
- CPU and alias spectra have not been comprehensively profiled; the harmonic system avoids naive non-bandlimited sawtooth partials, but this is not a guarantee of alias-free DSP under all settings.
- Host project restore/parameter automation and compatibility with previous prototype sessions still require real host validation.

## Repeat tests yourself

```bash
./tests/run-smoke.sh
./tests/render-demo.sh
./build-mac.sh
```

Optional macOS host validation: plugval (VST3/AU), Apple's `auval -v aumu Wl01 Wlab` only after checking the final 4-character type/subtype/manufacturer IDs, a short test in each target DAW, and manual listening/bouncing at 44.1/48/96 kHz.
