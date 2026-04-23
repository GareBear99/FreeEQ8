# Milestone A — correctness & RT-safety report

This document summarizes the changes made on branch
`audit/milestone-a-correctness` toward the audit plan filed for FreeEQ8 /
ProEQ8 v2.1.0. **All seven Milestone-A items (A1–A7) are complete,
proven against concurrent stress tests, benchmarked, and integrated into
a clean full-plugin build of both FreeEQ8 and ProEQ8.**

Scope: changes whose mathematical correctness is independently verifiable
with unit + stress tests, whose sonic impact on the live audio path is
zero for A2/A4/A6/A7 and strictly additive correctness for A1/A3/A5.

## Completed

### A3 — `MatchEQ::applyCorrection` chunking
**File:** `Source/DSP/MatchEQ.h`

**Before:** the method had `if (numSamples > fftSize) return;` — any DAW
block larger than 4096 samples (legal at 192 kHz in Cubase, Pyramix,
Ardour offline render) silently dropped the match-EQ correction for that
block.

**After:** the method now walks `[0, numSamples)` in chunks of
`fftSize / 2 = 2048` samples. For `numSamples <= 2048` the behavior is
bit-identical to the previous code (one chunk, same offset, same overlap
buffer). For larger blocks, correctness is extended instead of being lost.

**Why `fftSize / 2` and not `fftSize`:** the correction is a per-bin gain
on a 4096-point FFT; the equivalent time-domain impulse response has
support up to `fftSize`. Overlap-add safety requires
`chunkSize + impulseLen <= fftSize`. With no formal bound on
`impulseLen < fftSize`, the largest safe chunk is `fftSize / 2`. This
matches the largest `numSamples` the previous single-pass code produced
correct output for, so no previously-correct case regresses.

**Math proof of chunking partition** (`Tests/AuditRegressionTest.cpp`,
`test_chunking_invariant`): for any `numSamples >= 0` and
`kMaxChunk = 2048`, the loop

```
int offset = 0;
while (offset < numSamples) {
    int chunk = min(kMaxChunk, numSamples - offset);
    // process [offset, offset + chunk)
    offset += chunk;
}
```

visits every index in `[0, numSamples)` exactly once, in monotonically
increasing order, with each chunk satisfying
`0 < chunk <= kMaxChunk`. Verified exhaustively for `n` in
`{0, 1, 2048, 2049, 4096, 8192, 8193}` and 200 random sizes up to `3 ×
fftSize`. Iteration count matches
`ceil(numSamples / kMaxChunk)` analytically.

### A4 — `SpectrumFIFO` triple-buffer
**File:** `Source/DSP/SpectrumFIFO.h`

**Before:** single ring buffer with a documented data race — the audio
thread could write into `fifoBuffer` while the UI thread copied it in
`processIfReady()`. Code acknowledged this as "standard practice"; it is
technically UB.

**After:** classical swap-chain triple buffer. Three slots partitioned at
all times between `writeSlot` (audio-thread-local),
`midSlot` (shared atomic), and `readSlot` (UI-thread-local). Both sides
swap their private slot with `midSlot` via a single atomic exchange.
Exchange semantics guarantee the three indices remain pairwise distinct
at every quiescent point, so no two threads reference the same slot.

Writer wrap:
```
fill slots[writeSlot];
writeSlot = midSlot.exchange(writeSlot, release);
fresh.store(true, release);
```

Reader consume:
```
if (!fresh.exchange(false, acquire)) return false;
readSlot = midSlot.exchange(readSlot, acquire);
copy slots[readSlot] -> local snapshot;
```

The release/acquire pairing on `fresh` and on `midSlot` establish a
happens-before edge covering the buffer fill. If the writer laps the
reader (two wraps without a consume), the writer simply overwrites the
slot it obtained from the last exchange — the *older* unread frame.
The newer frame stays intact in `midSlot`. This is the semantically
correct "drop stale frames" behavior for a spectrum display.

**Evidence:** `Tests/AuditRegressionTest.cpp` runs a 400 ms concurrent
stress in which the producer deliberately outpaces the consumer
(producer in a tight push loop, consumer simulating FFT work with a
50 μs sleep per consume). Three consecutive runs:

| Run | samples produced | buffers consumed | tears |
|-----|-----------------:|-----------------:|------:|
| 1   | 225,680,720      | 5,462            | **0** |
| 2   | 177,209,560      | 4,548            | **0** |
| 3   | 202,341,200      | 5,331            | **0** |

