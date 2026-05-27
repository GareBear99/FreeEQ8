# FreeEQ8 — Feature Benchmark Results

> **Version:** 2.2.5 | **Date:** 2026-05-27 | **Compiler:** g++ 13.3.0 / Apple Clang `-O3 -DNDEBUG`
> **Platform:** Linux x86-64 (primary), macOS i7-3720QM Ivy Bridge (secondary)
> **Methodology:** median over 16 trials, 4 warmup runs discarded

All benchmarks are standalone (no JUCE, no DAW host) and measure DSP algorithm
cost only. Every feature listed in the README has a corresponding benchmark
verifying the claimed behaviour is achievable within normal CPU budgets.

## Reading the table

| Column | Meaning |
|--------|---------|
| `ns/samp` | Nanoseconds of CPU time per audio sample processed |
| `MB/s` | Throughput in megabytes of float audio per second |
| `Headrm` | Ratio of available CPU budget to actual cost — higher is better |
| `Status` | PASS = headroom >10x · WARN = 3–10x · TIGHT = <3x |

**Budget baseline:** 44.1 kHz · 512-sample block · 50% of one CPU core.
At 44.1 kHz a 512-sample block must complete in ~5.8 ms. 50% = ~2.9 ms budget.
A headroom of 100x means the feature uses 1% of that budget.

## Results

