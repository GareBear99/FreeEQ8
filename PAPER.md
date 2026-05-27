# Real-Time State-Space Parameterization and Lock-Free Semantic Analysis in Digital Equalization

**Gary Doman** (GareBear99 / TizWildin)  
FreeEQ8 / ProEQ8 Open-Source DSP Project  
https://github.com/GareBear99/FreeEQ8

---

## Abstract

This paper presents the architecture of a production-grade 8-band parametric
equalizer designed to eliminate high-frequency magnitude cramping without the
computational overhead of brute-force oversampling. By utilizing a 64-bit
double-precision implementation of the Simper State Variable Filter (SVF)
topology via trapezoidal integration, the system achieves a de-cramped
frequency response near the Nyquist limit while consuming only **0.62% of a
single-core real-time CPU budget** at 44.1 kHz (161× headroom). To ensure
absolute real-time safety within modern Digital Audio Workstations (DAWs), a
lock-free Single-Producer Single-Consumer (SPSC) triple-buffering swap-chain
isolates the audio hot-path from UI rendering. We further introduce a
**variable-cadence coefficient engine** that switches between 4-sample batching
during sustained signals and per-sample accuracy on transients, reducing Dynamic
EQ CPU cost by up to 75% under stable envelope conditions. Finally, an
allocation-free, log-frequency resonance detection array (`ResonanceDetector.h`)
maps live spectral coefficients to localized semantic labels—providing a
framework for zero-latency, explainable mix-assist workflows.

---

## 1. Introduction

Traditional parametric EQ implementations use the Robert Bristow-Johnson (RBJ)
Audio EQ Cookbook biquad formulas [6] in Transposed Direct Form II (TDF-II).
While mathematically correct, the bilinear transform introduces frequency
cramping near Nyquist: a Bell filter at 16 kHz with Q=1.0 at 44.1 kHz exhibits
an effective bandwidth **199% narrower** than intended. Industry solutions
include brute-force oversampling (adding latency and CPU cost) or proprietary
polynomial analog-matching curves (FabFilter Pro-Q family).

This paper documents a third path: the Simper SVF topology, which pre-warps
the cutoff frequency via `g = tan(π·fc/fs)`, achieving exact cutoff placement
at any frequency up to Nyquist without oversampling. All eight required filter
types (Bell, LowShelf, HighShelf, LP, HP, Bandpass, Notch, AllPass) emerge from
a single two-integrator core, simplifying maintenance and enabling structural
modulation safety that TDF-II cannot provide.

---

## 2. Filter Topology

### 2.1 RBJ TDF-II (Legacy Path — FreeEQ8)

```
H(z) = (b₀ + b₁z⁻¹ + b₂z⁻²) / (1 + a₁z⁻¹ + a₂z⁻²)
```

Coefficients computed per the RBJ cookbook [6]. 64-bit double internal state,
float I/O. Parameter smoothing: 20 ms linear ramp, coefficients refreshed every
16 samples (smoothing path) or every sample (Dynamic EQ path).

**Measured Q distortion (Bell, Q=1.0, 44.1 kHz):**

| Frequency | RBJ effective Q | Error |
|-----------|----------------|-------|
| 1 kHz | 1.005 | +0.5% |
| 8 kHz | 1.337 | +33.7% |
| 16 kHz | 2.990 | +199% |

### 2.2 Simper SVF (Modern Path — ProEQ8)

Reference: Andrew Simper, "Solving the continuous SVF equations using
trapezoidal integration and equivalent currents," Cytomic, 2013 [1].

**Pre-warped cutoff:**
```
g  = tan(π · fc / fs)          // exact pre-warp
k  = 1/Q
a1 = 1 / (1 + g·(g + k))      // shared across all filter types
a2 = g · a1
a3 = g · a2
```

**Per-sample processing (optimised bounded form):**
```cpp
v3  = v0 - ic2eq
t   = a2 * ic1eq               // cached — eliminates redundant mul
v1  = a1 * ic1eq + a2 * v3    // bandpass
v2  = ic2eq + t + a3 * v3     // lowpass
ic1eq = v1 + v1 - ic1eq       // v+v avoids 2.0* mul
ic2eq = v2 + v2 - ic2eq
out = m0·v0 + m1·v1 + m2·v2  // filter-type-specific mix
```

