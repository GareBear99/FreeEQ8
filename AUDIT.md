# FreeEQ8 — Code Audit Report

> **Version:** 2.2.5 | **Date:** 2026-05-27 | **Auditor:** Internal (full source review)

This document records the findings of a full line-by-line source audit
covering restriction correctness, DSP correctness, threading safety, and
performance. It serves as a transparency record for users and contributors.

---

## 1. Restriction Audit — FreeEQ8 (Free Version)

**Result: FreeEQ8 has zero feature locks and zero nag interruptions during
real-time playback.** Offline export is limited to 4 minutes 30 seconds
(ProEQ8 removes this limit). ProEQ8 demo (unactivated) blocks export entirely
and applies a 2 min clean + 30 s mute cycle.

### How the restriction system works

The codebase supports two products built from the same source tree:

- `FreeEQ8` — compiled without `#define PROEQ8`; `kIsProVersion = false`
- `ProEQ8` — compiled with `#define PROEQ8`; `kIsProVersion = true`

All restriction logic is guarded by `kIsProVersion`. The `LicenseValidator`
class is present in both builds but its demo mute path exits immediately for
FreeEQ8:

```cpp
bool shouldMuteDemo(double sampleRate, int numSamples)
{
    if (activated.load()) return false;
    if (!kIsProVersion) return false;  // FreeEQ8 never mutes  ← hard exit
    // ... rest of demo logic never reached for FreeEQ8 ...
}
```

### What is gated behind `#if PROEQ8`

| Feature | Gated? | Notes |
|---------|--------|-------|
| Audio mute (demo mode) | YES — ProEQ8 unactivated only | FreeEQ8 guard confirmed |
| Saturation modes: Tube, Tape, Transistor | YES — ProEQ8 only | FreeEQ8 gets Tanh only |
| A/B comparison | YES — ProEQ8 only | |
| License activation UI | YES — ProEQ8 only | |
| `sat_mode` APVTS parameter | YES — ProEQ8 only | |

### What FreeEQ8 gets (no restrictions)

- All 8 bands
- All 6 filter types (Bell, LowShelf, HighShelf, HP, LP, Bandpass)
- All slope options (12/24/48 dB/oct)
- Dynamic EQ (full — threshold, ratio, attack, release)
- Linear phase mode (full — FIR rebuild, 2048-sample latency)
- Match EQ (full — capture, analyse, apply)
- Mid/Side processing (full — per-band routing)
- Oversampling (full — 1x/2x/4x/8x)
- Per-band drive (Tanh saturation)
- Band linking (A/B groups)
- Preset system
- Undo/redo
- Real-time spectrum analyser (pre/post toggle)
- Resonance detector / Intent mode

---

## 2. DSP Correctness Audit

### 2.1 Biquad implementation

**Rating: ✅ Correct**

- Form: Transposed Direct Form II — numerically stable, correct
- Precision: 64-bit double throughout; float I/O upcast before processing
- Coefficients: RBJ Audio EQ Cookbook formulas — verified against reference
  calculations in `Tests/BiquadTest.cpp` at 44.1, 48, and 96 kHz
- Parameter smoothing: 20ms linear (`juce::SmoothedValue`), coefficients
  refreshed every 16 samples (smoothing path) or every sample (Dynamic EQ path)
- Unity gain check: Bell at 0 dB → max |out - in| = 0.00e+00

### 2.2 Dynamic EQ (fixed in v2.2.1)

**Rating: ✅ Correct (post v2.2.1)**

- **Before v2.2.1:** coefficients updated on a 16-sample interval even when
  Dynamic EQ was active → up to 16-sample transient lag
- **After v2.2.1:** `maybeUpdateCoeffs()` calls `setAllStages()` every sample
  when `dynEnabled = true`, matching the per-sample envelope follower cadence
- Envelope follower: one-pole attack/release on sidechain bandpass — correct
- Gain computation: `overDb * (1 - 1/ratio)` — standard compressor math, correct

### 2.3 Linear Phase Engine

**Rating: ✅ Correct**

- FIR length: 4096 taps
- Latency: 2048 samples (firLength / 2) — reported to DAW via `setLatencySamples`
- FIR construction: IFFT of magnitude spectrum → circular shift → Hann window
  → forward FFT for overlap-add. Produces a symmetric (zero-phase) kernel
- Threading: background rebuild thread, atomic triple-buffer swap-chain with
  `memory_order_release`/`acquire`. Audio thread never blocks
- Known limitation: does not apply M/S, drive, or Dynamic EQ (minimum-phase
  path only). UI correctly greys out affected controls

### 2.4 Match EQ (fixed in v2.2.1)

**Rating: ✅ Correct (post v2.2.1)**

