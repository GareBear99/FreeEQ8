# Competitive Analysis: EQ Implementation Benchmarks

This document describes the methodology for comparing FreeEQ8's filter implementations
against other EQ plugins and topologies. The framework is designed for academic
validation per DAFx/AES/ICMC submission standards.

## Overview

The competitive benchmark suite (`Tests/CompetitiveBench.cpp`) measures four key metrics
across different EQ implementations:

1. **CPU Time** — Processing cost in nanoseconds per sample
2. **Latency** — Sample delay introduced by the algorithm
3. **HF Cramping** — Magnitude error at high frequencies due to bilinear transform
4. **Memory Footprint** — State and coefficient storage requirements

## Benchmark Methodology

### Test Configuration

| Parameter | Value | Rationale |
|-----------|-------|-----------|
| Sample Rate | 44,100 Hz | Standard CD quality, common DAW default |
| Block Size | 512 samples | Typical DAW buffer size (11.6 ms) |
| CPU Budget | 50% of one core | Conservative real-time safety margin |
| Band Counts | 1, 8, 24 | Minimal, typical, professional configurations |
| Trial Count | 16 | Sufficient for stable median |
| Warmup Runs | 4 | Eliminate cold-cache effects |

### Timing Methodology

```
1. Execute warmup trials (discarded)
2. Execute N measurement trials
3. Record wall-clock time per trial
4. Divide by total samples processed
5. Report median (robust to outliers)
```

The median is preferred over mean because it resists outliers caused by
OS scheduling jitter, garbage collection (in JIT environments), and
thermal throttling.

### CPU Headroom Calculation

```
budget_ns  = (block_size / sample_rate) × 1e9 × 0.5
cost_ns    = ns_per_sample × block_size
headroom_x = budget_ns / cost_ns
```

**Interpretation:**
- `headroom > 10×` — Comfortable (green) — ample margin for complex sessions
- `headroom 3–10×` — Acceptable (yellow) — monitor in dense mixes
- `headroom < 3×` — Tight (red) — review before low-end hardware deployment

### HF Cramping Measurement

Per PAPER.md §2.3, we measure the magnitude response error at high frequencies
to characterize filter topology cramping.

**Test Signal:**
- Pure sine wave at center frequency (fc = 16 kHz default)
- Duration: 8192 samples (sufficient for steady-state)
- Transient skip: first 2048 samples discarded

**Filter Configuration:**
- Type: Bell (peaking)
- Gain: +6.0 dB
- Q: 1.0
- Center frequency: swept from 1 kHz to 20 kHz

**Measurement:**
```
1. Generate input sine at fc
2. Process through filter
3. Skip transient (first 2048 samples)
4. Calculate RMS of input and output steady-state
5. measured_gain_dB = 20 × log10(output_rms / input_rms)
6. error_dB = measured_gain_dB - target_gain_dB
```

**Expected Results (per PAPER.md §2.3):**

| Topology | Magnitude at 16 kHz | Error vs +6 dB |
|----------|---------------------|----------------|
| RBJ @ 44.1 kHz | +6.000 dB | 0.000 dB (BLT guarantees exact at fc) |
| SVF @ 44.1 kHz | +6.00 dB | 0.00 dB (exact) |
| RBJ @ 4× OS | +5.993 dB | −0.007 dB |

The SVF achieves exact gain at the center frequency without oversampling due
to the pre-warped cutoff: `g = tan(π·fc/fs)`.

## Metrics Explained

### CPU Time (ns/sample)

The wall-clock time to process one audio sample through the complete filter chain.
Lower is better. Includes:
- Coefficient application
- State variable updates
- Output mix computation

Does NOT include:
- Parameter smoothing (handled at block boundaries)
- UI rendering (separate thread)
- Plugin framework overhead

### Latency (samples)

The algorithmic delay introduced by the filter:
- **IIR filters** (RBJ, SVF): 0 samples — infinite impulse response
- **Linear phase FIR**: N/2 samples — half the filter length
- **Minimum phase FIR**: Variable — depends on design

### HF Cramping

The bilinear transform used in RBJ biquads maps analog frequencies to digital
using `ω_d = 2·arctan(ω_a·T/2)`. This relationship compresses high frequencies
toward Nyquist, causing filters to behave narrower than specified.

**Q Distortion at 44.1 kHz (Bell, Q=1.0):**

| Frequency | RBJ Effective Q | Error |
|-----------|-----------------|-------|
| 1 kHz | 1.005 | +0.5% |
| 4 kHz | 1.081 | +8.1% |
| 8 kHz | 1.337 | +33.7% |
| 12 kHz | 1.859 | +85.9% |
| 16 kHz | 2.990 | +199% |

The SVF topology uses direct frequency warping (`g = tan(π·fc/fs)`) which
exactly compensates for this effect.

### Memory Footprint

Total bytes required for filter state and coefficients:
- RBJ Biquad: 5 coefficients + 4 state variables = ~72 bytes/band
- SVF: 6 coefficients + 4 state variables = ~80 bytes/band
- Linear phase FIR: N coefficients + circular buffer = ~4K–16K bytes

## Caveats and Fair Comparison Notes

