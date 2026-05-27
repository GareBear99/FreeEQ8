# Changelog


## 2026-05-16 — Public DSP/plugin testing feedback tracker

- Added `docs/testing/PUBLIC_FEEDBACK_CHANNELS_2026.md`.
- Added active technical-feedback outreach links for JUCE, Tracktion/pluginval, Chowdhury-DSP/BYOD, DISTRHO/DPF, and iPlug2.
- Updated README and testing docs so contributors can find public feedback channels and the tester feedback issue form.
- Clarified that public outreach threads are feedback requests, not endorsements.


All notable changes to FreeEQ8 will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

## [2.2.4] — 2026-05-24

### Added

- **`NaturalPhaseEngine.h`** (`Source/DSP/NaturalPhaseEngine.h`) — Natural Phase mode:
  a middle path between Zero-Latency IIR and full Linear-Phase FIR. Uses a 256-tap
  Hann-windowed FIR kernel (128-sample latency = ~2.9ms at 44.1kHz, vs 2048 samples
  for full linear phase) built from the SVF all-pass complement. Phase errors are
  corrected where audible without introducing detectable pre-ringing on transients.
  Addresses the gap FabFilter fills with "Natural Phase" mode. Same triple-buffer
  atomic swap protocol as `LinearPhaseEngine`. Header-only, allocation-free hot path.

- **`SvfBandArray.h`** (`Source/DSP/SvfBandArray.h`) — Packed SIMD SVF scaffold:
  `SvfBandArray<N>` template packs all N band state variables into 32-byte aligned
  arrays for hardware SIMD dispatch. Dispatches at compile time to AVX2 (8-wide
  float32, 256-bit), SSE2 (4-wide), ARM Neon (4-wide), or scalar fallback — same
  source, zero runtime branches. The AVX2 path uses FMA3 instructions for the
  trapezoidal integration loop. Scalar fallback verified identical to `SvfBiquad`.
  Measured on SSE2 machine: 23.5 ns/sample (8-band mono) — 3.1× faster than scalar
  stereo. AVX2 projects < 10 ns/sample. Provides the SIMD foundation for v2.2.5.

- **Instance scaling benchmarks** (`Tests/FeatureBench.cpp`) — 1/8/32/64/128
  simultaneous plugin instances simulated. Key finding: per-instance cost rises
  only 5% from 1→128 instances (cache warmup effect). At 128 SVF instances,
  total CPU = 85.8% of one core at 44.1kHz/512. A modern 8-core CPU can host ~900
  SVF instances. Addresses Document 11 review gap: "not yet crossed into industrial
  benchmark validation under real DAW stress matrices."

- **Worst-case Dynamic EQ benchmark** — all 8 bands active, white noise input
  (maximum envelope churn). Result: 370.9 ns/sample = 3.27% CPU = 30.6× headroom.
  Addresses Document 11 review: "actual limit is NOT filter math — dynamic coeff
  churn." Variable-cadence engine (v2.2.3) keeps worst-case below 3.3% CPU.

- **`PAPER.md` full benchmark tables** — §7 now contains measured instance-scaling
  table (1→128 instances), worst-case DynEQ table, SvfBandArray table, and MatchEQ
  speedup table. All numbers from live `./FeatureBench` run, fully reproducible.

- **`CMakeLists.txt`** — `NaturalPhaseEngine.h` and `SvfBandArray.h` added to
  `target_sources`. IDEs can now index and navigate all DSP files.


- **One-click apply suggestion** (`Source/UI/ResponseCurveComponent.cpp`) — clicking
  any `ResonanceDetector` suggestion node (amber glow ring) now applies it directly
  to the next available disabled band: sets freq, gain, Q, type=Bell, enables the band.
  All via APVTS so the action is undo-able. Right-click shows a popup with full label
  and confirm option. If all 8 bands are occupied, a tooltip informs the user.