- Spectrum capture: averaged over multiple frames via running mean
- Correction: `clamp(reference - current, -24, 24)` dB per bin
- Application: overlap-add FFT, per-bin gain multiplier
- **Before v2.2.1:** `pow(10, corrDb/20)` called per bin per audio block (~4096
  transcendental calls/block while Match EQ is active)
- **After v2.2.1:** `correctionGain[]` pre-computed once when correction is
  finalised; hot path does a table lookup — **3x throughput improvement**
- Known limitation: capture is mono-summed (L+R * 0.5); correction applied
  per-channel. Documented in README

### 2.5 Saturation — Transistor (fixed in v2.2.5)

**Rating: ✅ Correct (post v2.2.5) — ProEQ8 only**

- **Before (through v2.2.4):** `clamp(x*d, -1, 1) * invD * d` where `invD = 1/d`
  → simplifies to `clamp(x*d, -1, 1) * 1.0` → no normalisation (no-op)
- **After (v2.2.5):** `clamp(x*d, -1, 1) * invD` → drives signal, clips, restores unity
- AUDIT.md v2.2.1 claimed this was fixed but the code was not committed until v2.2.5
- FreeEQ8 (Tanh only) was not affected

### 2.8 SVF Implementation (added v2.2.2)

**Rating: ✅ Correct**

- Simper SVF via trapezoidal integration (`SvfBiquad.h`)
- Bell uses `kA = k/A` for both denominator and m1 mix — critical detail
- 11-assertion correctness suite in `Tests/SvfTest.cpp`: unity, peak gain,
  LP/HP -3dB, stability sweep, state reset, Q-independence, HF gain accuracy
- All 11 pass on i7-3720QM (Ivy Bridge, SSE4.2, no AVX2)

### 2.9 MatchEQ Hot-Path (fixed in v2.2.5)

**Rating: ✅ Correct (post v2.2.5)**

- `correctionGain[]` precompute array added; hot path uses table lookup
- Eliminates ~4096 `std::pow()` calls per audio block when Match EQ is active
- 3× throughput improvement measured

### 2.6 Mid/Side

**Rating: ✅ Correct**

- Encode: `M = (L+R) * 0.5`, `S = (L-R) * 0.5`
- Decode: `L = M+S`, `R = M-S`
- Per-band channel routing via `ChannelRoute` enum (Both/Mid/Side)

### 2.7 Denormal handling

**Rating: ✅ Correct**

- `juce::ScopedNoDenormals` at top of `processBlock` sets FTZ/DAZ CPU flags
  for the duration of the callback
- Covers all biquad state variables for the block duration
- Benchmark with near-subnormal signal shows normal performance (~5 ns/sample)
  confirming FTZ is active

---

## 3. Threading Audit

### 3.1 Linear phase kernel rebuild

**Rating: ✅ Safe**

- Background thread owns FIR kernel build; publishes via atomic triple-buffer
- `memory_order_release` on write, `memory_order_acquire` on read
- Audio thread never acquires a mutex; pointer swap is lock-free
- Before first kernel is ready: audio passes through unprocessed (documented)

### 3.2 Spectrum FIFO (audio → UI)

**Rating: ✅ Safe**

- Triple-buffer: audio thread writes, UI thread reads
- All cross-thread state uses atomic operations
- No heap allocation on push path
- Benchmark: 0.66 ns/sample push cost — effectively free

### 3.3 License re-verification (ProEQ8)

**Rating: ✅ Safe**

- `std::thread([&validator] { validator.reverifyWithServer(); }).detach()`
- HTTP call does not touch the editor on completion
- Network failure keeps cached license within grace window (30 days)
- Not present in FreeEQ8 build

---

## 4. Issues Found and Fixed

| ID | Severity | Affected Build | Fixed In | Description |
|----|----------|---------------|----------|-------------|
| A1 | Medium | ProEQ8 | v2.2.5 | Transistor saturation gain error — `invD*d` no-op (code fix committed v2.2.5) |
| A2 | Low | Both | v2.2.1 | Dynamic EQ coefficient lag — up to 16-sample transient lag |
| A3 | Low | Both | v2.2.5 | Match EQ hot-path pow() — `correctionGain[]` precompute added v2.2.5 |
| A4 | Info | Both | v2.2.1 | LP + oversampling interaction — behaviour documented in source |
| A5 | Info | FreeEQ8 | v2.2.5 | Export limit (4:30 offline) added to FreeEQ8 |
| A6 | Info | ProEQ8 | v2.2.5 | Demo export block — ProEQ8 demo disables offline render entirely |

---

## 5. Open Known Issues (not blocking)

| Issue | Affects | Status |
|-------|---------|--------|
| Click when changing oversampling mid-playback | Both | Open — IIR state reset needed |
| Factory presets don't save slope/channel/drive/dynamic settings | Both | Open |
| Match EQ capture is mono-summed | Both | By design (documented) |
| Linear phase doesn't apply M/S / drive / dynEQ | Both | By design (documented) |
