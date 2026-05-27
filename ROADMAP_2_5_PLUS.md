# FreeEQ8 / ProEQ8 — Long-Horizon Roadmap (v2.2.5+)

> Based on architectural analysis, JUCE forum feedback (Nitsuj70), competitor
> feature audit (FabFilter Pro-Q 4, Soothe2, DMG Equilibrium), and the
> ARC-Core ecosystem convergence plan.
> Last updated: 2026-05-24

---

## Phase Matrix

```
v2.2.2 ──── v2.2.3 ──── v2.2.4 ──── v2.2.5 ──── v2.3.0
  SVF         Smart EQ    SIMD         Spectral    ProEQ8
  engine      layer       scaffold     dynamics    launch
  shipped     wired       + bench      + Atmos     (Stripe)
```

---

## v2.3.0 — ProEQ8 Commercial Launch

**Target:** First paid release. All v2.2.x stabilisation complete.

### DSP
- Wire `SvfBiquad` into `EQBand` as ProEQ8 engine via `#if PROEQ8` template:
  ```cpp
  #if PROEQ8
      using BandFilter = SvfBiquad;
  #else
      using BandFilter = Biquad;  // FreeEQ8: RBJ stays, feature-frozen
  #endif
  ```
- ProEQ8 band count: 24 (`kNumBands = 24` behind `#if PROEQ8`)
- Tube / Tape saturation modes verified post-SVF integration

### Commercial Infrastructure
- Stripe `checkout.session.completed` webhook → Cloudflare Worker → KV license
- `/activate` endpoint: SHA-256 device fingerprint, max 2 devices
- `/deactivate` endpoint: machine migration support
- Resend email delivery on purchase
- Idempotency guard: KV duplicate-event check
- Timing-attack defense: constant-time XOR comparison in activation path

### UI
- ProEQ8 branding, license activation dialog (already in `#if PROEQ8` blocks)
- 24-band layout adapts via `kNumBands` at compile time

---


---

## Benchmark Status (v2.2.4 Measured + v2.2.5+ Targets)

### Measured — v2.2.4 (60/60 PASS, 0 WARN, 0 TIGHT)

| Benchmark | Measured | CPU% | Headroom |
|-----------|----------|------|---------|
| SVF 8-band stereo, single instance | 72.7 ns/samp | 0.63% | 161× |
| RBJ instance scaling 1→128 | +5% cost rise | near-linear | 242× |
| SVF instance scaling 1→128 | +5% cost rise | 0.63–0.67% | 150× |
| Worst-case DynEQ (8 bands, white noise) | 370.9 ns/samp | 3.27% | 30.6× |
| SvfBandArray<8> SSE2 mono | 23.5 ns/samp | 0.21% | 482× |
| Natural Phase latency | 128 samples | ~2.9ms at 44.1kHz | — |
| MatchEQ correctionGain[] speedup | 3.0× vs naive pow() | — | — |

**Key finding:** At 128 simultaneous SVF instances, total CPU = 85.8% of one core
at 44.1kHz/512. A modern 8-core CPU can host ~900 SVF instances. Per-instance
cost rises only 5% from 1→128 due to coefficient table cache warmup.

### Planned Targets — v2.2.5+

| Feature | Target | Status |
|---------|--------|--------|
| SvfBandArray AVX2 8-band mono | < 10 ns/sample | Scaffold ready |
| SvfBandArray AVX2 8-band stereo | < 20 ns/sample | Scaffold ready |
| 128 SVF instances total CPU | < 60% one core | Projected with AVX2 |
| Worst-case DynEQ + AVX2 | < 100 ns/sample | Projected |
| SVF 8-band stereo (ProEQ8, v2.3) | < 80 ns/sample | v2.3.0 |
| Natural Phase mode wired to UI | 128-sample latency | v2.3.0 |
| Smart EQ analyse() per-call | < 500 µs UI thread | v2.3.0 |
| pluginval CI strictness-10 | All PASS | v2.2.4 ✅ |
| Mini vs full view parameter parity | 100% null-test cancel | v2.2.3 ✅ |

## v2.2.5 — SIMD Vectorisation

**Goal:** Close the performance gap with FabFilter's hand-optimised assembly.

### The Problem
Current SVF processes bands serially. At 8 bands × 2 channels = 16 sequential
filter operations per sample. FabFilter groups these into hardware vector
registers — 4 bands per SSE instruction or 8 bands per AVX2 instruction.