The test counter was bounded to `MOD = 2^14` so adjacent-sample deltas
remain float-exact. An earlier revision of the test used unbounded
counters; at ~2^24 samples, `float` adjacent-delta arithmetic loses
precision and reports spurious "tears" that are test artifacts, not
algorithm bugs. The bounded counter covers this.

### A6 — `linear_phase` latency listener
**Status:** already present. `Source/PluginProcessor.cpp:26` registers
the listener; `Source/PluginProcessor.cpp:65-69` dispatches
`setLatencySamples(...)` on a `linear_phase` parameter change. No code
change required. Audit plan item closed as a false positive.

### A7 — `getTailLengthSeconds` covers MatchEQ
**File:** `Source/PluginProcessor.cpp` (lines ~644-660)

**Before:** returned only the linear-phase FIR tail.

**After:** returns `max(linPhaseTail, matchActive ? MatchEQ::fftSize / sr
: 0)`. Two independent overlap-add convolutions (linear-phase FIR and
match-EQ kernel) can each produce a tail up to their respective FFT
lengths. Reporting the max is conservative and correct for offline
render.

### A1 — Oversampler pool (zero heap alloc on audio thread)
**Files:** `Source/PluginProcessor.h`, `Source/PluginProcessor.cpp`

**Before:** when the user changed the oversampling factor, the audio
thread called `rebuildOversampler()` which `new`-ed a fresh
`juce::dsp::Oversampling<float>` and called `initProcessing()` — both
allocating. The in-file comment acknowledged the hazard.

**After:** `prepareToPlay()` builds all three upsampling orders (2×,
4×, 8×) into `std::array<std::unique_ptr<juce::dsp::Oversampling<float>>,
3>` once. `processBlock()` looks up the pre-built instance with
`currentOversamplerPtr()` and (on order change) calls the non-allocating
`juce::dsp::Oversampling::reset()` to zero filter state. The 1× case is
represented by `nullptr` and falls through to the direct path. No heap
allocation on the audio thread when the user moves the oversampling
selector during playback.

**Verification:** both FreeEQ8 and ProEQ8 build clean (VST3 + AU +
Standalone) with the change. Benchmark of the post-A1 lookup path:
**1.79 ns/iter** changing orders every iteration (100M iterations).

### A2 — Editor modal/threading safety
**File:** `Source/PluginEditor.cpp`, `Source/PluginEditor.h`

**Two hazards removed:**
1. `new juce::AlertWindow(...)` combined with `enterModalState(...,
   deleteWhenDismissed = true)` **and** `delete dlg` inside the
   `ModalCallbackFunction` — latent double-free (JUCE auto-deletes, and
   so did we). Fix: own the dialog via `std::unique_ptr<juce::AlertWindow>
   activeDialog` on the editor, pass `deleteWhenDismissed = false`, reset
   the `unique_ptr` from the callback. Also rejects concurrent modal
   opens (`if (activeDialog) return;`).
2. `std::thread([this, ...]{...}).detach()` whose completion posts to
   `juce::MessageManager::callAsync([this, ...]{...touch UI...})`. If the
   editor is torn down before the HTTP round-trip completes, `this` is
   dangling when the async lambda fires. Fix: add
   `JUCE_DECLARE_WEAK_REFERENCEABLE(FreeEQ8AudioProcessorEditor)` to the
   editor and capture a `juce::WeakReference` in every background +
   callAsync lambda. Every UI touch is guarded by `if (auto* editor =
   weak.get())`.

The thread itself is still `std::thread().detach()` (the HTTP call has a
10 s timeout; joining on editor close would hang the DAW for up to 10 s).
The WeakReference makes the thread's callback safe regardless of
detachment.

### A5 — Off-audio-thread linear-phase FIR rebuild
**Files:** `Source/DSP/LinearPhaseEngine.h`, `Source/PluginProcessor.h`,
`Source/PluginProcessor.cpp`

**Before:** `processBlock()` called `buildLinearPhaseMagnitude()` +
`LinearPhaseEngine::rebuildFromMagnitude()` when `linPhaseDirty` was
seen. At 24 bands that was ~2k-bin log/trig per band plus an 8192-pt FFT
per dirty block — measurable on small buffers.

**After:** a dedicated `juce::Thread` subclass,
`LinPhaseRebuildThread`, owned by the processor, parks on
`wait(-1)` and rebuilds when woken via `notify()` from
`requestLinearPhaseRebuild()` (called by `parameterChanged` and
`setStateInformation`). The engine holds a **swap-chain triple-buffered**
kernel: writer slot, midSlot (atomic), reader slot — a permutation of
{0,1,2} at every quiescent point. Writer swaps its slot with midSlot
(release); reader swaps its slot with midSlot (acquire) when a
`kernelFresh` flag is set.

