# Smart EQ Layer — Design & Roadmap

The Smart EQ Layer is FreeEQ8's competitive moat: a real-time decision
layer that sits *on top* of the existing 8-band parametric engine and
helps users do the right thing instantly, without replacing the
surgical control they already have.

This doc is the source of truth for what's shipped, what's wired, and
what's still roadmap so the claim on the README stays honest.

## Architecture

```
                    ┌─────────────────────────────────────────┐
                    │           SpectrumFIFO (existing)       │
                    │  audio thread → triple-buffered spectrum│
                    └────────────────┬────────────────────────┘
                                     │ magnitudes-dB @ UI rate
                                     ▼
                    ┌─────────────────────────────────────────┐
                    │           ResonanceDetector              │   Source/DSP/
                    │  log-freq rebin → baseline → peak pick   │   ResonanceDetector.h
                    │  → intent weighting → top-N suggestions  │
                    └────────────────┬────────────────────────┘
                                     │
            ┌────────────────────────┼─────────────────────────┐
            ▼                        ▼                         ▼
┌───────────────────────┐ ┌──────────────────────┐ ┌──────────────────────┐
│  ResponseCurve overlay │ │  Explain hover popup │ │ One-click apply band │
│  (glowing suggestion   │ │  via                 │ │ (reuse existing band │
│   nodes on the curve)  │ │  FrequencyExplainer  │ │ parameters)          │
└───────────────────────┘ └──────────────────────┘ └──────────────────────┘
```

## Status

| Feature | Status | Location |
|---|---|---|
| Resonance detector (peak finder) | **Shipped** | `Source/DSP/ResonanceDetector.h` |
| Intent Mode weighting curves | **Shipped** | `Source/DSP/IntentMode.h` |
| Frequency-Explainer semantic map | **Shipped** | `Source/DSP/FrequencyExplainer.h` |
| `intent_mode` APVTS parameter | Next pass | `PluginProcessor::createParams` |
| Editor integration (call detector from UI timer) | Next pass | `PluginEditor.cpp` |
| Response-curve overlay (glowing suggestion nodes) | Next pass | `UI/ResponseCurveComponent.cpp` |
| Explain-on-hover band popup | Next pass | `UI/ResponseCurveComponent.cpp` |
| One-click "apply suggestion" (drop into an empty band) | Next pass | editor band-slot allocator |
| Zero-Lag auto-switch between precision / real-time | Backlog | already half-implemented via `linear_phase` toggle |
| Simplified Match EQ workflow (drag reference → click match → done) | Backlog | `MatchEQ` already exists; this is UX only |

## ResonanceDetector algorithm

1. Take the linear-bin magnitude-in-dB spectrum published by
   `SpectrumFIFO` (2048 bins at the default FFT order).
2. Re-sample into a 96-bin log-frequency grid covering 20 Hz → Nyquist.
3. Estimate a baseline as a ±0.5-octave moving average.
4. Mark each log-bin whose `(magnitude − baseline)` is ≥ +3 dB and is
   the local max in a ±3-bin neighbourhood (~0.6 octaves).
5. Score each peak by `deviation × intentWeight(hz)` where the intent
   weight is a log-frequency Gaussian bump around known problem zones
   (see `IntentMode.h`).
6. Return up to 4 ranked suggestions with:
    - `freqHz`  — peak centre in Hz
    - `gainDb`  — recommended cut (−3 dB … −12 dB)
    - `q`       — recommended Q (2 … 8) from peak sharpness
    - `confidence` — 0..1 for UI alpha blending
    - `label`   — short semantic tag ("mud", "harshness", etc.)

The algorithm is strictly RT-friendly: no allocation, no
indeterminate loops, and can be called at the spectrum update rate on
the UI thread without touching the audio thread.

## Intent Mode weighting

`intentWeightFor(mode, hz)` returns a multiplier in roughly 0.5 … 2.5
that gets applied to each peak's deviation score. The bumps are:

| Mode | Primary bump | Secondary bump |
|---|---|---|
| `None`         | flat 1.0 everywhere | — |
| `VocalClean`   | 300 Hz ×1.6 (mud) | 3.2 kHz ×1.5 (harshness) |
| `DrumPunch`    | 300 Hz ×1.5 (box) | 7.5 kHz ×1.4 (cymbal ring) |
| `GuitarSpace`  | 250 Hz ×1.5 (mud) | 2.5 kHz ×1.5 (honk) |
| `MasterPolish` | 250 Hz ×1.3 | 12 kHz ×1.2 (air ring) |

This is behavioural biasing, not preset band placement — the user
retains full surgical control.

## Why this is the moat

There is currently no free open-source 8-band EQ that offers:

- **auto-detection of resonance peaks** with intent-aware ranking
- **explain-on-hover** semantic descriptions
- **one-click apply** of a detected peak to an empty band slot

Competitors either hard-code preset bands (Nova), require paid
intelligence layers (Pro-Q 3+), or ship as proof-of-concept research
(various GitHub EQs). FreeEQ8 targets the intersection directly.

## Next commit (planned)

1. Add `intent_mode` as an `AudioParameterChoice` on APVTS.
2. Add a `ResonanceDetector` member to `FreeEQ8AudioProcessor` and call
   `setSampleRate()` from `prepareToPlay`.
3. In the editor's spectrum-update timer, after
   `spectrumFifo.processIfReady()` returns true, call
   `detector.analyse(spectrumFifo.getMagnitudes(), spectrumFifo.getNumBins())`.
4. Extend `ResponseCurveComponent` to render the published suggestions
   as glowing nodes. Right-click (or click) → drop the suggestion into
   the next unused band.
5. Add a small intent-mode dropdown to the top of the editor.