### Not Apples-to-Apples

Different plugins have different feature sets that affect CPU cost:
- **FabFilter Pro-Q 3**: Decramped response (Orfanidis-style) + dynamic EQ + spectrum analyzer
- **Soothe2**: ML-based resonance detection (not a traditional EQ)
- **TDR Nova**: 4-band dynamic EQ with sidechain options
- **FreeEQ8**: 8-band EQ with saturation and linear phase modes

Direct CPU comparisons should note feature parity.

### External Plugin Limitations

Commercial plugins cannot be directly instrumented. Measurements require:
1. DAW-based CPU metering (less precise)
2. Offline rendering comparison (no CPU timing)
3. Manual configuration (potential human error)

See `Tests/PluginBenchTemplate.cpp` for integration approaches.

### Platform Variability

Benchmark results vary significantly by:
- CPU architecture (Intel vs AMD vs Apple Silicon)
- SIMD support (SSE2, AVX2, ARM Neon)
- Compiler optimization level
- OS scheduling behavior

Always report: CPU model, compiler version, OS, build flags.

## How to Add New Plugins

### Option 1: Direct Hosting (Recommended)

1. Implement the `BenchmarkableEQ` interface from `CompetitiveBench.cpp`
2. Use VST3 SDK, Audio Unit API, or JUCE AudioPluginHost
3. Map plugin parameters to the standard `setBand()` interface
4. Add to the benchmark suite in `run_all_benchmarks()`

Example structure:
```cpp
class MyPluginBenchEQ : public BenchmarkableEQ {
    void prepare(double sr, int maxBlock) override { /* ... */ }
    void setBand(int idx, double fc, double q, double gainDb) override { /* ... */ }
    void process(float* L, float* R, int n) override { /* ... */ }
    int getLatency() const override { return 0; }
    const char* getName() const override { return "My Plugin"; }
    size_t getMemoryBytes() const override { return sizeof(*this); }
    void reset() override { /* ... */ }
    int getNumBands() const override { return 8; }
};
```

### Option 2: DAW Offline Rendering

For plugins that can't be hosted directly:

1. Generate test audio files (sine sweeps, noise)
2. Create DAW project with plugin on a track
3. Configure plugin to match benchmark settings
4. Render/bounce via CLI (Reaper: `reaper -renderproject`)
5. Measure rendered output

This approach measures HF cramping and latency but not CPU time.

### Option 3: Manual Measurement

See procedure in `Tests/PluginBenchTemplate.cpp`:
1. Load plugin in DAW
2. Configure 8-band Bell EQ
3. Play white noise, record CPU meter
4. Convert DAW % to ns/sample

## Placeholder Results Table

_Results to be populated after running benchmarks on target hardware._

### Internal Implementations (CompetitiveBench.cpp)

| Implementation | Bands | ns/samp | MB/s | Headroom | Latency | HF Error |
|----------------|-------|---------|------|----------|---------|----------|
| RBJ TDF-II | 1 | TBD | TBD | TBD | 0 | TBD |
| RBJ TDF-II | 8 | TBD | TBD | TBD | 0 | TBD |
| RBJ TDF-II | 24 | TBD | TBD | TBD | 0 | TBD |
| Simper SVF | 1 | TBD | TBD | TBD | 0 | TBD |
| Simper SVF | 8 | TBD | TBD | TBD | 0 | TBD |
| Simper SVF | 24 | TBD | TBD | TBD | 0 | TBD |

### External Plugins (Manual Measurement)

| Plugin | Version | Bands | CPU % | Latency | HF Error | Notes |
|--------|---------|-------|-------|---------|----------|-------|
| FabFilter Pro-Q 3 | — | 8 | TBD | TBD | TBD | Industry standard |
| TDR Nova | — | 4 | TBD | TBD | TBD | Free tier available |
| Soothe2 | — | N/A | TBD | TBD | N/A | Different paradigm |
| iZotope Neutron | — | 8 | TBD | TBD | TBD | AI-driven |

### Test Environment

| Parameter | Value |
|-----------|-------|
| CPU | TBD |
| OS | TBD |
| Compiler | g++ -std=c++17 -O2 |
| Sample Rate | 44,100 Hz |
| Block Size | 512 samples |
| Date | TBD |

## Running the Benchmarks

```bash
# Build
g++ -std=c++17 -O2 -I. Tests/CompetitiveBench.cpp -o CompetitiveBench

# Run human-readable output
./CompetitiveBench

# Run CSV output for data analysis
./CompetitiveBench --csv > benchmark_results.csv
```

## References

1. A. Simper, "Solving the continuous SVF equations using trapezoidal integration
   and equivalent currents," Cytomic, 2013.
   https://cytomic.com/files/dsp/SvfLinearTrapOptimised2.pdf

2. R. Bristow-Johnson, "Audio EQ Cookbook," musicdsp.org, 1994.
   https://www.musicdsp.org/files/Audio-EQ-Cookbook.txt

3. PAPER.md §2.3 — Measured Magnitude Response: SVF vs RBJ vs Oversampling

---
_Co-Authored-By: Oz <oz-agent@warp.dev>_