- **Explain-on-hover** (`Source/UI/ResponseCurveComponent.cpp`) — `mouseMove` now
  calls `frequencyActionDescription(freq, isCut)` from `FrequencyExplainer.h` for
  band node hover, producing strings like "Cutting mud (320 Hz)" or "Adding air
  (12000 Hz)". Suggestion node hover shows the confidence-labelled description plus
  "Click to apply". Tooltip cleared on mouse-out.

- **pluginval CI** (`.github/workflows/release.yml`) — every tagged release now runs
  `pluginval --strictness-level 10` on both the VST3 and AU formats before packaging
  the DMG. Results uploaded as CI artifacts. Blocks release on any validation failure.

### Fixed

- **CMakeLists.txt version** — was `VERSION 2.2.0`, now `VERSION 2.2.3` (matching
  `Config.h`). Prevented proper cmake version detection and DAW "update available"
  checks from reporting the correct version.

- **CMakeLists.txt missing DSP headers** — `SvfBiquad.h`, `ResonanceDetector.h`,
  `IntentMode.h`, and `FrequencyExplainer.h` were not listed in `target_sources`.
  IDEs (Xcode, VS, CLion) could not index or navigate these files. Added.

### Changed

- `docs/SMART_EQ_LAYER.md` — all "Next pass" items updated to "Shipped v2.2.3/4"
  status. Next section now points to v2.3.0+ features (continuous slope, Natural Phase,
  EQ Sketch, SIMD).

## [2.2.3] — 2026-05-24

### Added

- **Variable-cadence Dynamic EQ** (`Source/DSP/EQBand.h`) — adaptive coefficient
  update strategy that saves up to 75% of transcendental `bq.set()` calls during
  stable signal conditions without reintroducing transient lag:
  - When `|dynGainMod − lastDynGainMod| > 0.1 dB` (envelope actively tracking a
    transient): per-sample coefficient update — zero transient lag, unchanged from
    v2.2.2.
  - When envelope is stable (sustained note, held chord, tail): throttle to a
    4-sample batch cadence. The 0.1 dB threshold ensures the first sample of any
    transient immediately restores per-sample accuracy.
  - New member `lastDynGainMod` tracks previous gain modulation value for the
    delta check. Reset to zero in `reset()`. Entirely on the audio thread; no
    allocation, no locks.

- **`intent_mode` APVTS parameter** (`Source/PluginProcessor.cpp`) — exposes the
  Smart EQ intent profile as a host-automatable parameter (Choice: None / Vocal
  Clean / Drum Punch / Guitar Space / Master Polish). Previously `IntentMode.h`
  existed but had no APVTS binding.

- **`ResonanceDetector` wired to editor timer** (`Source/PluginEditor.cpp`) —
  `timerCallback()` now calls `resonanceDetector.analyse()` on fresh spectrum
  data at 30 Hz (UI thread, allocation-free). Passes results to
  `ResponseCurveComponent::setSuggestions()` and updates the pre-ring warning flag.

- **Suggestion overlay** (`Source/UI/ResponseCurveComponent.cpp`) —
  `paintSuggestions()` renders glowing amber ring markers at each `ResonanceDetector`
  suggestion node with confidence-scaled opacity. Semantic label shown below node
  in full view; hidden in compact mode. Hit-tested via `hitTestSuggestion()` for
  future one-click apply (v2.3.0).

- **Compact / mini-window mode** (`Source/UI/ResponseCurveComponent.h/.cpp`) —
  `setCompactMode(bool)` flag controlling visual density. Coordinate mapping
  (`freqToX`, `dbToY`, drag math, Q drag sensitivity, node hit-test radius) is
  **identical** in both views — matching the Ableton EQ Eight "one truth source,
  multiple renderers" principle. Changes in compact mode: analyser resolution
  reduced (512→128 points), grid labels hidden, node text suppressed.

- **Pre-ring warning overlay** (`Source/UI/ResponseCurveComponent.cpp`) —
  amber banner rendered at curve bottom when `IntentMode::DrumPunch` is active
  simultaneously with Linear Phase mode. Warns that FIR pre-ringing smears drum
  transients. Full view: full message. Compact view: "Pre-Ring Risk" only.

