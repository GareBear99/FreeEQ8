# ABX Listening Study Protocol
## Variable-Cadence Dynamic EQ Perceptual Transparency Validation

**Document Version:** 1.0  
**Last Updated:** 2024  
**Study Type:** ABX Forced-Choice Discrimination Test

---

## 1. Purpose and Research Question

### 1.1 Objective
To determine whether the variable-cadence coefficient update optimization in FreeEQ8's 8-band dynamic equalizer produces perceptually distinguishable differences from the reference per-sample update implementation.

### 1.2 Research Hypothesis
**H₀ (Null):** Listeners cannot distinguish between per-sample coefficient updates (Path A) and variable-cadence updates (Path B) at rates above chance (50%).

**H₁ (Alternative):** Listeners can reliably distinguish between the two processing paths.

### 1.3 Expected Outcome
Based on the technical analysis (0.1 dB threshold, 4-sample maximum batch, ≤90μs update latency), we expect the null hypothesis to hold, demonstrating perceptual transparency of the optimization.

---

## 2. Study Design

### 2.1 Paradigm
**ABX Forced-Choice Test** — the gold standard for perceptual difference detection in audio research.

- Participant hears stimulus A (reference)
- Participant hears stimulus B (optimized)  
- Participant hears stimulus X (randomly selected A or B)
- Participant must identify whether X matches A or B

### 2.2 Signal Processing Paths
| Path | Description | Update Strategy |
|------|-------------|-----------------|
| **A** | Reference | Per-sample coefficient recalculation (44,100 `bq.set()` calls/sec per band) |
| **B** | Optimized | Variable-cadence: update when Δgain > 0.1 dB OR every 4 samples |

### 2.3 Dynamic EQ Configuration
- **Bands:** 8 (center frequencies: 100, 300, 600, 1000, 2000, 4000, 8000, 12000 Hz)
- **Filter type:** SVF Bell (Simper/Cytomic topology)
- **Q:** 1.0 (fixed)
- **Attack time:** 10 ms
- **Release time:** 100 ms
- **Threshold:** -20 dBFS
- **Ratio:** 0.75 dB reduction per dB above threshold

---

## 3. Stimuli

### 3.1 Test Signals
Four programmatically synthesized stimuli designed to stress-test different aspects of dynamic processing:

| Stimulus | Description | Challenge Mode |
|----------|-------------|----------------|
| **Sustained Sine** | 440 Hz pure tone, 10 sec | Minimal envelope variation — tests steady-state accuracy |
| **Drum Loop** | 2 Hz impulse train with 100 Hz fundamental, exponential decay, click transients | Fast transients — tests cadence response time |
| **Vocal Simulation** | Formant-filtered glottal pulse (F1=500Hz, F2=1.5kHz, F3=2.5kHz) with 3 Hz amplitude modulation | Complex spectral content — tests multi-band interaction |
| **Full Mix** | Weighted sum of above three signals | Real-world approximation — tests combined behavior |

### 3.2 Synthesis Parameters
```
Sample rate:     44,100 Hz
Bit depth:       16-bit signed integer (PCM)
Duration:        10 seconds per stimulus
Output format:   Raw PCM (mono, little-endian)
Peak level:      -3 dBFS nominal
```

### 3.3 Reproducibility
All stimuli use deterministic PRNG (xorshift with fixed seed) to ensure identical source material for both processing paths within each stimulus type.

---

## 4. Procedure

### 4.1 Session Structure
```
1. Welcome and informed consent
2. Equipment verification (headphone check, volume calibration)
3. Training phase: 4 practice trials (1 per stimulus type, feedback provided)
4. Test phase: 40 trials (10 per stimulus type, randomized order)
5. Debrief and optional qualitative feedback
```

### 4.2 Trial Structure
```
[500 ms silence]
[Play A — 10 sec]
[1000 ms silence]
[Play B — 10 sec]
[1000 ms silence]  
[Play X — 10 sec]
[Await response]
[500 ms inter-trial interval]
```

Total time per trial: ~35 seconds  
Total test duration: ~25 minutes (excluding breaks)

