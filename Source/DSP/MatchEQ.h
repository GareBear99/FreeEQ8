#pragma once
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <atomic>
#include <cmath>
#include <algorithm>

// Match EQ: captures a reference spectrum and computes a correction curve.
// Flow:
//   1. startCapture() → pushSamples() accumulates reference spectrum → stopCapture()
//   2. setMatchActive(true) → enters analysis phase, accumulates current spectrum
//   3. After enough analysis frames, correction = reference - current is computed
//   4. applyCorrection() applies per-bin gain via FFT with overlap-add
class MatchEQ
{
public:
    static constexpr int fftOrder = 12;              // 4096-point FFT
    static constexpr int fftSize  = 1 << fftOrder;   // 4096
    static constexpr int numBins  = fftSize / 2;     // 2048
    static constexpr int analysisFramesNeeded = 8;

    MatchEQ()
        : fft(fftOrder),
          window(fftSize, juce::dsp::WindowingFunction<float>::hann)
    {
        clear();
    }

    // Start capturing reference spectrum.
    // NOTE: Must only be called from the GUI thread. Audio thread checks
    // `capturing` atomically before touching shared buffers.
    void startCapture()
    {
        // Disable capture/analysis first so audio thread stops touching buffers
        capturing.store(false, std::memory_order_release);
        analyzing.store(false, std::memory_order_release);

        // Safe to reset now — audio thread will skip pushSamples
        captureFrames.store(0, std::memory_order_relaxed);
        fifoIndex.store(0, std::memory_order_relaxed);
        std::fill(capturedSpectrum.begin(), capturedSpectrum.end(), 0.0f);

        capturing.store(true, std::memory_order_release);
    }

    // Stop capturing and finalize the reference spectrum.
    void stopCapture()
    {
        capturing.store(false, std::memory_order_release);
        const int frames = captureFrames.load(std::memory_order_acquire);
        if (frames > 0)
        {
            const float inv = 1.0f / (float)frames;
            for (int i = 0; i < numBins; ++i)
                capturedSpectrum[(size_t)i] *= inv;
            hasCapturedData.store(true, std::memory_order_release);
        }
    }

    // Clear all captured data and correction state.
    void clear()
    {
        // Disable all modes first so audio thread stops touching buffers
        capturing.store(false, std::memory_order_release);
        matchActive.store(false, std::memory_order_release);
        analyzing.store(false, std::memory_order_release);
        hasCapturedData.store(false, std::memory_order_release);
        captureFrames.store(0, std::memory_order_relaxed);
        analyzeFrames.store(0, std::memory_order_relaxed);
        fifoIndex.store(0, std::memory_order_relaxed);
        std::fill(capturedSpectrum.begin(), capturedSpectrum.end(), 0.0f);
        std::fill(currentSpectrum.begin(), currentSpectrum.end(), 0.0f);
        std::fill(correctionDb.begin(), correctionDb.end(), 0.0f);
        std::fill(overlapL.begin(), overlapL.end(), 0.0f);
        std::fill(overlapR.begin(), overlapR.end(), 0.0f);
    }

    // Push audio samples for capture or analysis (call from audio thread).
    void pushSamples(const float* L, const float* R, int numSamples)
    {
        const bool cap = capturing.load(std::memory_order_acquire);
        const bool ana = analyzing.load(std::memory_order_acquire);
        if (!cap && !ana) return;

        int idx = fifoIndex.load(std::memory_order_relaxed);
        for (int i = 0; i < numSamples; ++i)
        {
            fifoBuffer[(size_t)idx] = (L[i] + R[i]) * 0.5f;
            idx++;

            if (idx >= fftSize)
            {
                idx = 0;
                if (cap)
                    processReferenceFrame();
                else if (ana)
                    processAnalysisFrame();
            }
        }
        fifoIndex.store(idx, std::memory_order_relaxed);
    }

    // Activate or deactivate match correction.
    // When activated: begins an analysis phase to measure the current signal,
    // then automatically computes correction and starts applying it.
    void setMatchActive(bool active)
    {
        if (active)
        {
            // If already analyzing, cancel
            if (analyzing.load(std::memory_order_acquire))
            {
                analyzing.store(false, std::memory_order_release);
                return;
            }
            // No-op if already matching
            if (matchActive.load(std::memory_order_acquire)) return;
            // Need captured reference data
            if (!hasCapturedData.load(std::memory_order_acquire)) return;

            // Disable analysis first, reset state, then re-enable
            analyzing.store(false, std::memory_order_release);
            analyzeFrames.store(0, std::memory_order_relaxed);
            std::fill(currentSpectrum.begin(), currentSpectrum.end(), 0.0f);
            fifoIndex.store(0, std::memory_order_relaxed);
            analyzing.store(true, std::memory_order_release);
        }
        else
        {
            matchActive.store(false, std::memory_order_release);
            analyzing.store(false, std::memory_order_release);
        }
    }

    bool isMatchActive() const { return matchActive.load(std::memory_order_acquire); }
    bool hasCapture() const { return hasCapturedData.load(std::memory_order_acquire); }
    bool isCapturing() const { return capturing.load(std::memory_order_acquire); }
    bool isAnalyzing() const { return analyzing.load(std::memory_order_acquire); }

    const float* getCorrectionDb() const { return correctionDb.data(); }
    const float* getCapturedSpectrum() const { return capturedSpectrum.data(); }
    int getNumBins() const { return numBins; }

