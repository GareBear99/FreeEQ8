# Changelog

All notable changes to FreeEQ8 will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

## [2.2.0] — 2026-04-23

### Added
- **Audit regression test suite** (`Tests/AuditRegressionTest.cpp`) proving
  the triple-buffer SPSC invariants for `SpectrumFIFO` and
  `LinearPhaseEngine` under concurrent stress (0 tears across ~600M
  samples per run).
- **Audit micro-benchmarks** (`Tests/AuditBench.cpp`) reporting
  push/consume/chunk/pool/kernel-handoff timings.
- **Milestone-A report** (`docs/MILESTONE_A_REPORT.md`) documenting every
  change with math, benchmarks, and sonic-impact analysis.

### Changed — real-time safety & correctness (Milestone A)
- **A1**: oversamplers are pre-built in `prepareToPlay()` and looked up
  from a `std::array<unique_ptr<Oversampling>, 3>` in `processBlock()`.
  Order changes call only `Oversampling::reset()` (non-allocating).
  Eliminates heap allocation on the audio thread.
- **A2**: editor modal dialogs own a `std::unique_ptr<AlertWindow>`
  (ends the latent double-free with `deleteWhenDismissed = true` +
  `delete dlg` in callback). Background HTTP callbacks are guarded by
  `juce::WeakReference<FreeEQ8AudioProcessorEditor>`; closing the editor
  mid-activation no longer dereferences a dangling `this`.
- **A3**: `MatchEQ::applyCorrection` now chunks blocks >4096 samples
  into `<= fftSize/2` pieces instead of silently returning. Output is
  bit-identical to previous code for `n ≤ 2048`; the
  previously-dropped large-block case is now handled correctly.
- **A4**: `SpectrumFIFO` upgraded from a racey single-ring-buffer to a
  canonical swap-chain triple buffer (`writeSlot`/`midSlot`/`readSlot`
  permutation of {0,1,2}). Audio thread never writes to the slot the UI
  thread is reading.
- **A5**: linear-phase FIR rebuild moved off the audio thread. A
  dedicated `juce::Thread` worker (`LinPhaseRebuildThread`) is parked on
  `wait(-1)` and drains the `linPhaseDirty` flag on `notify()`.
  `LinearPhaseEngine` kernel handoff upgraded to the same swap-chain
  triple buffer.
- **A7**: `getTailLengthSeconds()` now returns `max(linPhaseTail,
  matchActive ? MatchEQ::fftSize/sr : 0)` so offline renders don't
  truncate the match-EQ overlap-add tail.
- **Demo cadence** (ProEQ8 only, unactivated): now 2 minutes of clean
  playback followed by 30 seconds of mute (was 4:30 / 30 s). Full
  activation removes the mute entirely.

### Notes
- Net audio-path impact: **none** for A2/A4/A6/A7; strictly additive
  correctness for A1/A3/A5.
- Cold-start for linear-phase mode: the engine pass-throughs audio for
  the (~sub-millisecond) window between `prepareToPlay()` and the
  background thread's first published kernel, instead of applying a
  zero-kernel.
- All seven Milestone-A items verified via full builds of
  `FreeEQ8_All` + `ProEQ8_All` (VST3 + AU + Standalone) with zero
  errors and zero warnings.

## [2.0.0] — 2026-03-25

### Added
- **Online license activation** — 2-device-per-purchase enforcement via Cloudflare Worker + KV
- **Device fingerprinting** — stable SHA-256 device ID (macOS hardware UUID, Windows MachineGuid, Linux machine-id)
- **`/activate` endpoint** — server-side activation with device tracking (max 2 per license)
- **`/deactivate` endpoint** — release a device slot for transfer to a new machine
- **Webhook idempotency** — duplicate Stripe `checkout.session.completed` events are detected via KV
- **Timing-safe comparisons** — all HMAC signature checks use constant-time XOR comparison
- **`ActivationResult` enum** — typed activation results with user-friendly error messages
- **Async activation dialog** — network call runs on background thread, shows "Activating..." state
- **Integration test suite** — 13 tests / 35 assertions for the activation server (`server/test-activation.js`)