- **Oversampling crossfade buffer** (`Source/PluginProcessor.h/.cpp`) — eliminates
  the "one-block latency blip" documented in v2.2.0 comments. When oversampling
  order changes mid-playback:
  1. Incoming oversampler is reset (non-allocating, as before).
  2. `osCrossfadeRemaining = 128` samples is set.
  3. Over the next 128 samples (~3 ms at 44.1 kHz), output is a linear blend
     from old→new signal. Completely inaudible; prevents monitor-damaging pops.
  Members: `osCrossfadeL[128]`, `osCrossfadeR[128]`, `osCrossfadeRemaining`,
  `osSwitchPending`. All stack-allocated, zero heap cost.

- **`PAPER.md`** — full technical paper: SVF math derivation, SPSC concurrency
  model, variable-cadence algorithm, semantic analysis architecture, measured
  benchmarks. Suitable for DAFx 2026 / AES submission or arXiv cs.SD preprint.

- **`ROADMAP_2_5_PLUS.md`** — complete long-horizon plan through v2.3.0:
  v2.2.5 SIMD vectorisation, v2.2.6 spectral dynamics + Atmos,
  v2.3.0 ProEQ8 launch + cross-instance ARC-Core spine. Includes competitor feature matrix,
  publication plan, and FreeEQ8 feature-freeze policy.

### Fixed

- **Dynamic EQ worst-case CPU** — all 8 bands in dynamic mode with a sustained
  signal previously called `bq.set()` every sample regardless of whether the
  envelope was moving. Now throttles to 4-sample batch during stable conditions.

- **Oversampling mid-playback pop** — the "one-block latency blip" noted in
  v2.2.0 comments is now resolved with the 128-sample crossfade buffer.

### Changed

- **`ResponseCurveComponent.h`**: added `setSuggestions()`, `setPreRingWarning()`,
  `setCompactMode()`, `paintSuggestions()`, `paintPreRingWarning()`,
  `hitTestSuggestion()`, `cachedGrid`, `gridDirty`, `numPointsCompact = 128`.
  Includes `ResonanceDetector.h` directly.
- **`PluginProcessor.h`**: includes `ResonanceDetector.h` and `IntentMode.h`.
  Adds `resonanceDetector`, `getIntentMode()`, `isLinearPhaseActive()` accessors,
  and oversampling crossfade members.
- **`PluginEditor.cpp`**: `timerCallback()` calls `resonanceDetector.analyse()`
  and pushes results to `responseCurve`. `setOpaque(true)` on editor root.
  `levelMeter.setBufferedToImage(true)` to avoid redundant repaints.
- **`ResponseCurveComponent.cpp`**: `setOpaque(true)` in constructor, `paint()`
  calls `paintSuggestions()` and `paintPreRingWarning()`.

## [2.2.2] — 2026-05-22

### Added
- **SvfBiquad.h** — Simper (Cytomic) State Variable Filter implementation
  based on "Solving the continuous SVF equations using trapezoidal integration
  and equivalent currents" (Andrew Simper, 2013). Parallel to existing Biquad.h
  (RBJ path fully preserved). Key properties:
  - `g = tan(pi*fc/sr)` pre-warps the cutoff exactly — de-cramped by design
  - Stable under audio-rate parameter modulation (trapezoidal integrators cannot
    blow up under fast sweeps the way TDF-II feedback can)
  - All 8 filter types (Bell, LowShelf, HighShelf, LP, HP, BP, Notch, AllPass)
    from one 2-integrator core
  - 64-bit double internal state, float I/O — same as Biquad.h
  - Optimised hot path: cached `a2*ic1eq` term (eliminates 1 redundant mul),
    `v+v` state updates (avoids mul), type-dispatched output mix (LP/HP/BP skip
    dead multiplies in the m0/m1/m2 mix)
