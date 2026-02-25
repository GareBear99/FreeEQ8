#pragma once
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <atomic>
#include <cmath>
#include <algorithm>

// Match EQ: captures a reference spectrum and computes a correction curve.
// The correction is applied as per-bin gain in the frequency domain.
class MatchEQ
{
public:
    static constexpr int fftOrder = 12;             // 4096-point FFT
    static constexpr int fftSize  = 1 << fftOrder;  // 4096
    static constexpr int numBins  = fftSize / 2;    // 2048

    MatchEQ()
        : fft(fftOrder),
          window(fftSize, juce::dsp::WindowingFunction<float>::hann)
    {
        clear();
    }

    // Start capturing reference spectrum.
    void startCapture()
    {
        capturing = true;
        captureFrames = 0;
        std::fill(capturedSpectrum.begin(), capturedSpectrum.end(), 0.0f);
    }

    // Stop capturing and finalize the reference spectrum.
    void stopCapture()
    {
        capturing = false;
        if (captureFrames > 0)
        {
            const float inv = 1.0f / (float)captureFrames;
            for (int i = 0; i < numBins; ++i)
                capturedSpectrum[(size_t)i] *= inv;
            hasCapturedData = true;
        }
    }

    // Clear captured data.
    void clear()
    {
        capturing = false;
        hasCapturedData = false;
        matchActive = false;
        captureFrames = 0;
        std::fill(capturedSpectrum.begin(), capturedSpectrum.end(), 0.0f);
        std::fill(correctionDb.begin(), correctionDb.end(), 0.0f);
        fifoIndex = 0;
    }

    // Push audio samples for capture (call from audio thread).
    void pushSamples(const float* L, const float* R, int numSamples)
    {
        if (!capturing) return;

        for (int i = 0; i < numSamples; ++i)
        {
            fifoBuffer[(size_t)fifoIndex] = (L[i] + R[i]) * 0.5f;
            fifoIndex++;

            if (fifoIndex >= fftSize)
            {
                fifoIndex = 0;
                processFrame();
            }
        }
    }

    // Set match active (applies correction).
    void setMatchActive(bool active) { matchActive = active; }
    bool isMatchActive() const { return matchActive; }
    bool hasCapture() const { return hasCapturedData; }
    bool isCapturing() const { return capturing; }

    // Compute correction: call when match is activated.
    // currentSpectrum: the current input spectrum in dB (numBins values).
    void computeCorrection(const float* currentSpectrumDb)
    {
        if (!hasCapturedData) return;

        for (int i = 0; i < numBins; ++i)
        {
            // Correction = reference - current (clamped to ±24 dB)
            const float ref = capturedSpectrum[(size_t)i];
            const float cur = currentSpectrumDb[i];
            correctionDb[(size_t)i] = std::clamp(ref - cur, -24.0f, 24.0f);
        }
    }

    // Get the correction curve in dB.
    const float* getCorrectionDb() const { return correctionDb.data(); }
    const float* getCapturedSpectrum() const { return capturedSpectrum.data(); }
    int getNumBins() const { return numBins; }

    // Apply match EQ correction to a buffer (frequency-domain approach).
    // Simple approach: per-sample gain interpolation from the correction curve.
    void applyCorrection(float* L, float* R, int numSamples, double sampleRate)
    {
        if (!matchActive || !hasCapturedData) return;

        // Apply smoothed per-sample correction as a gain curve.
        // Map each sample's position to a frequency bin and apply gain.
        // Actually, for simplicity, apply a block-based approach:
        // Divide the spectrum into bands and apply gains.
        // This is a simplified approach - for production, FFT-based would be ideal.

        // Apply the correction as a gentle EQ curve using per-bin gain.
        // We'll process using FFT: forward FFT, multiply by correction, inverse FFT.
        if (numSamples > fftSize) return; // Safety

        applyToChannel(L, numSamples);
        applyToChannel(R, numSamples);
    }

private:
    juce::dsp::FFT fft;
    juce::dsp::WindowingFunction<float> window;

    bool capturing = false;
    bool hasCapturedData = false;
    bool matchActive = false;
    int captureFrames = 0;

    std::array<float, fftSize> fifoBuffer {};
    int fifoIndex = 0;

    // Captured reference spectrum in dB
    std::array<float, numBins> capturedSpectrum {};

    // Correction curve in dB (reference - current)
    std::array<float, numBins> correctionDb {};

    // Pre-computed correction gains (linear)
    std::array<float, numBins> correctionLinear {};

    void processFrame()
    {
        std::array<float, fftSize * 2> fftData {};
        std::copy(fifoBuffer.begin(), fifoBuffer.end(), fftData.begin());

        window.multiplyWithWindowingTable(fftData.data(), (size_t)fftSize);
        fft.performFrequencyOnlyForwardTransform(fftData.data());

        for (int i = 0; i < numBins; ++i)
        {
            float mag = fftData[(size_t)i] / (float)fftSize;
            float db = 20.0f * std::log10(std::max(mag, 1e-7f));
            capturedSpectrum[(size_t)i] += db;
        }

        captureFrames++;
    }

    void applyToChannel(float* data, int numSamples)
    {
        // Forward FFT
        std::array<float, fftSize> buf {};
        for (int i = 0; i < numSamples; ++i)
            buf[(size_t)i] = data[i];

        fft.performRealOnlyForwardTransform(buf.data());

        // Apply correction gains in frequency domain
        // Bin 0 (DC)
        buf[0] *= std::pow(10.0f, correctionDb[0] / 20.0f);
        // Nyquist
        buf[1] *= std::pow(10.0f, correctionDb[numBins - 1] / 20.0f);
        // Other bins
        for (int i = 1; i < fftSize / 2; ++i)
        {
            const int binIdx = std::min(i, numBins - 1);
            const float gain = std::pow(10.0f, correctionDb[(size_t)binIdx] / 20.0f);
            buf[(size_t)(i * 2)]     *= gain;
            buf[(size_t)(i * 2 + 1)] *= gain;
        }

        // Inverse FFT
        fft.performRealOnlyInverseTransform(buf.data());

        // Copy back
        for (int i = 0; i < numSamples; ++i)
            data[i] = buf[(size_t)i];
    }
};
