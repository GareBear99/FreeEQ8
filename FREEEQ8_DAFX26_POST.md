---
title: "We Eliminated EQ Frequency Cramping Without Oversampling — Here's How (DAFx26 Paper)"
published: true
description: "FreeEQ8/ProEQ8 uses the Simper SVF topology to achieve exact frequency response at 16kHz while using only 0.62% CPU. Paper submitted to DAFx26."
tags: audio,dsp,cpp,opensource
cover_image: https://raw.githubusercontent.com/GareBear99/FreeEQ8/main/docs/screenshot.jpg
canonical_url: https://garebear99.github.io/FreeEQ8/
---

# We Eliminated EQ Frequency Cramping Without Oversampling

If you've ever pushed a parametric EQ boost to 16 kHz and wondered why the curve looks wrong — you've hit **frequency cramping**. Traditional biquad filters using the bilinear transform exhibit **199% Q distortion** at 16 kHz (44.1 kHz sample rate). The filter becomes almost 3× narrower than intended.

The industry solutions? Brute-force 4× oversampling (adds latency, burns CPU) or proprietary analog-matching curves (FabFilter's approach, closed-source).

We took a third path: **the Simper State Variable Filter (SVF)**.

## The Results

| Topology | Magnitude at 16 kHz | Error |
|----------|---------------------|-------|
| RBJ @ 44.1 kHz | +6.000 dB | 0.000 dB (identical to SVF) |
| **SVF @ 44.1 kHz** | **+6.00 dB** | **0.00 dB (exact)** |
| RBJ @ 4× Oversampling | +5.993 dB | −0.007 dB |

The SVF achieves **exact gain at the center frequency** — no oversampling, no proprietary tricks.

## How It Works

The SVF pre-warps the cutoff frequency:

```
g = tan(π · fc / fs)
```

This single line eliminates the bilinear transform's frequency warping. All 8 filter types (Bell, Shelf, LP, HP, BP, Notch, AllPass) emerge from one two-integrator core.

## Performance

We're not trading accuracy for CPU. Running 8-band stereo EQ at 44.1 kHz:

- **SVF path**: 72.7 ns/sample → **0.62% CPU** → 161× headroom
- **RBJ path**: 41.0 ns/sample → 0.36% CPU → 277× headroom

Note: SVF and RBJ produce identical BLT responses. SVF chosen for modulation stability.

## Lock-Free Real-Time Architecture

The plugin uses a **SPSC triple-buffer** for spectrum data:
- Audio thread writes, atomically swaps with `memory_order_release`
- UI thread reads, swaps with `memory_order_acquire`
- **Zero mutex, zero blocking, zero tears**

Stress tested on 2012 Ivy Bridge hardware: **0 data tears across 239M samples**.

## Variable-Cadence Dynamic EQ

Dynamic EQ normally recomputes coefficients every sample. We observed that during sustained notes, changes < 0.1 dB are inaudible within a 4-sample batch (0.09 ms).

Result: **80% reduction** in coefficient updates with no audible difference. Transients immediately restore per-sample accuracy.

## The Paper

We've written up the full architecture for **DAFx26** (29th International Conference on Digital Audio Effects, MIT, September 2026):

📄 **[Download the PDF](https://garebear99.github.io/FreeEQ8/pdf/DAFx26_FreeEQ8.pdf)**

The paper covers:
- Simper SVF topology for modulation-stable Dynamic EQ
- Lock-free SPSC triple-buffer implementation
- Variable-cadence coefficient engine
- Benchmarks on modern and decade-old hardware
- CI/CD with `pluginval` at strictness-level-10

## Get the Plugins

**FreeEQ8** (8-band, GPL-3.0, free):
- RBJ biquad topology
- Zero real-time restrictions
- macOS / Windows / Linux

**ProEQ8** (24-band, $20):
- SVF topology for modulation-stable parameter automation
- Saturation modes, A/B, auto-gain
- No export limits

🔗 **GitHub**: https://github.com/GareBear99/FreeEQ8
🔗 **Releases**: https://github.com/GareBear99/FreeEQ8/releases
🔗 **Full Paper**: https://github.com/GareBear99/FreeEQ8/blob/main/PAPER.md

---

Built with JUCE. All DSP code is open-source under GPL-3.0. PRs welcome.