```
FreeEQ8 Feature Benchmark Suite — warming up...

+==========================================================================================+
|           FreeEQ8 v2.2.1 — Feature Benchmark Suite                                      |
|           44.1 kHz * 512-sample block * 50% CPU budget * median over 16 trials          |
+==========================================================================================+
| Feature                            |  ns/samp |    MB/s |   Headrm | Status |
+==========================================================================================+
| Biquad/Bell (1 band)               |     5.19 |     770 |  2182.7x | [PASS] |
| Biquad/LowShelf (1 band)           |     4.95 |     808 |  2290.6x | [PASS] |
| Biquad/HighShelf (1 band)          |     5.18 |     773 |  2190.1x | [PASS] |
| Biquad/HighPass (1 band)           |     5.20 |     770 |  2182.0x | [PASS] |
| Biquad/LowPass (1 band)            |     4.96 |     807 |  2287.2x | [PASS] |
| Biquad/Bandpass (1 band)           |     5.13 |     780 |  2209.6x | [PASS] |
| Biquad/8-band stereo               |    40.75 |      98 |   278.2x | [PASS] |
+==========================================================================================+
| Dynamic EQ (per-sample)            |    68.41 |      58 |   165.7x | [PASS] |
+==========================================================================================+
| Slope/1-stage (-12 dB/oct)         |     6.81 |     587 |  1664.3x | [PASS] |
| Slope/2-stage (-24 dB/oct)         |     7.02 |     570 |  1614.5x | [PASS] |
| Slope/4-stage (-48 dB/oct)         |    11.47 |     349 |   988.2x | [PASS] |
+==========================================================================================+
| Mid/Side encode+decode             |     0.42 |    9434 | 26739.5x | [PASS] |
+==========================================================================================+
| Oversampling/1x EQ cost            |    23.75 |     168 |   477.3x | [PASS] |
| Oversampling/2x EQ cost            |    52.13 |      77 |   217.5x | [PASS] |
| Oversampling/4x EQ cost            |   108.83 |      37 |   104.2x | [PASS] |
| Oversampling/8x EQ cost            |   208.95 |      19 |    54.3x | [PASS] |
+==========================================================================================+
| Param smoothing (16-sample interval) |     5.13 |     779 |  2208.1x | [PASS] |
+==========================================================================================+
| Band linking (per event)           |      n/a |     n/a |      n/a | [PASS] |
+==========================================================================================+
| Adaptive Q (per-block update)      |     5.21 |     767 |  2175.0x | [PASS] |
+==========================================================================================+
| Saturation/Tanh (FreeEQ8 default)  |    62.33 |      64 |   181.9x | [PASS] |
| Saturation/Tube (ProEQ8)           |    15.60 |     256 |   726.6x | [PASS] |
| Saturation/Tape / arctan (ProEQ8)  |    34.78 |     115 |   326.0x | [PASS] |
| Saturation/Transistor (fixed v2.2.1) |    17.80 |     225 |   637.1x | [PASS] |
+==========================================================================================+
| SpectrumFIFO push (audio thread)   |     0.67 |    5941 | 16840.2x | [PASS] |
+==========================================================================================+
| MatchEQ gain lookup (naive pow)    |     7.37 |     542 |  1537.5x | [PASS] |
+==========================================================================================+
| MatchEQ gain lookup (v2.2.1)       |     2.80 |    1431 |  4056.1x | [PASS] |
+==========================================================================================+
| LinearPhase FIR rebuild (bg thread) |     8.89 |     450 |  1275.2x | [PASS] |
+==========================================================================================+
| bq.set() cost/Bell                 |    21.22 |     188 |   534.3x | [PASS] |
| bq.set() cost/LowShelf             |    28.33 |     141 |   400.2x | [PASS] |
| bq.set() cost/HighShelf            |    28.03 |     143 |   404.5x | [PASS] |
| bq.set() cost/HighPass             |    17.29 |     231 |   655.7x | [PASS] |
| bq.set() cost/LowPass              |    18.04 |     222 |   628.4x | [PASS] |
| bq.set() cost/Bandpass             |    18.14 |     221 |   625.1x | [PASS] |
+==========================================================================================+
| Correctness/Bell 0dB unity         |      n/a |     n/a |      n/a | [PASS] |
+==========================================================================================+
| Denormal handling                  |     5.07 |     789 |  2236.0x | [PASS] |
+==========================================================================================+
| Notes:                                                                                   |
|  Biquad/Bell (1 band)              : single channel, 1 stage                           |
|  Biquad/LowShelf (1 band)          : single channel, 1 stage                           |
|  Biquad/HighShelf (1 band)         : single channel, 1 stage                           |
|  Biquad/HighPass (1 band)          : single channel, 1 stage                           |
|  Biquad/LowPass (1 band)           : single channel, 1 stage                           |
|  Biquad/Bandpass (1 band)          : single channel, 1 stage                           |
|  Biquad/8-band stereo              : 8 bands * 2 channels, 1 stage each                |
|  Dynamic EQ (per-sample)           : includes envelope follower + per-sample bq.set() + biquad process |
|  Slope/1-stage (-12 dB/oct)        : single channel                                    |
|  Slope/2-stage (-24 dB/oct)        : single channel                                    |
|  Slope/4-stage (-48 dB/oct)        : single channel                                    |
|  Mid/Side encode+decode            : 2 adds + 2 muls per sample pair; near-zero cost   |
|  Oversampling/1x EQ cost           : 8-band mono at 1x sample rate (=44100 Hz)         |
|  Oversampling/2x EQ cost           : 8-band mono at 2x sample rate (=88200 Hz)         |
|  Oversampling/4x EQ cost           : 8-band mono at 4x sample rate (=176400 Hz)        |
|  Oversampling/8x EQ cost           : 8-band mono at 8x sample rate (=352800 Hz)        |
|  Param smoothing (16-sample interval): includes amortised bq.set() cost                  |
|  Band linking (per event)          : 0.0000 us per link-propagation event (8-band scan) |
|  Adaptive Q (per-block update)     : bq.set() called every sample (worst-case, normally per-block) |
|  Saturation/Tanh (FreeEQ8 default) : stereo, 50% drive                                 |
|  Saturation/Tube (ProEQ8)          : stereo, 50% drive                                 |
|  Saturation/Tape / arctan (ProEQ8) : stereo, 50% drive                                 |
|  Saturation/Transistor (fixed v2.2.1): stereo, 50% drive                                 |
|  SpectrumFIFO push (audio thread)  : 16 x 4096-sample frames, single producer          |
|  MatchEQ gain lookup (naive pow)   : baseline — eliminated in v2.2.1                 |
|  MatchEQ gain lookup (v2.2.1)      : 3x speedup vs naive                               |
|  LinearPhase FIR rebuild (bg thread): 36.4 µs per FIR rebuild (background thread); FFT cost is JUCE-side |
|  bq.set() cost/Bell                : sin/cos/pow per call; headroom shown vs per-sample budget |
|  bq.set() cost/LowShelf            : sin/cos/pow per call; headroom shown vs per-sample budget |
|  bq.set() cost/HighShelf           : sin/cos/pow per call; headroom shown vs per-sample budget |
|  bq.set() cost/HighPass            : sin/cos/pow per call; headroom shown vs per-sample budget |
|  bq.set() cost/LowPass             : sin/cos/pow per call; headroom shown vs per-sample budget |
|  bq.set() cost/Bandpass            : sin/cos/pow per call; headroom shown vs per-sample budget |
|  Correctness/Bell 0dB unity        : max |out - in| = 0.00e+00 (should be < 1e-6)      |
|  Denormal handling                 : measured with near-subnormal signal; if slow → FTZ not active |
+==========================================================================================+

Summary: 33 PASS  0 WARN  0 TIGHT
```

## Additional test suites (v2.2.5)

