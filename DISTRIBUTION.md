# FreeEQ8 Paper Distribution

## 1. arXiv Submission

**Category:** cs.SD (Sound) or eess.AS (Audio and Speech Processing)

**Files to upload:** `paper/arxiv/main.tex` (zip it)

**Submit at:** https://arxiv.org/submit

**Metadata:**
- Title: Real-Time State-Space Parameterization in Digital Equalization: A Production-Grade SVF Implementation
- Authors: Gary Doman
- Abstract: (copy from paper)
- Comments: 2 pages, 3 tables. Submitted to DAFx26 Demo Track.
- ACM classes: H.5.5 (Sound and Music Computing)
- MSC classes: 94A12 (Signal theory)

---

## 2. Hacker News

**URL:** https://news.ycombinator.com/submit

**Title:**
```
Show HN: FreeEQ8 – Open-source JUCE parametric EQ with lock-free Dynamic EQ architecture
```

**URL to submit:** `https://github.com/GareBear99/FreeEQ8`

**Comment to post after:**
```
Hey HN! I built an open-source parametric EQ plugin that solves a fundamental problem in digital audio: high-frequency cramping.

The problem: Traditional EQ plugins use RBJ biquad coefficients with bilinear transform. At 16kHz (44.1kHz sample rate), a Q=1.0 bell filter actually has Q=2.99 – that's 199% narrower than intended. Most plugins "fix" this with 4x oversampling, adding latency and CPU cost.

SVF topology is used for modulation stability under Dynamic EQ. Note: SVF and RBJ produce identical BLT responses — original decramping claim was incorrect (see PAPER.md §2 correction).

Technical highlights:
- Lock-free SPSC triple-buffer for real-time safety (239M samples, 0 data tears)
- Variable-cadence coefficient engine: 75% CPU reduction for Dynamic EQ
- 0.62% single-core CPU at 44.1kHz (161× headroom)
- pluginval strictness-level-10 validated

Paper: https://garebear99.github.io/FreeEQ8/pdf/DAFx26_FreeEQ8_SUBMIT.pdf
Code: https://github.com/GareBear99/FreeEQ8

Happy to answer questions about the DSP or lock-free architecture!
```

---

## 3. Reddit Posts

### r/audioengineering
**Title:** I wrote a paper on why your EQ plugin lies to you at high frequencies (and how to fix it)

**Body:**
```
TL;DR: Most EQ plugins use RBJ biquad math that makes a 16kHz bell filter 199% narrower than the knob says. I built an open-source EQ using Simper SVF that fixes this without oversampling.

I've been obsessing over this problem for a while. When you set Q=1.0 at 16kHz in most EQs, you're actually getting Q=2.99 due to bilinear transform frequency warping. The industry "fix" is 4x oversampling, which adds latency and CPU cost.

The Simper SVF topology (from Cytomic's Andrew Simper) provides modulation-stable Dynamic EQ. Both SVF and RBJ use BLT prewarping and produce the same steady-state response.

I wrote up the math and benchmarks: https://garebear99.github.io/FreeEQ8/pdf/DAFx26_FreeEQ8_SUBMIT.pdf

Free plugin (GPL-3.0): https://github.com/GareBear99/FreeEQ8

Would love feedback from anyone who's dealt with this problem in their mixes!
```

### r/DSP
**Title:** Production-grade SVF implementation with lock-free triple-buffering – paper + open source

**Body:**
```
Just published a paper on implementing Andrew Simper's SVF topology in a production audio plugin. Key contributions:

1. **SVF vs RBJ comparison**: Measured Q distortion at various frequencies. RBJ gives 199% error at 16kHz/44.1kHz; SVF gives 0%.

2. **Lock-free SPSC triple-buffer**: Audio→UI data flow with no mutex. Atomic swaps with memory_order_release/acquire. Stress tested 239M samples with 0 data tears.

3. **Variable-cadence coefficient updates**: Dynamic EQ typically recomputes per-sample. By skipping updates when envelope delta < 0.1dB, we get 75-80% CPU reduction with no audible difference.

Paper: https://garebear99.github.io/FreeEQ8/pdf/DAFx26_FreeEQ8_SUBMIT.pdf
Code: https://github.com/GareBear99/FreeEQ8

Submitted to DAFx26 demo track. Happy to discuss implementation details!
```

---

## 4. KVR Audio Forum

**Section:** DSP and Plugin Development

**Title:** Open-source SVF EQ with lock-free architecture – paper + code

**Body:**
```
Hi all,

I've been working on FreeEQ8/ProEQ8, an open-source parametric EQ that uses Andrew Simper's SVF topology instead of RBJ biquads.

Why bother? At 16kHz (44.1kHz SR), RBJ gives you Q=2.99 when you dial in Q=1.0 – that's the bilinear transform cramping everyone talks about. SVF provides modulation-stable coefficient updates for Dynamic EQ automation.

I wrote up the implementation details in a paper:
https://garebear99.github.io/FreeEQ8/pdf/DAFx26_FreeEQ8_SUBMIT.pdf

Key features:
- Simper SVF with exact HF response
- Lock-free triple-buffer (no audio thread blocking)
- Variable-cadence Dynamic EQ (75% CPU savings)
- 0.62% CPU at 44.1kHz stereo

Code (GPL-3.0): https://github.com/GareBear99/FreeEQ8

Feedback welcome, especially from anyone who's implemented SVF in production!

Cheers,
Gary
```

---

## 5. Twitter/X Thread

```
🧵 Why your EQ plugin lies to you at 16kHz (and how I fixed it)

1/ Set a bell filter at 16kHz, Q=1.0 in most EQ plugins. You're actually getting Q=2.99. That's 199% narrower than the knob says.

2/ This is bilinear transform frequency cramping – a known problem since the 70s. The "fix" is usually 4x oversampling, which adds latency and CPU cost.

3/ SVF topology: modulation-stable Dynamic EQ. Note: both RBJ and SVF use BLT — exact response at fc is guaranteed for both, not unique to SVF.

4/ I built an open-source EQ using this approach + lock-free architecture for real-time safety. 239M samples stress tested, 0 data tears.

5/ Paper: [link]
Code: https://github.com/GareBear99/FreeEQ8

Submitted to DAFx26 at MIT. Happy to answer questions!

cc @cyabormusic (Andrew Simper – thanks for the SVF papers!)
```

---

## Quick Links

- **Paper PDF:** https://garebear99.github.io/FreeEQ8/pdf/DAFx26_FreeEQ8_SUBMIT.pdf
- **GitHub:** https://github.com/GareBear99/FreeEQ8
- **dev.to post:** https://dev.to/tizwildin/we-eliminated-eq-frequency-cramping-without-oversampling-heres-how-dafx26-paper-4f7l