### 4.3 Response Collection
- Binary forced choice: "1" (X = A) or "2" (X = B)
- Response time recorded (milliseconds from X offset to keypress)
- No time limit, but participants encouraged to respond within 10 seconds
- Participant may request replay (not recommended, tracked in data)

### 4.4 Randomization
- X assignment: Balanced (5× X=A, 5× X=B per stimulus type)
- Trial order: Fisher-Yates shuffle across all 40 trials
- Seed: System time at session start (recorded for reproducibility)

---

## 5. Equipment Requirements

### 5.1 Listening Environment
- **Minimum:** Quiet room with ambient noise < 35 dB SPL
- **Recommended:** Acoustically treated space or professional studio
- **Not acceptable:** Open office, public space, outdoors

### 5.2 Playback Equipment
- **Headphones:** Closed-back or open-back circumaural
  - Minimum: flat response ±3 dB from 80 Hz–16 kHz
  - Recommended: Reference-grade (Sennheiser HD600/650, Beyerdynamic DT880, AKG K701, or equivalent)
- **Audio interface:** Any with ≤1 ms latency, proper driver support
- **Not acceptable:** Laptop speakers, earbuds, Bluetooth audio

### 5.3 Calibration
- Playback level: 75 dB SPL ±2 dB (calibrated with 1 kHz reference tone)
- If calibration equipment unavailable: Comfortable listening level, consistent across session

### 5.4 Software Requirements
- Operating system: macOS, Linux, or Windows with terminal access
- Audio playback: `afplay` (macOS), `aplay` (Linux), or equivalent raw PCM player
- No additional DSP, EQ, or "enhancement" features enabled

---

## 6. Participants

### 6.1 Sample Size Justification
**Target N = 20 participants**

Power analysis (binomial test):
- Effect size of interest: 10% deviation from chance (60% hit rate = detectable difference)
- α = 0.05 (two-tailed)
- Power (1-β) = 0.80
- Required N ≈ 18 participants × 40 trials each

With 20 participants × 40 trials = 800 total observations, we achieve:
- 95% CI width ≈ ±3.5% for overall hit rate
- Statistical power > 0.85 for detecting d' > 0.5

### 6.2 Inclusion Criteria
- Age 18–65 years
- Self-reported normal hearing (no diagnosed hearing loss)
- Experience with critical listening (musicians, audio engineers, or audiophiles preferred)
- Native English speaker or fluent in test instructions

### 6.3 Exclusion Criteria
- Known hearing impairment or tinnitus
- Current ear infection or cold affecting hearing
- Participation in hearing study within past 30 days
- Direct involvement in FreeEQ8 development

### 6.4 Recruitment
- Convenience sampling from audio engineering community
- No compensation required (voluntary participation)
- Participants may withdraw at any time without penalty

---

## 7. Data Collection and Privacy

### 7.1 Recorded Variables
| Variable | Type | Description |
|----------|------|-------------|
| `trial` | int | Trial number (1–40) |
| `stimulus` | string | Stimulus type identifier |
| `x_is_a` | int | Ground truth (1 = X was A, 0 = X was B) |
| `user_response` | int | Participant response (1 or 2) |
| `correct` | int | Response accuracy (1 = correct, 0 = incorrect) |
| `response_time_ms` | float | Time from X offset to response |
| `timestamp` | string | ISO 8601 timestamp |

### 7.2 Data Storage
- Results stored as CSV: `abx_results_YYYYMMDD_HHMMSS.csv`
- No personally identifiable information (PII) in data files
- Participant ID: Sequential number only (P01, P02, ...)
- Session metadata stored separately with consent forms

### 7.3 Privacy Protections
- Anonymized data only in publications
- Raw data retained for 5 years per research standards
- Aggregate results only shared publicly
- Participants may request data deletion

---

## 8. Statistical Analysis Plan

### 8.1 Primary Analysis
**Binomial Test** against chance performance (50%)
- H₀: p = 0.5 (no discrimination ability)
- H₁: p ≠ 0.5 (discrimination ability exists)
- Two-tailed, α = 0.05

### 8.2 Secondary Analyses

#### 8.2.1 Sensitivity Index (d')
Signal detection theory metric quantifying discrimination ability:
```
d' = 2 × Z(hit_rate)
```
where Z is the inverse normal CDF.