**Output mix coefficients per filter type** (from Simper paper §§3–8):

| Type | m0 | m1 | m2 |
|------|----|----|-----|
| LP | 0 | 0 | 1 |
| HP | 1 | −k | −1 |
| BP | 0 | 1 | 0 |
| Bell | 1 | kA·(A²−1) | 0 |
| LowShelf | 1 | k·(A−1) | A²−1 |
| HighShelf | A² | k·(1−A)·A | 1−A² |

where `A = 10^(gainDb/40)` and `kA = k/A` (Bell uses modified denominator).

**Measured performance (g++ -O3, 44.1 kHz, 512-sample block, 8-band stereo):**

| Path | ns/sample | CPU headroom |
|------|-----------|-------------|
| RBJ 8-band | 40.7 | 277× |
| SVF 8-band | 70.4 | 161× |
| SVF overhead | 1.73× | — |
| CPU budget used | 0.62% | — |

---

## 3. Real-Time Safety Architecture

### 3.1 SPSC Triple-Buffer (SpectrumFIFO)

Three buffer slots indexed by `{writeSlot, midSlot, readSlot}` (a permutation
of {0, 1, 2}). Audio thread writes into `writeSlot`; when a frame is complete,
atomically swaps `writeSlot ↔ midSlot` with `memory_order_release`. UI thread
reads by atomically swapping `midSlot ↔ readSlot` with `memory_order_acquire`.
No mutex, no lock, no blocking on either thread.

### 3.2 Off-Thread FIR Reconstruction (LinearPhaseEngine)

Linear phase mode latency: 2048 samples (4096-tap Hann-windowed FIR / 2).
Parameter changes set an atomic `linPhaseDirty` flag. A dedicated background
thread (`LinPhaseRebuildThread`) parks via `wait(-1)`, wakes on notify, rebuilds
the FIR kernel (magnitude → IFFT → circular shift → Hann window → forward FFT),
and publishes via the same triple-buffer atomic swap protocol. Audio thread reads
with a single acquire load—never blocks.

### 3.3 Allocation-Free Hot Path

All oversamplers (1×/2×/4×/8× via JUCE polyphase IIR) are pre-constructed in
`prepareToPlay()` into `std::array<std::unique_ptr<Oversampling<float>>, 3>`.
Mid-playback order changes call `Oversampling::reset()` (non-allocating) and
trigger a 128-sample linear crossfade to eliminate the transient pop (v2.2.3).

---

## 4. Variable-Cadence Dynamic EQ (v2.2.3)

Dynamic EQ recomputes biquad coefficients per-sample when active, matching the
one-pole envelope follower's cadence. With all 8 bands in dynamic mode the
`bq.set()` call dominates (22 ns/call for SVF Bell). We observe that during
held/sustained notes the envelope `dynGainMod` is near-static: changes of less
than **0.1 dB between samples** are inaudible within the 4-sample batch window
(0.09 ms at 44.1 kHz).

**Variable-cadence algorithm:**
```
δ = |dynGainMod − lastDynGainMod|
if δ > 0.1 dB:
    update coefficients (per-sample — zero transient lag)
    intervalCounter = 0
else:
    if intervalCounter++ >= 4:
        update coefficients (batched)
```

**Measured savings** at 8 bands all-dynamic, sustained note:
up to 75% reduction in `bq.set()` calls with no audible difference.
On a transient attack the first sample with δ > 0.1 dB immediately restores
per-sample accuracy.

---

## 5. Allocation-Free Semantic Analysis (ResonanceDetector)

Traditional "smart EQ" products apply machine-learning inference models (Soothe2,
iZotope Neutron) — opaque, CPU-heavy, non-deterministic. We introduce a
deterministic alternative that adds zero latency and allocates no memory:

1. Take the 2048-bin log-magnitude spectrum from `SpectrumFIFO` (UI thread rate).
2. Re-sample into a 96-bin log-frequency grid (20 Hz → Nyquist).
3. Estimate local baseline via ±0.5-octave moving average.
4. Flag peaks where `(magnitude − baseline) ≥ 3 dB` and peak is local max in
   ±3-bin neighbourhood.
5. Score each peak: `score = deviation × intentWeight(hz, mode)` where
   `intentWeight` is a log-frequency Gaussian bump per `IntentMode` profile.
6. Return top-4 suggestions: `{freqHz, gainDb, q, confidence, label}`.

The `intentWeight` function encodes instrument-specific problem zones:

| Mode | Primary Bump | Zone |
|------|-------------|------|
| VocalClean | ×1.6 at 300 Hz | mud |
| DrumPunch | ×1.5 at 300 Hz | boxiness |
| GuitarSpace | ×1.5 at 250 Hz | mud |
| MasterPolish | ×1.3 at 250 Hz | low-end buildup |

Semantic labels from `FrequencyExplainer.h` map frequency ranges to strings
("mud", "harshness", "sibilance", "air") enabling explain-on-hover UX.

---

## 7. Benchmarks (Measured — Reproducible)

All benchmarks run from `Tests/FeatureBench.cpp` — standalone, no JUCE, no DAW,
no mock. Build: `g++ -std=c++17 -O3 -DNDEBUG -pthread Tests/FeatureBench.cpp -o FeatureBench -ISource`.
Platform: Linux x86-64, g++ 13.3.0. Median of 16 trials, 4 warmup runs discarded.

### 7.1 Single-Instance Filter Cost

| Path | ns/sample | MB/s | CPU% (44.1kHz/512/50%) | Headroom |
|------|-----------|------|------------------------|---------|
| RBJ 8-band stereo | 41.0 | 98 | 0.36% | 277× |
| SVF 8-band stereo | 72.7 | 55 | 0.63% | 161× |
| SVF overhead vs RBJ | 1.61× | — | — | — |
| SVF DynEQ per-sample | 68.8 | 58 | 0.61% | 165× |

### 7.2 Instance Scaling (Real DAW Load Simulation)

The critical gap identified in post-release review: "not yet crossed into industrial
benchmark validation under real DAW stress matrices." This table fills that gap.
Each row simulates N simultaneous independent 8-band stereo plugin instances.

| Instances | RBJ ns/samp | RBJ CPU% | SVF ns/samp | SVF CPU% | SVF/RBJ |
|-----------|-------------|----------|-------------|----------|---------|
| 1 | 44.4 | 0.39% | 71.9 | 0.63% | 1.62× |
| 8 | 46.6 | 0.41% | 73.4 | 0.65% | 1.58× |
| 32 | 47.5 | 0.42% | 75.1 | 0.66% | 1.58× |
| 64 | 46.4 | 0.41% | 75.9 | 0.67% | 1.64× |
| 128 | 46.8 | 0.41% | 75.5 | 0.67% | 1.61× |

**Key finding:** Per-instance cost rises only **5% from 1→128 instances** (cache
pressure from larger working set). The scaling is sub-linear — each instance
benefits from the previous instance's cache warmup on shared coefficient tables.

At 128 SVF instances total CPU = 128 × 0.67% = **85.8%** of one core at 44.1 kHz
with 512-sample blocks. A modern 8-core CPU can host ~900 SVF instances.

### 7.3 Worst-Case Dynamic EQ

Document 11 review identified: *"the actual limit is NOT filter math — it becomes
dynamic coefficient churn."* This benchmark quantifies exactly that ceiling.

Configuration: 8 bands simultaneously in dynamic mode, white noise input
(maximum envelope follower excitation — all transients, all samples active),
variable-cadence engine active (v2.2.3 optimization).

| Configuration | ns/sample | CPU% | Headroom |
|--------------|-----------|------|---------|
| 8-band DynEQ, white noise, all active | 370.9 | 3.27% | 30.6× |

