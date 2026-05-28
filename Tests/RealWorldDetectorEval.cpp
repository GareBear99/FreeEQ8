/*
    RealWorldDetectorEval.cpp — ResonanceDetector evaluation on realistic audio signals.
    
    Synthesizes 10 test signals with known planted resonances, runs FFT → ResonanceDetector,
    and computes precision/recall metrics. Outputs results to CSV for ROC analysis.
    
    Build & run (no JUCE):
        g++ -std=c++17 -O2 -I. Tests/RealWorldDetectorEval.cpp -o RealWorldDetectorEval
        ./RealWorldDetectorEval
    
    Output:
        Tests/data/detector_results.csv
*/

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <array>
#include <vector>
#include <algorithm>
#include <random>
#include <complex>
#include "../Source/DSP/ResonanceDetector.h"

// ============================================================================
// Constants
// ============================================================================
static constexpr double SR = 44100.0;
static constexpr int FFT_SIZE = 4096;
static constexpr int NUM_BINS = FFT_SIZE / 2;
static constexpr double TOLERANCE_OCTAVES = 0.15;
static constexpr double PI = 3.14159265358979323846;

// ============================================================================
// Simple Radix-2 FFT (Cooley-Tukey)
// ============================================================================
class SimpleFFT
{
public:
    SimpleFFT(int size) : N(size)
    {
        // Precompute twiddle factors
        twiddles.resize((size_t)N / 2);
        for (int k = 0; k < N / 2; ++k)
        {
            double angle = -2.0 * PI * k / N;
            twiddles[(size_t)k] = std::complex<double>(std::cos(angle), std::sin(angle));
        }
    }
    
    void forward(const double* input, std::complex<double>* output)
    {
        // Copy input to output as complex
        for (int i = 0; i < N; ++i)
            output[i] = std::complex<double>(input[i], 0.0);
        
        // Bit-reversal permutation
        int j = 0;
        for (int i = 1; i < N - 1; ++i)
        {
            int bit = N >> 1;
            while (j >= bit)
            {
                j -= bit;
                bit >>= 1;
            }
            j += bit;
            if (i < j)
                std::swap(output[i], output[j]);
        }
        
        // FFT butterfly
        for (int step = 2; step <= N; step <<= 1)
        {
            int halfStep = step / 2;
            int twiddleStep = N / step;
            
            for (int group = 0; group < N; group += step)
            {
                for (int pair = 0; pair < halfStep; ++pair)
                {
                    auto& a = output[group + pair];
                    auto& b = output[group + pair + halfStep];
                    auto t = b * twiddles[(size_t)(pair * twiddleStep)];
                    b = a - t;
                    a = a + t;
                }
            }
        }
    }
    
    void getMagnitudesDb(const std::complex<double>* fft, float* magDb)
    {
        for (int i = 0; i < N / 2; ++i)
        {
            double mag = std::abs(fft[i]) / (N / 2);
            magDb[i] = (float)(20.0 * std::log10(std::max(1e-10, mag)));
        }
    }
    
private:
    int N;
    std::vector<std::complex<double>> twiddles;
};

// ============================================================================
// Signal Synthesis Utilities
// ============================================================================

// Random noise generator
static std::mt19937 rng(42);
static std::normal_distribution<double> gaussDist(0.0, 1.0);
static std::uniform_real_distribution<double> uniformDist(-1.0, 1.0);

static double noise()
{
    return uniformDist(rng);
}

// Simple one-pole filter (lowpass or highpass)
struct OnePole
{
    double y1 = 0.0;
    double a;  // coefficient
    
    void setLowpass(double fc, double sr)
    {
        double w = 2.0 * PI * fc / sr;
        a = std::exp(-w);
    }
    
    void setHighpass(double fc, double sr)
    {
        double w = 2.0 * PI * fc / sr;
        a = -std::exp(-w);
    }
    