Interpretation scale (Macmillan & Creelman, 2005):
| d' | Interpretation |
|----|----------------|
| < 0.5 | Negligible (chance-level) |
| 0.5–1.0 | Low (barely discriminable) |
| 1.0–2.0 | Medium (reliably discriminable) |
| > 2.0 | High (easily discriminable) |

#### 8.2.2 Per-Stimulus Analysis
Separate binomial tests for each stimulus type to identify if specific signal characteristics affect discriminability.

#### 8.2.3 Response Time Analysis
Compare mean RT for correct vs. incorrect responses:
- Significantly longer RT for correct responses may indicate genuine discrimination
- Similar RT suggests guessing behavior

### 8.3 Confidence Intervals
Wilson score intervals for proportions (more accurate than normal approximation for rates near 0.5 or sample sizes < 100).

### 8.4 Multiple Comparisons
Bonferroni correction applied for 4 stimulus-type comparisons:
- Adjusted α = 0.05 / 4 = 0.0125 per comparison

---

## 9. Interpretation Framework

### 9.1 Success Criteria for Perceptual Transparency
The variable-cadence optimization is deemed **perceptually transparent** if ALL of the following hold:

1. **Overall hit rate** not significantly different from 50% (p > 0.05)
2. **d' < 0.5** (negligible sensitivity)
3. **No individual stimulus type** shows significant discrimination (after Bonferroni correction)

### 9.2 Reporting Standards
Results will be reported following APA guidelines with:
- Exact p-values (not "p < 0.05")
- Effect sizes (d')
- 95% confidence intervals
- Sample sizes for all comparisons

### 9.3 Publication
Results suitable for inclusion in:
- DAFx (Digital Audio Effects) conference proceedings
- AES (Audio Engineering Society) convention papers
- ICMC (International Computer Music Conference)
- Journal of the Audio Engineering Society

---

## 10. Limitations and Considerations

### 10.1 Known Limitations
1. **Synthetic stimuli:** Real music may reveal differences not captured by synthetic signals
2. **Headphone-only:** Speaker playback in rooms not tested
3. **Short duration:** 10-second excerpts may miss effects in longer-form content
4. **Single parameter set:** Only default cadence parameters (0.1 dB, 4 samples) tested

### 10.2 Future Work
- Extend to real music excerpts (with appropriate licensing)
- Test extreme parameter settings (0.5 dB threshold, 16-sample batch)
- Include participants with varying hearing ability
- Measure physiological correlates (EEG, pupillometry)

---

## 11. References

1. Macmillan, N. A., & Creelman, C. D. (2005). *Detection Theory: A User's Guide* (2nd ed.). Lawrence Erlbaum Associates.

2. ITU-R BS.1116-3. (2015). *Methods for the subjective assessment of small impairments in audio systems*.

3. AES20-1996 (r2007). *AES recommended practice for professional audio — Subjective evaluation of loudspeakers*.

4. Simper, A. (2013). *Solving the continuous SVF equations using trapezoidal integration and equivalent currents*. Cytomic Technical Paper.

---

## 12. Appendix: Quick Start Guide

### For Researchers Running This Study

```bash
# 1. Build the test tool
cd /path/to/FreeEQ8
g++ -std=c++17 -O2 Tests/ABXListeningTest.cpp -o ABXListeningTest -I.

# 2. Run a test session
./ABXListeningTest -n 10   # 10 trials per stimulus = 40 total

# 3. Analyze results
python3 Tests/ABXAnalysis.py abx_results_*.csv

# 4. Generate LaTeX table for paper
python3 Tests/ABXAnalysis.py abx_results_*.csv --json
```

### Audio Playback Commands
```bash
# macOS
afplay -f LEI16 -r 44100 -c 1 /tmp/abx_A.raw

# Linux  
aplay -f S16_LE -r 44100 -c 1 /tmp/abx_A.raw

# Windows (with SoX)
play -t raw -r 44100 -e signed -b 16 -c 1 /tmp/abx_A.raw
```

---

**Document prepared for PAPER.md §5 — Perceptual Validation**

*This protocol enables reproducible perceptual validation suitable for peer-reviewed publication in audio engineering venues.*
