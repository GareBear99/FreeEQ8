#pragma once
#include <array>
#include <atomic>
#include <cmath>
#include <algorithm>
#include "IntentMode.h"

// ============================================================================
// ResonanceDetector
// ----------------------------------------------------------------------------
// Finds the most prominent resonances ("mud", "boxiness", "harshness",
// "ringing") in a magnitude-in-dB spectrum and emits up to `kMaxSuggestions`
// ranked suggestion bands.
//
// Algorithm (log-frequency, smoothed-baseline peak ranking):
//   1. Convert the linear spectrum to a 1/3-octave log-frequency grid so the
//      analysis isn't dominated by HF bin density.
//   2. Estimate a slow-moving baseline (1-octave running average).
//   3. For every log-bin compute deviation = mag - baseline. Positive deviations
//      of at least +3 dB with no higher-deviation neighbour within one octave
//      are marked as peaks.
//   4. Apply the intent-mode weighting curve (vocal / drum / guitar / master)
//      to each peak so the *right* peaks rank first.
//   5. Return the top-N peaks by weighted score with recommended
//      cut-gain = -min(12 dB, deviation - 3) and Q = 2..8 depending on the
//      peak's sharpness.
//
// This class is non-allocating on the analysis path and can be called from the
// UI thread at the spectrum-update rate (~20-30 Hz). Call `analyse(...)` with
// the magnitudes-in-dB array from SpectrumFIFO and then read `getSuggestions()`.
// ============================================================================
class ResonanceDetector
{
public:
    static constexpr int kMaxSuggestions = 4;
    static constexpr int kLogBins        = 96;   // ~1/8-octave from 20 Hz to SR/2

    struct Suggestion
    {
        float freqHz     = 0.0f;
        float gainDb     = 0.0f;   // negative (cut)
        float q          = 2.0f;
        float confidence = 0.0f;   // 0..1
        const char* label = "";    // short semantic tag e.g. "mud", "harshness"
    };

    ResonanceDetector() = default;

    void setSampleRate(double sr) noexcept
    {
        sampleRate = sr > 0.0 ? sr : 44100.0;
        buildLogGrid();
    }

    void setIntent(IntentMode mode) noexcept
    {
        intent = mode;
    }

    // magnitudes are in dB, length = numBins (linear bin spacing, bin i = i * sr / (2*numBins))
    void analyse(const float* magnitudes, int numBins) noexcept
    {
        if (numBins <= 1 || !magnitudes) return;

        // 1. Log-bin re-sampling (max magnitude within each log bin).
        for (int i = 0; i < kLogBins; ++i)
        {
            const int lo = logBinStart[(size_t) i];
            const int hi = std::min(numBins, logBinEnd[(size_t) i]);
            float maxDb = -120.0f;
            for (int k = lo; k < hi; ++k)
                maxDb = std::max(maxDb, magnitudes[k]);
            logSpectrum[(size_t) i] = maxDb;
        }

        // 2. One-octave running baseline (moving average).
        constexpr int halfOctaveBins = 4;  // ≈ ±0.5 octaves
        for (int i = 0; i < kLogBins; ++i)
        {
            const int lo = std::max(0, i - halfOctaveBins);
            const int hi = std::min(kLogBins - 1, i + halfOctaveBins);
            float sum = 0.0f;
            for (int k = lo; k <= hi; ++k) sum += logSpectrum[(size_t) k];
            baseline[(size_t) i] = sum / (float) (hi - lo + 1);
        }

        // 3. Peak extraction (local-max over 1/2-octave neighbourhood, >= +3 dB over baseline)
        struct Peak { float freqHz, deviation, sharpness; };
        std::array<Peak, kLogBins> peaks {};
        int peakCount = 0;
        constexpr int neighbour = 3;
        for (int i = 1; i < kLogBins - 1; ++i)
        {
            const float dev = logSpectrum[(size_t) i] - baseline[(size_t) i];
            if (dev < 3.0f) continue;
            bool isLocalMax = true;
            for (int k = std::max(0, i - neighbour); k <= std::min(kLogBins - 1, i + neighbour); ++k)
            {
                if (k == i) continue;
                if (logSpectrum[(size_t) k] - baseline[(size_t) k] > dev) { isLocalMax = false; break; }
            }
            if (!isLocalMax) continue;

            // Sharpness proxy = max drop to neighbours 1 bin away (in dB).
            const float left  = logSpectrum[(size_t) (i - 1)];
            const float right = logSpectrum[(size_t) (i + 1)];
            const float drop  = logSpectrum[(size_t) i] - 0.5f * (left + right);

            peaks[(size_t) peakCount++] = { logBinCenterHz[(size_t) i], dev, drop };
            if (peakCount >= (int) peaks.size()) break;
        }

        // 4. Apply intent weighting + rank.
        struct Scored { Peak peak; float score; };
        std::array<Scored, kLogBins> scored {};
        int scoredCount = 0;
        for (int i = 0; i < peakCount; ++i)
        {
            const float w = intentWeight(peaks[(size_t) i].freqHz);
            const float s = peaks[(size_t) i].deviation * w;
            scored[(size_t) scoredCount++] = { peaks[(size_t) i], s };
        }
        std::sort(scored.begin(), scored.begin() + scoredCount,
                  [](const Scored& a, const Scored& b) { return a.score > b.score; });

        // 5. Emit up to kMaxSuggestions.
        Suggestion next[kMaxSuggestions] {};
        const int n = std::min(kMaxSuggestions, scoredCount);
        for (int i = 0; i < n; ++i)
        {
            const auto& p = scored[(size_t) i].peak;
            Suggestion& sg = next[i];
            sg.freqHz     = p.freqHz;
            sg.gainDb     = -std::min(12.0f, std::max(3.0f, p.deviation - 1.5f));
            sg.q          = std::clamp(2.0f + 0.4f * p.sharpness, 2.0f, 8.0f);
            sg.confidence = std::clamp(scored[(size_t) i].score / 12.0f, 0.0f, 1.0f);
            sg.label      = labelFor(p.freqHz);
        }

        // Publish: copy in, update count with release ordering so UI reader
        // is guaranteed to see the new array before it sees the new count.
        for (int i = 0; i < kMaxSuggestions; ++i)
            published[(size_t) i] = (i < n) ? next[i] : Suggestion {};
        publishedCount.store(n, std::memory_order_release);
    }