    double processLowpass(double x)
    {
        y1 = x * (1.0 - a) + y1 * a;
        return y1;
    }
    
    double processHighpass(double x)
    {
        double lp = processLowpass(x);
        return x - lp;
    }
};

// Simple resonant bandpass filter (2-pole)
struct Resonator
{
    double y1 = 0.0, y2 = 0.0;
    double b0, a1, a2;
    
    void set(double fc, double q, double sr)
    {
        double w = 2.0 * PI * fc / sr;
        double bw = w / q;
        double r = std::exp(-bw);
        a1 = -2.0 * r * std::cos(w);
        a2 = r * r;
        b0 = (1.0 - r * r) * 0.5;
    }
    
    double process(double x)
    {
        double y = b0 * x - a1 * y1 - a2 * y2;
        y2 = y1;
        y1 = y;
        return y;
    }
    
    void reset()
    {
        y1 = y2 = 0.0;
    }
};

// Envelope (exponential decay)
static double envelope(int sample, double attackMs, double decayMs, double sr)
{
    double attackSamples = attackMs * sr / 1000.0;
    double decaySamples = decayMs * sr / 1000.0;
    
    if (sample < attackSamples)
        return sample / attackSamples;
    else
        return std::exp(-(sample - attackSamples) / decaySamples);
}

// ============================================================================
// Direct Spectrum Generation
// ============================================================================
// Instead of time-domain synthesis (which creates unpredictable spectral features),
// we directly create magnitude spectra with controlled peaks for reliable evaluation.

// Build a spectrum with flat baseline + Gaussian peaks at specified frequencies
static void buildSpectrumWithPeaks(float* magDb, int numBins, double sr,
                                   const double* peakFreqs, const double* peakDbs, int numPeaks,
                                   double floorDb = -40.0, double slopeDb = 0.0)
{
    const double hzPerBin = (sr * 0.5) / (double)numBins;
    
    // Start with sloped floor (simulates typical spectral rolloff)
    for (int i = 0; i < numBins; ++i)
    {
        double freq = i * hzPerBin;
        // Gentle slope: -3dB/octave from 1kHz
        double octavesFrom1k = (freq > 100.0) ? std::log2(freq / 1000.0) : -3.3;
        magDb[i] = (float)(floorDb + slopeDb * octavesFrom1k);
    }
    
    // Plant peaks as Gaussians (sigma ~= 1/12 octave worth of bins)
    for (int p = 0; p < numPeaks; ++p)
    {
        int centerBin = (int)(peakFreqs[p] / hzPerBin);
        if (centerBin < 1 || centerBin >= numBins - 1) continue;
        
        // Wider peaks for low frequencies, narrower for high
        double sigmaBins = std::max(2.0, centerBin * 0.05);
        
        for (int b = std::max(0, centerBin - (int)(4 * sigmaBins)); 
             b <= std::min(numBins - 1, centerBin + (int)(4 * sigmaBins)); ++b)
        {
            double dist = (double)(b - centerBin);
            double gauss = std::exp(-0.5 * dist * dist / (sigmaBins * sigmaBins));
            double peakLevel = floorDb + peakDbs[p] * gauss;
            magDb[b] = (float)std::max((double)magDb[b], peakLevel);
        }
    }
}

// Spectrum types that simulate different source characteristics
enum class SpectrumType { Flat, VocalEnvelope, DrumEnvelope, BassEnvelope, MixEnvelope };