### Implementation Plan

**Target file:** `Source/DSP/SvfBiquad.h` — add `SvfBandArray<N>` template

```cpp
// Processes N SVF bands simultaneously using JUCE SIMD wrappers.
// SampleType = juce::dsp::SIMDRegister<float> (4 or 8 lanes)
template <int N>
struct SvfBandArray
{
    // State: N bands packed into ceil(N/lanes) SIMD registers
    using Vec = juce::dsp::SIMDRegister<float>;
    static constexpr int lanes = Vec::SIMDNumElements; // 4 (SSE) or 8 (AVX2)
    static constexpr int vecs  = (N + lanes - 1) / lanes;

    alignas(32) std::array<Vec, vecs> ic1eq {}, ic2eq {};
    alignas(32) std::array<Vec, vecs> a1 {}, a2 {}, a3 {};
    alignas(32) std::array<Vec, vecs> m0 {}, m1 {}, m2 {};

    // processBlock: input[N] → output[N], all N bands in ceil(N/lanes) passes
    void processBlock(const float* in, float* out, int numSamples);
};
```

**Projected improvement:** 2–4× throughput on 8-band path (SSE/AVX2).
At 44.1 kHz/512 block: SVF 8-band from 70.4 ns/sample → ~20–35 ns/sample.
100+ simultaneous ProEQ8 instances on a single modern CPU core.

### ARM Neon (Apple Silicon)
`juce::dsp::SIMDRegister<float>` auto-selects Neon on ARM.
Same code, different width: 4-wide float32×4.
M1 Pro projected: 8-band SVF stereo < 15 ns/sample.

---

## v2.2.6 — Spectral Dynamics + Dolby Atmos

### Spectral Dynamics (Soothe2 territory — ProEQ8 exclusive)

**Concept:** Beyond per-band dynamic EQ (one threshold per biquad band),
per-bin FFT threshold clamping acts on individual frequency bins.
Surgical resonance suppression without scooping adjacent clean frequencies.

**Implementation — leverages existing Match EQ infrastructure:**
```
Audio block
    │
    ▼ (overlap-add FFT — already in MatchEQ.h, reuse)
Frequency-domain block [numBins]
    │
    ▼ per-bin: if |X[k]| > threshold[k], attenuate X[k]
threshold[k] = adaptiveThreshold(k, intentMode, attack, release)
    │
    ▼ (overlap-add synthesis — already implemented)
Output block
```

The `threshold[k]` array is updated at the spectrum analyser cadence (~30 Hz)
from `ResonanceDetector` output — no new infrastructure needed. Just a new
signal path through the existing FFT machinery.

**CPU estimate:** ~2× current Match EQ cost per active spectral dynamics band.
Well within budget given v2.2.5 SIMD improvements.

### Dolby Atmos 9.1.6 (ProEQ8 exclusive)

**Target:** `isBusesLayoutSupported` in `PluginProcessor.cpp`

```cpp
// Add to supported layouts:
juce::AudioChannelSet::create9point1point6()  // Atmos bed
juce::AudioChannelSet::create7point1point4()  // Atmos lite
```

**Band routing extension:** Add `SpatialZone` enum to `EQBand`:
- `All` (current default)
- `LFE` (subwoofer only)
- `Overhead` (ceiling channels)
- `Surround` (side + rear)
- `Bed` (floor layer only)

User can apply unique SVF filter curves per spatial zone from one plugin instance.

---

## v2.3.0 — Cross-Instance Intelligence (ARC-Core Spine)

**The killer feature.** FabFilter Pro-Q 4's "Instance List" lets you *see* other
instances. ProEQ8 v2.3 lets them *talk* and *negotiate*.

### Architecture

```
DAW Session
├── ProEQ8 Instance 1 (Lead Vocal, 2kHz mud flagged)
│       │
│       └── arcQueue.write({trackId, freqHz=2000, energy=high})
│
├── ProEQ8 Instance 2 (Guitars, 2kHz competing)
│       │
│       └── arcQueue.write({trackId, freqHz=2000, energy=high})
│
└── [ArcDaemon — message thread, async, no audio thread contact]
        │
        ├── Reads both queues via lock-free SPSC
        ├── POST to http://127.0.0.1:8000/api/v1/events/receipt
        └── ARC-Core spine: detects collision at 2kHz
                │
                └── Notifies Instance 2: apply -2.5dB dynamic notch
                    at 2kHz, keyed to Instance 1's vocal presence
```