    // Apply match EQ correction via FFT with overlap-add (call from audio thread).
    void applyCorrection(float* L, float* R, int numSamples, double /*sampleRate*/)
    {
        if (!matchActive.load(std::memory_order_acquire)) return;
        if (numSamples > fftSize) return;

        applyToChannel(L, numSamples, overlapL);
        applyToChannel(R, numSamples, overlapR);
    }

private:
    juce::dsp::FFT fft;
    juce::dsp::WindowingFunction<float> window;

    std::atomic<bool> capturing { false };
    std::atomic<bool> hasCapturedData { false };
    std::atomic<bool> matchActive { false };
    std::atomic<bool> analyzing { false };

    std::atomic<int> captureFrames { 0 };
    std::atomic<int> analyzeFrames { 0 };

    std::array<float, fftSize> fifoBuffer {};
    std::atomic<int> fifoIndex { 0 };

    // Captured reference spectrum in dB (averaged over capture frames)
    std::array<float, numBins> capturedSpectrum {};

    // Current signal spectrum in dB (accumulated during analysis phase)
    std::array<float, numBins> currentSpectrum {};

    // Correction curve in dB (reference - current, clamped ±24 dB)
    std::array<float, numBins> correctionDb {};

    // Overlap-add buffers for artifact-free correction
    std::array<float, fftSize> overlapL {};
    std::array<float, fftSize> overlapR {};

    // Scratch buffer for FFT processing (avoids large stack allocations on the audio thread)
    // JUCE real-only FFT requires 2*fftSize floats
    std::array<float, fftSize * 2> processBuf {};

    // Pre-allocated scratch for processReferenceFrame / processAnalysisFrame
    // (avoids 32 KB stack allocations on the audio thread)
    std::array<float, fftSize * 2> frameFftBuf {};

    void processReferenceFrame()
    {
        std::fill(frameFftBuf.begin(), frameFftBuf.end(), 0.0f);
        std::copy(fifoBuffer.begin(), fifoBuffer.end(), frameFftBuf.begin());

        window.multiplyWithWindowingTable(frameFftBuf.data(), (size_t)fftSize);
        fft.performFrequencyOnlyForwardTransform(frameFftBuf.data());

        for (int i = 0; i < numBins; ++i)
        {
            float mag = frameFftBuf[(size_t)i] / (float)fftSize;
            float db = 20.0f * std::log10(std::max(mag, 1e-7f));
            capturedSpectrum[(size_t)i] += db;
        }

        captureFrames.fetch_add(1, std::memory_order_relaxed);
    }

    void processAnalysisFrame()
    {
        std::fill(frameFftBuf.begin(), frameFftBuf.end(), 0.0f);
        std::copy(fifoBuffer.begin(), fifoBuffer.end(), frameFftBuf.begin());

        window.multiplyWithWindowingTable(frameFftBuf.data(), (size_t)fftSize);
        fft.performFrequencyOnlyForwardTransform(frameFftBuf.data());

        for (int i = 0; i < numBins; ++i)
        {
            float mag = frameFftBuf[(size_t)i] / (float)fftSize;
            float db = 20.0f * std::log10(std::max(mag, 1e-7f));
            currentSpectrum[(size_t)i] += db;
        }

        const int frames = analyzeFrames.fetch_add(1, std::memory_order_relaxed) + 1;
        if (frames >= analysisFramesNeeded)
        {
            // Average the current spectrum
            const float inv = 1.0f / (float)frames;
            for (int i = 0; i < numBins; ++i)
                currentSpectrum[(size_t)i] *= inv;

            // Compute correction = reference - current (clamped to ±24 dB)
            for (int i = 0; i < numBins; ++i)
                correctionDb[(size_t)i] = std::clamp(
                    capturedSpectrum[(size_t)i] - currentSpectrum[(size_t)i],
                    -24.0f, 24.0f);

            // Reset overlap buffers for a clean start
            std::fill(overlapL.begin(), overlapL.end(), 0.0f);
            std::fill(overlapR.begin(), overlapR.end(), 0.0f);

            analyzing.store(false, std::memory_order_release);
            matchActive.store(true, std::memory_order_release);
        }
    }

    void applyToChannel(float* data, int n, std::array<float, fftSize>& overlap)
    {
        // Zero-pad input into scratch buffer
        std::fill(processBuf.begin(), processBuf.end(), 0.0f);
        for (int i = 0; i < n; ++i)
            processBuf[(size_t)i] = data[i];

        // Forward FFT
        fft.performRealOnlyForwardTransform(processBuf.data());

        // Apply correction gains in frequency domain
        // DC (JUCE packs DC in index 0)
        processBuf[0] *= std::pow(10.0f, correctionDb[0] / 20.0f);
        // Nyquist (JUCE packs Nyquist in index 1)
        processBuf[1] *= std::pow(10.0f, correctionDb[numBins - 1] / 20.0f);
        // Complex bins 1..N/2-1
        for (int i = 1; i < fftSize / 2; ++i)
        {
            const int binIdx = std::min(i, numBins - 1);
            const float gain = std::pow(10.0f, correctionDb[(size_t)binIdx] / 20.0f);
            processBuf[(size_t)(i * 2)]     *= gain;
            processBuf[(size_t)(i * 2 + 1)] *= gain;
        }

        // Inverse FFT
        fft.performRealOnlyInverseTransform(processBuf.data());

        // Overlap-add: combine with previous tail, output current block
        for (int i = 0; i < n; ++i)
        {
            data[i] = processBuf[(size_t)i] + overlap[(size_t)i];
            overlap[(size_t)i] = 0.0f;
        }

        // Save the convolution tail as overlap for the next block
        for (int i = n; i < fftSize; ++i)
            overlap[(size_t)(i - n)] += processBuf[(size_t)i];
    }
};