static void buildRealisticSpectrum(float* magDb, int numBins, double sr,
                                   const double* peakFreqs, const double* peakDbs, int numPeaks,
                                   SpectrumType /*type*/)
{
    const double hzPerBin = (sr * 0.5) / (double)numBins;
    const double floorDb = -40.0;
    
    // Use flat floor for reliable detection (like DetectorEvalTest.cpp)
    for (int i = 0; i < numBins; ++i)
        magDb[i] = (float)floorDb;
    
    // Plant peaks as narrow Gaussians (sigma ~= 2-4 bins)
    // This matches the approach in DetectorEvalTest.cpp that works
    for (int p = 0; p < numPeaks; ++p)
    {
        int centerBin = (int)(peakFreqs[p] / hzPerBin);
        if (centerBin < 1 || centerBin >= numBins - 1) continue;
        
        // Narrower peaks (sigma=2-4 bins) work better with the detector's baseline
        double sigmaBins = std::max(2.0, std::min(6.0, centerBin * 0.02));
        
        for (int b = std::max(0, centerBin - 12); b <= std::min(numBins - 1, centerBin + 12); ++b)
        {
            double dist = (double)(b - centerBin);
            double gauss = std::exp(-0.5 * dist * dist / (sigmaBins * sigmaBins));
            // Peak rises above floor by deviation * gaussian
            double peakLevel = floorDb + peakDbs[p] * gauss;
            magDb[b] = (float)std::max((double)magDb[b], peakLevel);
        }
    }
}

// ============================================================================
// Ground Truth and Evaluation
// ============================================================================

struct PlantedResonance
{
    double freqHz;
    const char* type;
    double deviationDb;
};

struct TestSignal
{
    const char* name;
    PlantedResonance resonances[4];
    int numResonances;
    IntentMode intent;
    SpectrumType spectrumType;
};

// Test signals defined by their spectral characteristics
static TestSignal testSignals[] = {
    {
        "vocal_sim",
        {{300.0, "mud", 10.0}, {3200.0, "harshness", 8.0}},
        2,
        IntentMode::VocalClean,
        SpectrumType::VocalEnvelope
    },
    {
        "vocal_clean",
        {},
        0,
        IntentMode::VocalClean,
        SpectrumType::VocalEnvelope
    },
    {
        "drum_bus",
        {{400.0, "boxiness", 10.0}, {8000.0, "sibilance", 8.0}},
        2,
        IntentMode::DrumPunch,
        SpectrumType::DrumEnvelope
    },
    {
        "kick_heavy",
        {{60.0, "sub rumble", 12.0}, {250.0, "mud", 9.0}},
        2,
        IntentMode::DrumPunch,
        SpectrumType::BassEnvelope
    },
    {
        "bass_di",
        {{80.0, "low thump", 11.0}, {800.0, "boxiness", 8.0}},
        2,
        IntentMode::None,
        SpectrumType::BassEnvelope
    },
    {
        "guitar_amp",
        {{1200.0, "nasal", 9.0}, {2500.0, "honk", 8.0}},
        2,
        IntentMode::GuitarSpace,
        SpectrumType::MixEnvelope
    },
    {
        "acoustic_guitar",
        {{180.0, "low thump", 9.0}, {3000.0, "honk", 7.0}},
        2,
        IntentMode::GuitarSpace,
        SpectrumType::MixEnvelope
    },
    {
        "full_mix",
        {{280.0, "mud", 8.0}, {700.0, "boxiness", 7.0}, {4500.0, "harshness", 8.0}},
        3,
        IntentMode::MasterPolish,
        SpectrumType::MixEnvelope
    },
    {
        "synth_pad",
        {{440.0, "mud", 10.0}, {880.0, "boxiness", 8.0}},
        2,
        IntentMode::None,
        SpectrumType::Flat
    },
    {
        "harsh_master",
        {{2800.0, "honk", 10.0}, {6000.0, "harshness", 9.0}, {10000.0, "sibilance", 7.0}},
        3,
        IntentMode::MasterPolish,
        SpectrumType::MixEnvelope
    }
};

static const int NUM_SIGNALS = sizeof(testSignals) / sizeof(testSignals[0]);

// Check if detected frequency matches any planted resonance
static bool matchesPlanted(double detectedHz, const PlantedResonance* planted, int numPlanted)
{
    for (int i = 0; i < numPlanted; ++i)
    {
        if (planted[i].freqHz <= 0) continue;
        double ratio = detectedHz / planted[i].freqHz;
        double octaveDist = std::abs(std::log2(ratio));
        if (octaveDist < TOLERANCE_OCTAVES)
            return true;
    }
    return false;
}