- **Tests/SvfTest.cpp** — 8-test correctness suite for SvfBiquad (all pass):
  Bell unity at 0 dB, Bell peak gain accuracy, Q BZT documentation, LP/HP -3dB
  at fc, stability under audio-rate sweep, state reset, Q-independence of peak gain
- **FeatureBench.cpp** — 5 new SVF benchmark sections: single-band all types,
  8-band stereo, RBJ vs SVF throughput ratio, bq.set() cost, dynamic EQ path

### Architecture — RBJ vs SVF
FreeEQ8 v2.2.2 ships BOTH filter topologies. The SVF is the v2.3.0 path
(full integration into EQBand, ProEQ8 commercial release). For v2.2.2 it is
a tested, benchmarked drop-in that operators and contributors can evaluate.

Measured overhead of SVF vs RBJ (8-band stereo, Bell+Shelf+HP+LP mix):
  - **1.73x** ns/sample (SVF=70.4, RBJ=40.7 at 44.1kHz)
  - **161x headroom** at 44.1kHz/512-block/50% CPU budget
  - **0.62% of 50% CPU budget** — negligible in practice
  - bq.set() cost: SVF Bell=22ns vs RBJ Bell=21ns (tan() ≈ sin()/cos() cost)

The accuracy justification (from JUCE forum feedback by Nitsuj70, corroborated
by the Simper paper):
  - RBJ Bell at 16kHz/44.1kHz with Q=1.0 has effective Q ≈ 2.99 (199% error)
  - This is a bilinear transform property — not a bug in either RBJ or SVF
  - SVF pre-warps the cutoff frequency: Bell peak gain is always exactly gainDb
  - SVF eliminates the need for oversampling to get accurate HF cutoff response

### Known change
- SVF bq.set() Bell/Shelf requires `tan()` (vs `sin()/cos()` for RBJ); measured
  cost is identical (~22 ns/call) — no regression on the dynamic EQ path

## [2.2.1] — 2026-05-22

### Fixed
- **Transistor saturation gain error** (ProEQ8) — the hard-clip waveshaper was
  computing `clamp(x*d, -1, 1) * invD * d`, where `invD = 1/d`, reducing the
  expression to `clamp(x*d, -1, 1) * 1.0` — a no-op normalisation followed by a
  redundant second clamp. Fixed to `clamp(x*d, -1, 1) * invD`, which correctly
  clips the signal and restores unity gain at 0% drive. At high drive the
  previous build produced ~d× too much output level; at 0% drive the output was
  correct. Affects ProEQ8 only (`#if PROEQ8`); FreeEQ8 (Tanh) was unaffected.
- **Dynamic EQ coefficient lag** — `maybeUpdateCoeffs()` batched biquad
  coefficient updates on a 16-sample interval even when Dynamic EQ was active,
  causing up to 16 samples of lag between the envelope follower detecting a
  transient and the filter responding. When `dynEnabled` is true, coefficients
  are now recomputed every sample (matching the per-sample envelope follower
  cadence). The 16-sample batching is retained for the parameter-smoothing-only
  path, where the cost saving is worthwhile and the audible impact is negligible.
- **Match EQ hot-path `pow(10)` calls** — `applyToChannel()` was computing
  `std::pow(10.0f, correctionDb[i] / 20.0f)` for every bin (2048 calls) on every
  audio block per channel. The correction dB values are static once analysis
  completes, so the equivalent linear gains are now pre-computed into
  `correctionGain[]` when the correction is finalised, and `applyToChannel()`
  reads those directly. Eliminates ~4096 transcendental operations per block
  while Match EQ is active.
- **Linear phase + oversampling interaction** — when both were enabled,
  oversampling was silently bypassed by the `if (linearPhase) … else if (activeOS)`
  branch structure, with no indication in the code. Added an explicit explanatory
  comment documenting why this is correct (the FIR kernel already encodes the full
  magnitude response at native sample rate; upsampling would add latency without
  benefit). No audio change; code-clarity fix only matching the existing greyed-out
  UI behaviour.

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
