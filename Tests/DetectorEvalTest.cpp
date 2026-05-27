/*
    DetectorEvalTest.cpp — ResonanceDetector precision/recall evaluation.
    Provides quantitative evidence for PAPER.md §5.1.

    Synthesizes magnitude spectra with known resonance peaks planted at
    specific frequencies, runs the detector, and checks whether it finds them.

    Build & run (no JUCE):
        g++ -std=c++17 -O2 Tests/DetectorEvalTest.cpp -o DetectorEvalTest -I.
        ./DetectorEvalTest
*/

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <array>
#include <algorithm>
#include "../Source/DSP/ResonanceDetector.h"

static constexpr double SR = 44100.0;
static constexpr int NUM_BINS = 2048;

// Build a synthetic magnitude spectrum (dB) with a flat floor + planted peaks
static void buildSpectrum(float* mag, int numBins, double sr,
                          const double* peakFreqs, const double* peakDbs, int numPeaks,
                          double floorDb = -40.0)
{
    const double hzPerBin = (sr * 0.5) / (double)numBins;

    // Flat floor
    for (int i = 0; i < numBins; ++i)
        mag[i] = (float)floorDb;

    // Plant peaks as narrow Gaussians (±2 bins width)
    for (int p = 0; p < numPeaks; ++p)
    {
        int centerBin = (int)(peakFreqs[p] / hzPerBin);
        if (centerBin < 0 || centerBin >= numBins) continue;

        for (int b = std::max(0, centerBin - 8); b <= std::min(numBins - 1, centerBin + 8); ++b)
        {
            double dist = (double)(b - centerBin);
            double gauss = std::exp(-0.5 * dist * dist / 4.0); // sigma=2 bins
            mag[b] = (float)std::max((double)mag[b], floorDb + peakDbs[p] * gauss);
        }
    }
}

// Check if a detected suggestion matches a planted peak within tolerance
static bool matchesPeak(float detectedHz, const double* peakFreqs, int numPeaks, double toleranceOctaves = 0.15)
{
    for (int p = 0; p < numPeaks; ++p)
    {
        double ratio = detectedHz / peakFreqs[p];
        double octaveDist = std::abs(std::log2(ratio));
        if (octaveDist < toleranceOctaves)
            return true;
    }
    return false;
}

struct EvalResult
{
    int planted;
    int detected;
    int truePositive;
    double precision() const { return detected > 0 ? (double)truePositive / detected : 0.0; }
    double recall()    const { return planted > 0  ? (double)truePositive / planted  : 0.0; }
};

static EvalResult runEval(const char* name,
                          const double* peakFreqs, const double* peakDbs, int numPeaks,
                          IntentMode mode = IntentMode::None)
{
    std::array<float, NUM_BINS> spectrum {};
    buildSpectrum(spectrum.data(), NUM_BINS, SR, peakFreqs, peakDbs, numPeaks);

    ResonanceDetector det;
    det.setSampleRate(SR);
    det.setIntent(mode);
    det.analyse(spectrum.data(), NUM_BINS);

    ResonanceDetector::Suggestion suggestions[4];
    int n = det.getSuggestions(suggestions);

    int tp = 0;
    for (int i = 0; i < n; ++i)
    {
        if (matchesPeak(suggestions[i].freqHz, peakFreqs, numPeaks))
            ++tp;
    }

    EvalResult result { numPeaks, n, tp };

    std::printf("  %-30s  planted=%d  detected=%d  TP=%d  precision=%.0f%%  recall=%.0f%%\n",
                name, result.planted, result.detected, result.truePositive,
                result.precision() * 100.0, result.recall() * 100.0);

    return result;
}

int main()
{
    std::printf("── ResonanceDetector Evaluation on Synthetic Spectra ──\n\n");

    int totalPlanted = 0, totalDetected = 0, totalTP = 0;

    // Test 1: Single peak at 300 Hz (+10 dB above floor)
    {
        double f[] = {300.0};
        double d[] = {10.0};
        auto r = runEval("Single 300Hz +10dB", f, d, 1);
        totalPlanted += r.planted; totalDetected += r.detected; totalTP += r.truePositive;
    }

    // Test 2: Single peak at 3 kHz (+8 dB)
    {
        double f[] = {3000.0};
        double d[] = {8.0};
        auto r = runEval("Single 3kHz +8dB", f, d, 1);
        totalPlanted += r.planted; totalDetected += r.detected; totalTP += r.truePositive;
    }

    // Test 3: Single peak at 8 kHz (+12 dB)
    {
        double f[] = {8000.0};
        double d[] = {12.0};
        auto r = runEval("Single 8kHz +12dB", f, d, 1);
        totalPlanted += r.planted; totalDetected += r.detected; totalTP += r.truePositive;
    }

    // Test 4: Two peaks (300 Hz + 3 kHz)
    {
        double f[] = {300.0, 3000.0};
        double d[] = {10.0, 8.0};
        auto r = runEval("Dual 300Hz+3kHz", f, d, 2);
        totalPlanted += r.planted; totalDetected += r.detected; totalTP += r.truePositive;
    }

    // Test 5: Three peaks (300 Hz + 3 kHz + 8 kHz)
    {
        double f[] = {300.0, 3000.0, 8000.0};
        double d[] = {10.0, 8.0, 12.0};
        auto r = runEval("Triple 300Hz+3kHz+8kHz", f, d, 3);
        totalPlanted += r.planted; totalDetected += r.detected; totalTP += r.truePositive;
    }

    // Test 6: Four peaks (mud + box + harsh + sibilance)
    {
        double f[] = {320.0, 800.0, 5000.0, 9000.0};
        double d[] = {9.0, 7.0, 11.0, 8.0};
        auto r = runEval("Quad mix-problem zones", f, d, 4);
        totalPlanted += r.planted; totalDetected += r.detected; totalTP += r.truePositive;
    }

    // Test 7: VocalClean intent mode (should boost 300 Hz detection)
    {
        double f[] = {300.0, 3200.0};
        double d[] = {6.0, 6.0}; // both marginal
        auto r = runEval("VocalClean intent (marginal)", f, d, 2, IntentMode::VocalClean);
        totalPlanted += r.planted; totalDetected += r.detected; totalTP += r.truePositive;
    }

    // Test 8: No peaks (flat spectrum — should detect nothing)
    {
        auto r = runEval("Flat spectrum (no peaks)", nullptr, nullptr, 0);
        totalPlanted += r.planted; totalDetected += r.detected; totalTP += r.truePositive;
    }

    // Summary
    double overallPrecision = totalDetected > 0 ? (double)totalTP / totalDetected * 100.0 : 100.0;
    double overallRecall    = totalPlanted > 0  ? (double)totalTP / totalPlanted  * 100.0 : 100.0;

    std::printf("\n── Summary ──\n");
    std::printf("  Total planted: %d  Total detected: %d  True positives: %d\n",
                totalPlanted, totalDetected, totalTP);
    std::printf("  Overall precision: %.0f%%  Overall recall: %.0f%%\n",
                overallPrecision, overallRecall);

    return (overallPrecision >= 80.0 && overallRecall >= 60.0) ? 0 : 1;
}