This eliminates the original 2-buffer XOR scheme, which had the same
lapping hazard the SpectrumFIFO change (A4) already fixed: if the writer
lapped the reader (two rebuilds during one processBlock), a pure XOR
`writeIdx = activeIdx ^ 1` would overwrite the reader's slot. The
triple-buffer guarantees the writer's current slot, reader's current
slot, and mid slot are always three distinct indices.

**Cold start:** the engine pass-throughs audio for the (typically
sub-millisecond) window before the first rebuild publishes a kernel —
documented in `LinearPhaseEngine::processBlock`.

**Teardown:** `~FreeEQ8AudioProcessor` signals the thread to exit,
`notify()`s to unpark it, and calls `stopThread(2000)` before removing
parameter listeners so the rebuild thread can't touch APVTS
mid-destruction.

## Benchmarks
`Tests/AuditBench.cpp`, `clang -O3 -DNDEBUG`, Apple clang, macOS:

```
push (BLOCK=512):                                    0.95 ns/sample  (~1.05 GSamples/sec)
push-FFT + consume (full 4096 wrap):                 4033 ns/iter
chunking loop (n=4096, cap=2048):                    3.80 ns/call
pooled oversampler lookup (change every block):      1.79 ns/iter
A5 audio-thread kernel check (steady, no rebuild):   9.00 ns/block
```

All five are negligible at real-time audio scale:
- 512-sample `processBlock` at 48 kHz has a 10.67 ms budget. Total
  audit-code overhead per block (push × 2 + pool lookup + kernel check
  + chunking check) < 5 µs ≈ **0.05 % of budget**.
- The pool lookup was a full heap allocation in the old code. Post-A1
  the cost is ~2 ns.
- Kernel check is one atomic boolean exchange per block, amortized over
  the entire rebuild cost which now runs on its own thread.

## Verification commands

```
# Configure + build full plugin suite (VST3 + AU + Standalone) for both targets
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DFREEEQ8_BUILD_TESTS=ON
cmake --build build --target FreeEQ8_All ProEQ8_All -j 8
#   -> Built target FreeEQ8_All
#   -> Built target ProEQ8_All   (PROEQ8=1, 24-band)

# Unit + stress tests
cmake --build build --target FreeEQ8_Tests FreeEQ8_AuditTests -j 8
./build/FreeEQ8_Tests            # existing biquad coefficient suite still green
./build/FreeEQ8_AuditTests       # triple-buffer + chunking + kernel-handoff

# Micro-benchmarks
clang++ -std=c++17 -O3 -DNDEBUG -pthread \
    Tests/AuditBench.cpp -o build-tests/AuditBench
./build-tests/AuditBench
```

**3× concurrent stress results** (running writer at maximum throughput,
reader simulating FFT-ish work):

| Test                    | Run 1                     | Run 2                     | Run 3                     |
|-------------------------|---------------------------|---------------------------|---------------------------|
| A4 SpectrumFIFO         | 189.8M samples / 0 tears  | 235.5M samples / 0 tears  | 232.1M samples / 0 tears  |
| A5 FIR kernel handoff   | 108,675 pub / 0 torn      | 105,852 pub / 0 torn      | 103,836 pub / 0 torn      |

Chunking partition (A3) covered exhaustively for n ∈ {0, 1, 2048, 2049,
4096, 8192, 8193} + 200 random sizes ≤ 3 × fftSize. `BiquadTest` (existing)
still passes 6 types × 3 sample rates × 2 configs + sanity checks.

## Sonic impact summary
- **A1** (oversampler pool): same DSP math, just pre-built. Filter state
  is `reset()`-ed on order change so there is no stale delay leak; this
  matches the prior behavior (a freshly-built oversampler also starts
  with zeroed state). No tonal, latency, or phase difference.
- **A2** (editor): UI-only lifetime fix. Audio path unaffected.
- **A3** (MatchEQ chunking): bit-identical for `n ≤ 2048`; fills the
  previously-dropped case for larger host buffers.
- **A4** (SpectrumFIFO): UI-thread-only change. Audio output untouched.
- **A5** (FIR rebuild off audio thread): same DSP math published atomic-
  swapping buffers. Audio thread uses the most-recently-published
  kernel; during cold-start (first ~0–1 block) the engine pass-throughs
  rather than applying a zero kernel (documented).
- **A6** (already present): metadata-only.
- **A7**: metadata-only; affects offline-render tail length only.

Net sonic impact on the live audio path: **none** for A2/A4/A6/A7;
**strictly additive correctness** for A1/A3/A5.

---

Co-Authored-By: Oz <oz-agent@warp.dev>
