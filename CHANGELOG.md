# Changelog

All notable changes to FreeEQ8 will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added
- **Bandpass filter type** — available in all 8 bands alongside Bell, LowShelf, HighShelf, HighPass, LowPass
- **Tooltips** for every knob, button, and combo box (`juce::TooltipWindow`)
- **Biquad unit tests** — standalone test verifying RBJ cookbook coefficients for all 6 filter types at 44.1/48/96 kHz (`Tests/BiquadTest.cpp`)
- **Linux build support** — `build_linux.sh` script with automatic dependency install; Linux job in CI
- **8 new factory presets**: Vocal Clarity, Kick Punch, Acoustic Guitar Warmth, Bass Guitar DI, Drum Bus Glue, De-Harsh, Broadcast Voice, Master Gentle Tilt (16 total)

### Fixed
- **Spectrum analyzer not showing audio** — smoothing logic used `std::max` with zero-initialized buffer vs. negative-dB values; fixed init to -100 dB + subtractive decay
- **MatchEQ thread safety** — `fifoIndex`, `captureFrames`, `analyzeFrames` are now `std::atomic`; disable-reset-enable pattern on capture/apply
- **State restoration** — `initLinkTracking()` + latency re-sync now called after `replaceState()`
- **Audio-thread heap allocation** — `std::vector<float> magDb` in `buildLinearPhaseMagnitude()` replaced with pre-allocated member
- **Linear phase FIR rebuilt every block** (#11) — added `std::atomic<bool> linPhaseDirty` flag; FIR only rebuilt when EQ params change
- **Factory presets incomplete** — all 15 per-band params now set (was missing solo/slope/ch/link/drive/dyn)
- **`getTailLengthSeconds()`** — returns `firLength / sampleRate` when linear phase active (was 0)

### Changed
- Replaced fragile `#ifndef M_PI / #define M_PI` with `constexpr double kPi` in `Biquad.h` and `LinearPhaseEngine.h`
- Expanded parameter listeners to include on/type/slope/scale/adaptive_q for dirty-flag + linking
- Updated CONTRIBUTING.md priority list to reflect implemented features
- Updated README: added Linux support, fixed macOS version badge, removed stale known issues

## [1.0.0] — 2026-02-25

### Added
- 8-band parametric EQ with VST3/AU support
- Linear phase mode (FIR convolution via overlap-add FFT, 2048-sample latency)
- Dynamic EQ per band (threshold, ratio, attack, release)
- Match EQ functionality (capture reference, compute & apply correction)
- Per-band drive/saturation (gain-compensated tanh waveshaper)
- Band linking (groups A/B with delta-based propagation)
- Undo/Redo system via juce::UndoManager
- Real-time spectrum analyzer (4096-pt FFT, Hann window, pre/post toggle)
- Interactive response curve with draggable band nodes
- Stereo level meter (peak hold + RMS)
- Multiple filter slopes (12/24/48 dB/oct)
- Mid/Side processing with per-band channel routing
- Oversampling (1x/2x/4x/8x)
- Adaptive Q (auto-widens Q with gain)
- Preset management (save/load/delete, factory presets)
- macOS and Windows build scripts
- CI/CD pipeline (GitHub Actions) for macOS + Windows releases
- Contributing guidelines and code of conduct

### Changed
- Expanded `.gitignore` for comprehensive C++/JUCE/CMake coverage

---

> **Note:** ProEQ8 ($20) is planned — 24 bands, 4 saturation modes, A/B comparison.