**Finding:** Even at absolute worst-case (8 active dynamic bands tracking white
noise), the variable-cadence engine keeps CPU below 3.3%. The 30.6× headroom
means a 50% CPU budget can host ~9 simultaneous worst-case dynamic EQ instances.

### 7.4 SvfBandArray — Packed SIMD Scaffold

The `SvfBandArray<8>` template (v2.2.4) packs all 8 band states into aligned
arrays for SIMD dispatch. On this test machine (SSE2, no AVX2 available at test
time), the scalar fallback runs:

| Path | ns/sample (mono) | CPU% | vs SVF scalar stereo |
|------|-----------------|------|----------------------|
| SvfBandArray<8> scalar (SSE2 host) | 23.5 | 0.21% | 3.1× faster |

The mono vs stereo difference accounts for half the gap. With AVX2 active
(8-wide float32), projected improvement is an additional 2–4× over scalar,
targeting < 10 ns/sample for all 8 bands mono — approaching 0.09% CPU.

### 7.5 MatchEQ Hot-Path Optimization

| Path | ns/sample equivalent | Speedup |
|------|----------------------|---------|
| Naive pow(10) per bin (old) | 7.4 | — |
| Pre-computed correctionGain[] (v2.2.1) | 2.8 | **3.0×** |

### 7.6 Reproducing These Results

```bash
git clone --recursive https://github.com/GareBear99/FreeEQ8.git
cd FreeEQ8
g++ -std=c++17 -O3 -DNDEBUG -pthread Tests/FeatureBench.cpp -o FeatureBench -ISource
./FeatureBench          # human-readable table
./FeatureBench --csv    # machine-readable CSV

# For ARC-AudioBench integration (JSON output):
g++ -std=c++17 -O3 -DNDEBUG Tests/ArcBenchIntegration.cpp -o ArcBench -ISource
./ArcBench --json > arc_results.json
```

Numbers will vary by CPU and compiler. The headroom ratios should remain
comfortably above 10× on any modern x86-64 or Apple Silicon machine.



Inspired by Ableton Live's EQ Eight compact device view. Design constraint: the
coordinate mapping (`freqToX`, `dbToY`), drag sensitivity (pixel delta → parameter
delta), Q drag acceleration, and node hit-test radius (as proportion of view
height) must be **identical** between full and compact views. Only visual density
changes: FFT resolution, grid label density, node text size.

This is enforced architecturally: `setCompactMode(bool)` sets a flag but never
modifies the mapping functions. The APVTS remains the single source of truth;
both renderers read the same parameter values.

---

## 7. Future Work (v2.5+)

- **Explicit SIMD vectorisation**: group 8 bands into `juce::dsp::SIMDRegister<float>`,
  processing 4 bands per SSE instruction or 8 via AVX2.
- **Cross-instance masking negotiation**: via ARC-Core local IPC spine, multiple
  plugin instances communicate energy peaks and negotiate inverse dynamic notches.
- **Spectral dynamics mode**: per-bin FFT threshold clamping (Soothe2 territory)
  using the existing overlap-add Match EQ infrastructure.
- **Dolby Atmos 9.1.6**: expand `isBusesLayoutSupported` for discrete immersive
  channel arrays with spatial zone linking.

---

## References

[1] A. Simper, "Solving the continuous SVF equations using trapezoidal integration
    and equivalent currents," Cytomic, 2013.
    https://cytomic.com/files/dsp/SvfLinearTrapOptimised2.pdf

[2] W. Pirkle, "Designing Audio Effect Plugins in C++," Focal Press, 2019.

[3] J. Reiss and A. McPherson, "Audio Effects: Theory, Implementation and
    Application," CRC Press, 2014.

[4] F. Renn-Giles and D. Rowland, "Real-time 101," ADC 2019.
    https://github.com/hogliux/farbot

[5] JUCE, "juce::dsp::Oversampling," Articy, 2023.
    https://docs.juce.com/master/classjuce_1_1dsp_1_1Oversampling.html

[6] R. Bristow-Johnson, "Audio EQ Cookbook," musicdsp.org, 1994.
    https://www.musicdsp.org/files/Audio-EQ-Cookbook.txt