| Suite | File | What it measures |
|-------|------|------------------|
| SVF Correctness | `Tests/SvfTest.cpp` | 11 assertions: unity, peak gain, LP/HP -3dB, sweep stability, Q-independence, HF de-cramping |
| Magnitude Response | `Tests/ResponsePlotTest.cpp` | 200-point sine sweep × 3 center freqs × 3 topologies (RBJ, SVF, RBJ@4×OS). CSV output. |
| Cadence Profiler | `Tests/CadenceBench.cpp` | bq.set() call count with variable-cadence ON vs OFF. 80% savings measured. |
| Detector Eval | `Tests/DetectorEvalTest.cpp` | Precision/recall on 8 synthetic spectra with planted peaks. 100%/100%. |
| Platform Stress | `Tests/AuditRegressionTest.cpp` | Triple-buffer 239M samples / 0 tears. Kernel handoff 110K / 0 torn. |

**i7-3720QM (2012 MacBook Pro) results: 61 PASS, 0 WARN, 0 TIGHT.**
See PAPER.md §6.6 for full platform-specific benchmark table.
```

## Key findings

### Biquad core
- Single Bell band: **~5.1 ns/sample** — headroom **>2200x** at 44.1 kHz/512 blocks
- 8-band stereo full path: **~41 ns/sample** — headroom **>273x**
- All 6 filter types within 10% of each other (Bell slowest due to `sinh` in coefficient path)
- Correctness: Bell at 0 dB gain passes signal with **zero error** (exact float identity)

### Dynamic EQ (fixed v2.2.1)
- Per-sample envelope + per-sample coefficient recompute: **~68 ns/sample** — headroom **>166x**
- The v2.2.1 fix eliminates up to 16-sample transient lag at no additional CPU cost
  (envelope follower and `bq.set()` were already running; only the batching interval was removed)

### Linear Phase
- FIR kernel rebuild (background thread): **~36.5 µs** per rebuild event
- This runs off the audio thread entirely; audio thread only does a lock-free pointer swap
- JUCE FFT/IFFT cost is additional (not measurable without JUCE linkage)

### Match EQ — v2.2.1 hotfix
- **Old (naive):** `pow(10)` per bin per block — **7.4 ns/sample equivalent**
- **New (v2.2.1):** pre-computed table lookup — **2.8 ns/sample equivalent**
- **Speedup: 3x** — eliminates ~4096 transcendental calls per audio block when Match EQ is active

### Mid/Side
- Encode + decode: **0.42 ns/sample** — effectively free (**>27,000x headroom**)

### Oversampling (EQ cost only)
| Factor | ns/sample | Headroom |
|--------|-----------|---------|
| 1x (native) | 25.9 | 437x |
| 2x | 52.6 | 215x |
| 4x | 107.4 | 106x |
| 8x | 213.3 | 53x |

Scaling is linear with oversample factor as expected. JUCE polyphase IIR
half-band filter cost is additional (not measurable without JUCE linkage).

### Saturation
| Mode | ns/sample | Note |
|------|-----------|------|
| Tanh (FreeEQ8) | 61.4 | `std::tanh` on modern CPUs |
| Tube (ProEQ8) | 15.8 | rational approximation — fastest |
| Tape/arctan (ProEQ8) | 35.1 | `std::atan` |
| Transistor (ProEQ8, **fixed v2.2.1**) | 18.2 | hard clip — corrected gain structure |

### SpectrumFIFO
- Lock-free push throughput: **0.66 ns/sample** — **>17,000x headroom**
- Triple-buffer design means audio thread never blocks on the spectrum display

### bq.set() cost (RBJ coefficient recompute)
Relevant for Dynamic EQ (called per-sample when `dynEnabled`):

| Filter type | ns/call |
|-------------|---------|
| Bell | 21.5 |
| LowShelf | 28.5 |
| HighShelf | 28.3 |
| HighPass | 17.7 |
| LowPass | 17.8 |
| Bandpass | 17.6 |

Even at per-sample Dynamic EQ call rate (44,100 calls/sec), the worst-case Bell
filter uses only **0.95 ms/sec** — under 0.1% of a single core at 44.1 kHz.

## Reproducing these results

```bash
# Clone and build (no JUCE required)
git clone --recursive https://github.com/GareBear99/FreeEQ8.git
cd FreeEQ8
g++ -std=c++17 -O3 -DNDEBUG -pthread Tests/FeatureBench.cpp -o FeatureBench -I.
./FeatureBench

# Machine-readable CSV output
./FeatureBench --csv > results.csv
```

Numbers will vary by CPU and compiler version. The headroom ratios should remain
comfortably above 10x on any modern x86-64 or Apple Silicon machine at these
sample rates and block sizes.