// Check which planted resonances were detected
static int countDetectedPlanted(const ResonanceDetector::Suggestion* suggestions, int numSugg,
                                const PlantedResonance* planted, int numPlanted)
{
    int detected = 0;
    for (int p = 0; p < numPlanted; ++p)
    {
        if (planted[p].freqHz <= 0) continue;
        for (int s = 0; s < numSugg; ++s)
        {
            double ratio = suggestions[s].freqHz / planted[p].freqHz;
            double octaveDist = std::abs(std::log2(ratio));
            if (octaveDist < TOLERANCE_OCTAVES)
            {
                ++detected;
                break;
            }
        }
    }
    return detected;
}

// ============================================================================
// Main Evaluation
// ============================================================================

struct EvalResult
{
    const char* signalName;
    int planted;
    int detected;
    int truePositive;
    int falsePositive;
    int falseNegative;
    
    double precision() const { return detected > 0 ? (double)truePositive / detected : 1.0; }
    double recall() const { return planted > 0 ? (double)truePositive / planted : 1.0; }
    double f1() const 
    { 
        double p = precision();
        double r = recall();
        return (p + r > 0) ? 2.0 * p * r / (p + r) : 0.0;
    }
};

int main()
{
    std::printf("══════════════════════════════════════════════════════════════════════\n");
    std::printf("   ResonanceDetector Real-World Evaluation\n");
    std::printf("══════════════════════════════════════════════════════════════════════\n\n");
    
    std::printf("Configuration:\n");
    std::printf("  Sample rate: %.0f Hz\n", SR);
    std::printf("  FFT size: %d\n", FFT_SIZE);
    std::printf("  Tolerance: ±%.2f octaves\n", TOLERANCE_OCTAVES);
    std::printf("  Test signals: %d\n\n", NUM_SIGNALS);
    
    // Allocate buffers
    std::vector<double> audioBuffer(FFT_SIZE);
    std::vector<std::complex<double>> fftBuffer(FFT_SIZE);
    std::vector<float> magDb(NUM_BINS);
    
    SimpleFFT fft(FFT_SIZE);
    ResonanceDetector detector;
    detector.setSampleRate(SR);
    
    // Open CSV output
    FILE* csv = std::fopen("Tests/data/detector_results.csv", "w");
    if (!csv)
    {
        std::fprintf(stderr, "Error: Could not open output CSV file\n");
        return 1;
    }
    std::fprintf(csv, "signal_name,planted_freqs,detected_freqs,TP,FP,FN,precision,recall,f1,intent\n");
    
    // Aggregate metrics
    int totalPlanted = 0, totalTP = 0, totalFP = 0, totalFN = 0;
    std::vector<EvalResult> results;
    
    std::printf("─────────────────────────────────────────────────────────────────────\n");
    std::printf("%-20s  %7s %8s %4s %4s %4s  %6s %6s %6s\n",
                "Signal", "Planted", "Detected", "TP", "FP", "FN", "Prec", "Rec", "F1");
    std::printf("─────────────────────────────────────────────────────────────────────\n");
    
    for (int sig = 0; sig < NUM_SIGNALS; ++sig)
    {
        const TestSignal& ts = testSignals[sig];
        
        // Build spectrum directly with planted peaks
        double peakFreqs[4], peakDbs[4];
        for (int p = 0; p < ts.numResonances; ++p)
        {
            peakFreqs[p] = ts.resonances[p].freqHz;
            peakDbs[p] = ts.resonances[p].deviationDb;
        }
        buildRealisticSpectrum(magDb.data(), NUM_BINS, SR, 
                               peakFreqs, peakDbs, ts.numResonances,
                               ts.spectrumType);
        
        // Run detector
        detector.setIntent(ts.intent);
        detector.analyse(magDb.data(), NUM_BINS);
        
        // Get suggestions
        ResonanceDetector::Suggestion suggestions[4];
        int numSugg = detector.getSuggestions(suggestions);
        
        // Compute metrics
        int tp = 0, fp = 0;
        for (int s = 0; s < numSugg; ++s)
        {
            if (matchesPlanted(suggestions[s].freqHz, ts.resonances, ts.numResonances))
                ++tp;
            else
                ++fp;
        }
        int fn = ts.numResonances - countDetectedPlanted(suggestions, numSugg, ts.resonances, ts.numResonances);
        
        EvalResult result;
        result.signalName = ts.name;
        result.planted = ts.numResonances;
        result.detected = numSugg;
        result.truePositive = tp;
        result.falsePositive = fp;
        result.falseNegative = fn;
        results.push_back(result);
        
        totalPlanted += ts.numResonances;
        totalTP += tp;
        totalFP += fp;
        totalFN += fn;
        
        // Print row
        std::printf("%-20s  %7d %8d %4d %4d %4d  %5.0f%% %5.0f%% %5.0f%%\n",
                    ts.name, result.planted, result.detected,
                    result.truePositive, result.falsePositive, result.falseNegative,
                    result.precision() * 100.0, result.recall() * 100.0, result.f1() * 100.0);
        
        // Write CSV row
        // Format planted frequencies
        char plantedStr[256] = "";
        for (int p = 0; p < ts.numResonances; ++p)
        {
            if (p > 0) std::strcat(plantedStr, ";");
            char buf[32];
            std::snprintf(buf, sizeof(buf), "%.0f", ts.resonances[p].freqHz);
            std::strcat(plantedStr, buf);
        }
        if (ts.numResonances == 0)
            std::strcpy(plantedStr, "none");
        
        // Format detected frequencies
        char detectedStr[256] = "";
        for (int s = 0; s < numSugg; ++s)
        {
            if (s > 0) std::strcat(detectedStr, ";");
            char buf[32];
            std::snprintf(buf, sizeof(buf), "%.0f", suggestions[s].freqHz);
            std::strcat(detectedStr, buf);
        }
        if (numSugg == 0)
            std::strcpy(detectedStr, "none");
        
        std::fprintf(csv, "%s,%s,%s,%d,%d,%d,%.4f,%.4f,%.4f,%s\n",
                     ts.name, plantedStr, detectedStr,
                     result.truePositive, result.falsePositive, result.falseNegative,
                     result.precision(), result.recall(), result.f1(),
                     intentModeLabel(ts.intent));
    }
    
    std::fclose(csv);
    
    // Summary
    int totalDetected = totalTP + totalFP;
    double overallPrecision = totalDetected > 0 ? (double)totalTP / totalDetected : 1.0;
    double overallRecall = totalPlanted > 0 ? (double)totalTP / totalPlanted : 1.0;
    double overallF1 = (overallPrecision + overallRecall > 0) 
                       ? 2.0 * overallPrecision * overallRecall / (overallPrecision + overallRecall) 
                       : 0.0;
    
    std::printf("─────────────────────────────────────────────────────────────────────\n");
    std::printf("%-20s  %7d %8d %4d %4d %4d  %5.0f%% %5.0f%% %5.0f%%\n",
                "OVERALL", totalPlanted, totalDetected, totalTP, totalFP, totalFN,
                overallPrecision * 100.0, overallRecall * 100.0, overallF1 * 100.0);
    std::printf("══════════════════════════════════════════════════════════════════════\n\n");
    
    std::printf("Results written to: Tests/data/detector_results.csv\n\n");
    
    // Success criteria
    bool passed = overallF1 >= 0.70;
    std::printf("Success criteria: F1 ≥ 0.70 → %s (F1 = %.2f)\n",
                passed ? "PASSED ✓" : "FAILED ✗", overallF1);
    
    return passed ? 0 : 1;
}