    // UI thread: copy out the current snapshot. Returns the number of valid
    // suggestions (0..kMaxSuggestions).
    int getSuggestions(Suggestion* out) const noexcept
    {
        const int n = publishedCount.load(std::memory_order_acquire);
        for (int i = 0; i < n; ++i) out[i] = published[(size_t) i];
        return n;
    }

private:
    static const char* labelFor(float hz) noexcept
    {
        if (hz <  80.0f)  return "sub rumble";
        if (hz < 200.0f)  return "low thump";
        if (hz < 400.0f)  return "mud";
        if (hz < 800.0f)  return "boxiness";
        if (hz < 1600.0f) return "nasal";
        if (hz < 3200.0f) return "honk";
        if (hz < 6400.0f) return "harshness";
        if (hz < 10000.0f) return "sibilance";
        return "air ring";
    }

    float intentWeight(float hz) const noexcept
    {
        return intentWeightFor(intent, hz);
    }

    void buildLogGrid() noexcept
    {
        const double fMin = 20.0;
        const double fMax = std::max(21.0, sampleRate * 0.5);
        const double step = std::pow(fMax / fMin, 1.0 / (double) kLogBins);
        const int    linBins = 2048; // matches SpectrumFIFO::numBins at the default FFT order
        const double hzPerBin = sampleRate * 0.5 / (double) linBins;
        double f = fMin;
        for (int i = 0; i < kLogBins; ++i)
        {
            const double fHi = f * step;
            logBinCenterHz[(size_t) i] = (float) std::sqrt(f * fHi);
            logBinStart   [(size_t) i] = std::clamp((int) std::floor(f   / hzPerBin), 0, linBins - 1);
            logBinEnd     [(size_t) i] = std::clamp((int) std::ceil (fHi / hzPerBin),
                                                   logBinStart[(size_t) i] + 1, linBins);
            f = fHi;
        }
    }

    double sampleRate = 44100.0;
    IntentMode intent = IntentMode::None;

    std::array<int,   kLogBins> logBinStart {};
    std::array<int,   kLogBins> logBinEnd   {};
    std::array<float, kLogBins> logBinCenterHz {};
    std::array<float, kLogBins> logSpectrum {};
    std::array<float, kLogBins> baseline    {};

    std::array<Suggestion, kMaxSuggestions> published {};
    std::atomic<int>                         publishedCount { 0 };
};
