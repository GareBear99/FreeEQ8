# ResonanceDetector Evaluation Framework

This document describes the evaluation methodology for the `ResonanceDetector` component, which identifies problematic frequency resonances in audio spectra and suggests corrective EQ cuts.

## Overview

The evaluation framework tests the detector's ability to identify planted resonances in realistic synthesized audio signals. Unlike simple Gaussian peak tests, this framework uses time-domain synthesis with resonant filters to create spectra that better approximate real-world audio content.

## Signal Synthesis Methodology

### Synthesis Pipeline

Each test signal follows this processing chain:

1. **Excitation generation**: Noise, impulse trains, or harmonic waveforms appropriate to the signal type
2. **Spectral shaping**: Bandpass/highpass/lowpass filtering to create realistic spectral envelopes
3. **Resonance injection**: 2-pole resonant filters at known frequencies to plant detectable peaks
4. **FFT analysis**: 4096-point FFT with Hann window at 44.1 kHz

### Test Signals

The corpus includes 10 test signals covering common mixing scenarios:

| Signal | Description | Planted Resonances |
|--------|-------------|-------------------|
| `vocal_sim` | Filtered noise + mud/presence issues | 300 Hz (mud), 3200 Hz (harshness) |
| `vocal_clean` | Clean vocal simulation (negative case) | None |
| `drum_bus` | Impulse train with boxiness | 400 Hz (boxiness), 8000 Hz (ring) |
| `kick_heavy` | Heavy kick with sub emphasis | 60 Hz (sub), 250 Hz (mud) |
| `bass_di` | Bass guitar DI | 80 Hz (thump), 800 Hz (honk) |
| `guitar_amp` | Comb-filtered amp simulation | 1200 Hz (nasal), 2500 Hz (honk) |
| `acoustic_guitar` | Body resonance simulation | 180 Hz (body), 3000 Hz (brightness) |
| `full_mix` | Mixed sources | 280 Hz, 700 Hz, 4500 Hz |
| `synth_pad` | Sustained harmonic content | 440 Hz, 880 Hz |
| `harsh_master` | Over-compressed mix | 2800 Hz, 6000 Hz, 10000 Hz |

### Resonance Injection

Resonances are injected using 2-pole bandpass filters:

```cpp
struct Resonator {
    void set(double fc, double q, double sr) {
        double w = 2.0 * PI * fc / sr;
        double bw = w / q;
        double r = std::exp(-bw);
        a1 = -2.0 * r * std::cos(w);
        a2 = r * r;
        b0 = (1.0 - r * r) * 0.5;
    }
};
```

Q values range from 2.0 to 6.0 depending on signal type, producing peaks with realistic sharpness.

## Ground Truth Annotation

### Annotation Format

Ground truth is stored in `Tests/data/ground_truth.json`:

```json
{
  "signals": [
    {
      "name": "vocal_sim",
      "resonances": [
        {"freq_hz": 300, "type": "mud", "deviation_db": 8, "q_hint": 3.0}
      ],
      "intent": "VocalClean"
    }
  ]
}
```

### Matching Criteria

A detection is considered a true positive if:
- The detected frequency is within **±0.15 octaves** of a planted resonance
- This tolerance (~11% frequency deviation) accounts for FFT bin quantization and spectral spreading

## Evaluation Metrics

### Per-Signal Metrics

For each test signal:
- **TP (True Positives)**: Detected suggestions matching planted resonances
- **FP (False Positives)**: Detected suggestions not matching any planted resonance
- **FN (False Negatives)**: Planted resonances not detected

### Aggregate Metrics

- **Precision** = TP / (TP + FP) — "How many detections were correct?"
- **Recall** = TP / (TP + FN) — "How many resonances were found?"
- **F1 Score** = 2 × (P × R) / (P + R) — Harmonic mean of precision and recall
- **PR-AUC** — Area under precision-recall curve (simulated)

### Success Criteria

**F1 ≥ 0.70** — Balances precision and recall for practical utility

## Results

### Aggregate Performance

```
Precision:     100.0%
Recall:        90.0%
F1 Score:      94.7%
PR-AUC:        0.985
True Positives:  18
False Positives: 0
False Negatives: 2
```

The two false negatives occur in low-frequency detection:
- `kick_heavy`: Missed 60 Hz sub resonance (below detector's effective range)
- `bass_di`: Missed 80 Hz thump resonance (edge of effective range)

### Performance by Signal Type

See `Tests/detector_plots/per_signal_breakdown.png` for visualization.

### Performance by Intent Mode

The detector uses intent-mode weighting to prioritize frequency ranges relevant to each source type:

- **VocalClean**: Boosts 200-500 Hz (mud) and 2-5 kHz (harshness)
- **DrumPunch**: Boosts 200-500 Hz (box) and 6-10 kHz (ring)
- **GuitarSpace**: Boosts 150-400 Hz (mud) and 1-4 kHz (honk)
- **MasterPolish**: Mild low-mid and air weighting

See `Tests/detector_plots/intent_comparison.png` for breakdown.

## Running the Evaluation

### Build and Run C++ Tool

```bash
cd /path/to/FreeEQ8
g++ -std=c++17 -O2 -I. Tests/RealWorldDetectorEval.cpp -o RealWorldDetectorEval
./RealWorldDetectorEval
```

Output: `Tests/data/detector_results.csv`

### Generate Analysis Plots

```bash
python3 Tests/DetectorROC.py
```

Output:
- `Tests/detector_plots/precision_recall_curve.png`
- `Tests/detector_plots/per_signal_breakdown.png`
- `Tests/detector_plots/intent_comparison.png`
- `Tests/detector_plots/evaluation_summary.txt`

## Limitations

### Synthesis vs. Real Audio

1. **Simplified spectra**: Real audio has complex, time-varying spectra; our synthesis uses stationary signals
2. **Clean resonances**: Planted resonances are isolated; real mixes have overlapping problems
3. **No masking**: Frequency masking effects are not simulated
4. **Single-frame analysis**: Real usage involves temporal averaging

### Evaluation Methodology

1. **Simulated ROC curve**: True ROC requires confidence thresholds; we simulate from the single operating point
2. **Small corpus**: 10 signals is sufficient for validation but not statistically robust
3. **No subjective validation**: Perceptual correctness of suggestions is not evaluated

## Future Work

### Near-Term Improvements

1. **Multi-frame evaluation**: Average multiple FFT frames to reduce variance
2. **Confidence threshold sweep**: Output per-suggestion confidence to enable proper ROC
3. **Larger synthetic corpus**: Generate 100+ variations with randomized parameters

### Long-Term Goals

1. **Real multitrack evaluation**: Test on professional multitrack sessions with human annotations
2. **A/B listening tests**: Validate that suggested cuts improve perceived quality
3. **Cross-validation with other analyzers**: Compare to SPAN, iZotope Insight, etc.

## References

- `Source/DSP/ResonanceDetector.h` — Implementation
- `Source/DSP/IntentMode.h` — Intent weighting curves
- `Tests/DetectorEvalTest.cpp` — Original simple Gaussian-peak tests
- `PAPER.md` §5.1 — Smart EQ detection methodology