### Fixed
- **Spectrum analyzer blank in Reaper** (#10) — reset spectrum FIFOs in `prepareToPlay()` so analyzer recovers after offline/online cycle
- **SpectrumFIFO thread safety** — `fifoWriteIndex` changed from `int` to `std::atomic<int>`
- **Linear phase silently disables features** (#12) — drive, dynamic EQ, M/S, oversampling, and saturation mode controls are now greyed out when linear phase is active
- **CORS preflight** — `/activate` and `/deactivate` handle OPTIONS requests properly

### Changed
- `LicenseValidator::activate()` now returns `ActivationResult` (was `bool`), performs online activation
- `LicenseValidator::deactivate()` now POSTs to server to release device slot
- License payload includes `license_id` field linking to KV activation record
- ProEQ8 CMake target enables `JUCE_USE_CURL=1` for HTTP support
- License email now mentions 2-device limit and deactivation instructions
- Server version bumped to 2.0.0

## [1.1.0] — 2026-03-13

### Added
- **ProEQ8 commercial target** — 24-band parametric EQ built from same source with `PROEQ8=1` compile flag
- **4 saturation modes** (Pro) — Tanh, Tube, Tape, Transistor per band via `sat_mode` parameter
- **A/B comparison** (Pro) — instant snapshot toggle with Copy A→B / B→A
- **Auto-gain bypass** — RMS-matched loudness compensation for honest A/B listening
- **Piano roll overlay** (Pro) — C1–C8 musical note reference lines on response curve
- **Collision detection** (Pro) — amber warning rings when bands overlap within 1/3 octave
- **Update checker** — background thread checks GitHub releases API, shows blue banner in editor
- **License validator** (Pro) — offline HMAC-SHA256-signed keys, activation dialog, demo mode (mute 30s/5min)
- **Stripe webhook server** — Cloudflare Worker for Stripe checkout → license key generation → email via Resend
- **30 factory presets** — genre-specific starting points (Vocal Clarity, Kick Punch, Acoustic Guitar, Bass DI, Drum Bus, De-Harsh, Broadcast Voice, EDM Sub Bass, Lo-Fi, Mix Bus Sweetener, and more)
- **Bandpass filter type** — available in all bands alongside Bell, LowShelf, HighShelf, HighPass, LowPass
- **Tooltips** for every knob, button, and combo box (`juce::TooltipWindow`)
- **Biquad unit tests** — standalone test verifying RBJ cookbook coefficients for all 6 filter types at 44.1/48/96 kHz (`Tests/BiquadTest.cpp`)
- **Linux build support** — `build_linux.sh` script with automatic dependency install; Linux job in CI
- **STRIPE_SETUP.md** — complete deployment guide for ProEQ8 Stripe integration

### Fixed
- **Spectrum analyzer not showing audio** — smoothing logic used `std::max` with zero-initialized buffer vs. negative-dB values; fixed init to -100 dB + subtractive decay
- **MatchEQ thread safety** — `fifoIndex`, `captureFrames`, `analyzeFrames` are now `std::atomic`; disable-reset-enable pattern on capture/apply
- **State restoration** — `initLinkTracking()` + latency re-sync now called after `replaceState()`
- **Audio-thread heap allocation** — `std::vector<float> magDb` in `buildLinearPhaseMagnitude()` replaced with pre-allocated member
- **Linear phase FIR rebuilt every block** (#11) — added `std::atomic<bool> linPhaseDirty` flag; FIR only rebuilt when EQ params change
- **Factory presets incomplete** — all 15 per-band params now set (was missing solo/slope/ch/link/drive/dyn)
- **Factory preset OOB access** — fixed for ProEQ8's 24-band layout (bands 9-24 get safe defaults)
- **`getTailLengthSeconds()`** — returns `firLength / sampleRate` when linear phase active (was 0)
- **Missing brace in freq link propagation** — for-loop body had only first `if` in scope
- **Missing typeChoices assignment** — `auto typeChoices` had no initializer, would not compile
- **HMAC verification alignment** — LicenseValidator.h now uses full RFC 2104 HMAC-SHA256 via `juce::SHA256`

### Changed
- Replaced fragile `#ifndef M_PI / #define M_PI` with `constexpr double kPi` in `Biquad.h` and `LinearPhaseEngine.h`
- Expanded parameter listeners to include on/type/slope/scale/adaptive_q for dirty-flag + linking
- Added `juce_cryptography` to link libraries for both CMake targets
- Preset directory now uses `kProductName` (FreeEQ8 or ProEQ8) instead of hardcoded path
- Build scripts and CI updated to build both FreeEQ8 and ProEQ8 targets
- Updated README with ProEQ8 comparison, corrected preset counts, expanded project structure

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
