#pragma once
#include <juce_dsp/juce_dsp.h>
#include "Biquad.h"   // provides constexpr kPi
#include <array>
#include <vector>
#include <cmath>
#include <algorithm>

// Linear-phase EQ engine.
// Generates a symmetric FIR from the combined biquad magnitude response
// and applies it via overlap-add FFT convolution.
class LinearPhaseEngine
{
public:
    static constexpr int firLength  = 4096;               // FIR taps
    static constexpr int fftOrder   = 13;                 // 2^13 = 8192 (firLength * 2 for OLA)
    static constexpr int fftSize    = 1 << fftOrder;      // 8192
    static constexpr int latency    = firLength / 2;      // 2048 samples

    LinearPhaseEngine() : fft(fftOrder) {}

    void prepare(double sampleRate, int /*maxBlockSize*/)
    {
        sr = sampleRate;
        std::fill(overlapL.begin(), overlapL.end(), 0.0f);
        std::fill(overlapR.begin(), overlapR.end(), 0.0f);
        std::fill(firFreqDomain.begin(), firFreqDomain.end(), 0.0f);
        needsRebuild = true;
    }

    void reset()
    {
        std::fill(overlapL.begin(), overlapL.end(), 0.0f);
        std::fill(overlapR.begin(), overlapR.end(), 0.0f);
    }

    // Call once per block (or when parameters change) to rebuild the FIR kernel.
    // magnitudeDb: array of dB values at linearly-spaced frequency bins (0..sr/2).
    // numBins: number of magnitude bins (typically firLength/2 + 1 = 2049).
    void rebuildFromMagnitude(const float* magnitudeDb, int numBins)
    {
        // Build the frequency domain array for JUCE's real-only format.
        // JUCE requires 2*fftSize floats for performRealOnlyInverseTransform.
        // Packing: [Re0, ReN/2, Re1, Im1, Re2, Im2, ...]
        // For zero phase: all Im = 0, just set the real parts.
        std::fill(rebuildFreqBuf.begin(), rebuildFreqBuf.end(), 0.0f);

        // Bin 0 (DC)
        {
            const float db = magnitudeDb[0];
            rebuildFreqBuf[0] = std::pow(10.0f, db / 20.0f);
        }
        // Bins 1..N/2-1
        for (int i = 1; i < fftSize / 2; ++i)
        {
            const float frac = (float)i / (float)(fftSize / 2);
            const int srcBin = std::min((int)(frac * (float)(numBins - 1)), numBins - 1);
            const float db = magnitudeDb[srcBin];
            const float linGain = std::pow(10.0f, db / 20.0f);
            rebuildFreqBuf[(size_t)(i * 2)]     = linGain; // Real
            rebuildFreqBuf[(size_t)(i * 2 + 1)] = 0.0f;    // Imag (zero phase)
        }
        // Nyquist bin
        {
            const float db = magnitudeDb[numBins - 1];
            rebuildFreqBuf[1] = std::pow(10.0f, db / 20.0f); // JUCE packs Nyquist in [1]
        }

        // IFFT to get impulse response
        fft.performRealOnlyInverseTransform(rebuildFreqBuf.data());

        // Circular shift: move the center of the impulse to the middle of the FIR
        std::array<float, firLength> fir {};
        for (int i = 0; i < firLength; ++i)
        {
            int srcIdx = (i - firLength / 2 + fftSize) % fftSize;
            fir[(size_t)i] = rebuildFreqBuf[(size_t)srcIdx];
        }

        // Apply Hann window to the FIR
        for (int i = 0; i < firLength; ++i)
        {
            const float w = 0.5f * (1.0f - std::cos(2.0f * (float)kPi * (float)i / (float)(firLength - 1)));
            fir[(size_t)i] *= w;
        }

        // Pre-compute FFT of the FIR kernel (zero-padded to fftSize)
        std::fill(firFreqDomain.begin(), firFreqDomain.end(), 0.0f);
        for (int i = 0; i < firLength; ++i)
            firFreqDomain[(size_t)i] = fir[(size_t)i];

        fft.performRealOnlyForwardTransform(firFreqDomain.data());
        needsRebuild = false;
    }

    // Process a block of audio using overlap-add convolution.
    // inputL/R: pointers to input samples (also used as output).
    // numSamples: number of samples in this block.
    void processBlock(float* inputL, float* inputR, int numSamples)
    {
        // Process in chunks of (fftSize - firLength) to allow overlap
        const int maxChunk = fftSize - firLength;
        int offset = 0;

        while (offset < numSamples)
        {
            const int chunkSize = std::min(maxChunk, numSamples - offset);
            processChunk(inputL + offset, inputR + offset, chunkSize);
            offset += chunkSize;
        }
    }

    bool getNeedsRebuild() const { return needsRebuild; }

private:
    juce::dsp::FFT fft;
    double sr = 44100.0;
    bool needsRebuild = true;

    // Pre-computed FFT of the FIR kernel (JUCE real-only FFT needs 2*fftSize floats)
    std::array<float, fftSize * 2> firFreqDomain {};

    // Scratch buffer for rebuildFromMagnitude (avoids large stack allocation)
    std::array<float, fftSize * 2> rebuildFreqBuf {};

    // Scratch buffer for processChannel (avoids large stack allocation)
    std::array<float, fftSize * 2> channelBuf {};

    // Overlap buffers for overlap-add
    std::array<float, fftSize> overlapL {};
    std::array<float, fftSize> overlapR {};

    void processChunk(float* L, float* R, int n)
    {
        // --- Left channel ---
        processChannel(L, n, overlapL);
        // --- Right channel ---
        processChannel(R, n, overlapR);
    }

    void processChannel(float* data, int n, std::array<float, fftSize>& overlap)
    {
        // Zero-pad input to fftSize (JUCE real-only FFT needs 2*fftSize floats)
        std::fill(channelBuf.begin(), channelBuf.end(), 0.0f);
        for (int i = 0; i < n; ++i)
            channelBuf[(size_t)i] = data[i];

        // Forward FFT of input
        fft.performRealOnlyForwardTransform(channelBuf.data());

        // Complex multiply with FIR kernel in frequency domain
        // JUCE real-only format: [Re0, ReN/2, Re1, Im1, Re2, Im2, ...]
        // Bin 0 (DC): channelBuf[0] * firFreqDomain[0]
        channelBuf[0] *= firFreqDomain[0];
        // Bin N/2 (Nyquist): channelBuf[1] * firFreqDomain[1]
        channelBuf[1] *= firFreqDomain[1];
        // Bins 1..N/2-1: complex multiply
        for (int i = 1; i < fftSize / 2; ++i)
        {
            const int ri = i * 2;
            const int ii = i * 2 + 1;
            const float ar = channelBuf[(size_t)ri], ai = channelBuf[(size_t)ii];
            const float br = firFreqDomain[(size_t)ri], bi = firFreqDomain[(size_t)ii];
            channelBuf[(size_t)ri] = ar * br - ai * bi;
            channelBuf[(size_t)ii] = ar * bi + ai * br;
        }

        // Inverse FFT
        fft.performRealOnlyInverseTransform(channelBuf.data());

        // Overlap-add: add previous overlap, save new overlap
        for (int i = 0; i < n; ++i)
        {
            data[i] = channelBuf[(size_t)i] + overlap[(size_t)i];
            overlap[(size_t)i] = 0.0f;
        }

        // Save the tail as overlap for next chunk
        for (int i = n; i < fftSize; ++i)
            overlap[(size_t)(i - n)] += channelBuf[(size_t)i];
    }
};