**Audio thread contract:** `arcQueue.write()` is the only addition.
Ring buffer write: ~15 ns, non-allocating. Audio thread is never blocked.

### ArcDaemon (new file: `Source/ARC_Interface/ArcDaemon.h`)

```cpp
struct ArcCoreEvent
{
    int64_t  timestamp;       // juce::Time::getHighResolutionTicks()
    int      bandIndex;
    float    freqHz;
    float    energyDb;
    char     semanticTag[32]; // "mud", "harshness", etc.
    char     trackId[64];     // DAW track name from processor getName()
};

class ArcDaemon : public juce::Timer
{
public:
    void timerCallback() override  // UI thread, ~10 Hz
    {
        ArcCoreEvent e;
        while (queue.read(e))
            postToSpine(e);       // async HTTPS, fire-and-forget
    }
private:
    juce::AbstractFifo::ScopedRead arcQueue;
    void postToSpine(const ArcCoreEvent& e);
};
```

### ARC-Neuron Training Loop Integration

Every `ArcCoreEvent` emitted by the plugin is a training datapoint:
- `IntentMode` selected
- `ResonanceDetector` suggestion shown
- Whether user accepted/rejected/modified the suggestion
- Final band settings after interaction

The ARC-Neuron LLMBuilder reads this event log to fine-tune
a micro-model on authentic human mixing provenance — not scraped data.
The model learns *your* mixing decisions, not internet averages.

---

## Competitive Analysis (Target: First Place)

| Feature | FabFilter Pro-Q 4 | Soothe2 | **ProEQ8 v2.3** |
|---------|------------------|---------|--------------|
| Filter topology | Proprietary analog-match | Proprietary | **Simper SVF (published)** |
| De-cramping | Custom polynomial | N/A | **g=tan(π·fc/fs) exact** |
| Dynamic EQ | Per-band threshold | Per-bin spectral | **Both (v2.2.6)** |
| Cross-instance | Visual list only | None | **Active negotiation (v2.3)** |
| SIMD | Manual AVX/SSE | Unknown | **JUCE SIMDRegister (v2.2.5)** |
| Atmos | No | No | **9.1.6 (v2.2.6)** |
| Semantic labels | None | None | **Deterministic, allocation-free** |
| Open source | No | No | **GPL-3.0 core** |
| Price | $179 | $149 | **$0 / $20** |
| AI training loop | None | None | **ARC-Neuron (v2.3)** |

---

## Publication Plan

1. **v2.2.3 release** → post PAPER.md to dev.to, KVR Audio, JUCE forums
2. **v2.3.0 launch** → Product Hunt, Hacker News, social proof from Rekkerd
3. **DAFx 2026 submission** → deadline typically July; submit PAPER.md draft
4. **AES 159th Convention** → New York; audio engineering audience
5. **arXiv cs.SD preprint** → immediately after v2.3.0 release for citability

**Paper title:** "De-cramped Parametric EQ with Lock-Free Semantic Mix
Assistance: Architecture and Benchmark Results"

**The hook:** The only peer-reviewed paper documenting a commercial-grade audio
plugin built with lock-free SPSC concurrency + state-space SVF filtering +
deterministic semantic analysis — all open-source and benchmarked.

---

## FreeEQ8 (Free Tier) — Feature Freeze Policy

After v2.3.0, FreeEQ8 receives:
- Bug fixes (all versions)
- Security patches (all versions)
- Smart EQ layer updates (ResonanceDetector improvements)

FreeEQ8 does NOT receive:
- SVF engine (ProEQ8 exclusive)
- 24-band layout (ProEQ8 exclusive)
- Spectral dynamics (ProEQ8 exclusive)
- Atmos support (ProEQ8 exclusive)
- Cross-instance spine (ProEQ8 exclusive)

**Rationale:** FreeEQ8's RBJ engine is correct, well-tested, and fully
functional. The cramping limitation is documented. At $0 the tradeoff is
transparent. The Smart EQ layer remains free because it runs on top of the
analyser, not the filter engine, and is the strongest marketing hook for
converting free users to ProEQ8.
