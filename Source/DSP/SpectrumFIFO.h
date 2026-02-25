#pragma once
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <atomic>

// Lock-free single-producer / single-consumer FIFO for spectrum analysis.
// The audio thread pushes samples; the UI thread pulls and runs FFT.
class SpectrumFIFO
{
public:
    static constexpr int fftOrder  = 12;             // 4096-point FFT
    static constexpr int fftSize   = 1 << fftOrder;  // 4096
    static constexpr int numBins   = fftSize / 2;    // 2048

    SpectrumFIFO()
        : fft(fftOrder),
          window(fftSize, juce::dsp::WindowingFunction<float>::hann)
    {
        std::fill(fifoBuffer.begin(), fifoBuffer.end(), 0.0f);
        std::fill(fftData.begin(),    fftData.end(),    0.0f);
        std::fill(outputMagnitudes.begin(), outputMagnitudes.end(), -100.0f);
    }

    // Call from audio thread: push interleaved mono samples (sum L+R).
    void pushSamples(const float* data, int numSamples)
    {
        for (int i = 0; i < numSamples; ++i)
        {
            fifoBuffer[(size_t)fifoWriteIndex] = data[i];
            fifoWriteIndex = (fifoWriteIndex + 1) % fftSize;

            if (fifoWriteIndex == 0)
                dataReady.store(true, std::memory_order_release);
        }
    }

    // Call from audio thread: push a stereo buffer (will sum to mono).
    void pushBlock(const float* L, const float* R, int numSamples)
    {
        for (int i = 0; i < numSamples; ++i)
        {
            const float mono = (L[i] + R[i]) * 0.5f;
            fifoBuffer[(size_t)fifoWriteIndex] = mono;
            fifoWriteIndex = (fifoWriteIndex + 1) % fftSize;

            if (fifoWriteIndex == 0)
                dataReady.store(true, std::memory_order_release);
        }
    }

    // Call from UI thread: returns true if new data was processed.
    bool processIfReady()
    {
        if (!dataReady.load(std::memory_order_acquire))
            return false;

        dataReady.store(false, std::memory_order_release);

        // Copy FIFO into FFT buffer
        std::copy(fifoBuffer.begin(), fifoBuffer.end(), fftData.begin());
        std::fill(fftData.begin() + fftSize, fftData.end(), 0.0f);

        // Apply window
        window.multiplyWithWindowingTable(fftData.data(), (size_t)fftSize);

        // Forward FFT (in-place, real input → complex output packed)
        fft.performFrequencyOnlyForwardTransform(fftData.data());

        // Convert to dB
        for (int i = 0; i < numBins; ++i)
        {
            float mag = fftData[(size_t)i];
            // Normalize by FFT size
            mag /= (float)fftSize;
            // Convert to dB
            outputMagnitudes[(size_t)i] = 20.0f * std::log10(std::max(mag, 1e-7f));
        }

        return true;
    }

    const float* getMagnitudes() const { return outputMagnitudes.data(); }
    int getNumBins() const { return numBins; }

private:
    juce::dsp::FFT fft;
    juce::dsp::WindowingFunction<float> window;

    std::array<float, fftSize>      fifoBuffer {};
    std::array<float, fftSize * 2>  fftData {};
    std::array<float, numBins>      outputMagnitudes {};

    int fifoWriteIndex = 0;
    std::atomic<bool> dataReady { false };
};
