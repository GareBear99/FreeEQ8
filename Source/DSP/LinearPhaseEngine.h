#pragma once
#include <juce_dsp/juce_dsp.h>
#include "Biquad.h"   // provides constexpr kPi
#include <array>
#include <atomic>
#include <vector>
#include <cmath>
#include <algorithm>

// Linear-phase EQ engine.
// Generates a symmetric FIR from the combined biquad magnitude response
// and applies it via overlap-add FFT convolution.
//
// Threading (A5):
//   rebuildFromMagnitude() is called from a background juce::Thread (owned
//   by FreeEQ8AudioProcessor). It writes the result into a *writer-owned*
//   kernel slot, then atomically swaps slot indices with processBlock()
//   (audio thread) via the canonical swap-chain triple-buffer protocol.
//   processBlock() only mutates its local readKernelSlot and reads it
//   thereafter. writeKernelSlot, midKernelSlot, and readKernelSlot form a
//   permutation of {0,1,2} at every quiescent point, so the writer never
//   overwrites the slot the audio thread is reading, even if the writer
//   laps the reader (two rebuilds per audio block).
class LinearPhaseEngine
{
public:
    static constexpr int firLength  = 4096;               // FIR taps
    static constexpr int fftOrder   = 13;                 // 2^13 = 8192 (firLength * 2 for OLA)
    static constexpr int fftSize    = 1 << fftOrder;      // 8192
    static constexpr int latency    = firLength / 2;      // 2048 samples
    static constexpr int kNumKernelSlots = 3;

    LinearPhaseEngine() : fft(fftOrder) {}

    void prepare(double sampleRate, int /*maxBlockSize*/)
    {
        sr = sampleRate;
        std::fill(overlapL.begin(), overlapL.end(), 0.0f);
        std::fill(overlapR.begin(), overlapR.end(), 0.0f);
        for (auto& k : kernelSlots)
            std::fill(k.begin(), k.end(), 0.0f);
        writeKernelSlot = 0;
        midKernelSlot.store(1, std::memory_order_relaxed);
        readKernelSlot  = 2;
        kernelFresh.store(false, std::memory_order_relaxed);
        kernelHasBeenLoaded = false;
        needsRebuild = true;
    }

    void reset()
    {
        std::fill(overlapL.begin(), overlapL.end(), 0.0f);
        std::fill(overlapR.begin(), overlapR.end(), 0.0f);
    }

    // Called from the background rebuild thread. Fills the writer's private
    // kernel slot, then atomically swaps with midKernelSlot and flags a new
    // kernel as available. Safe for the writer to run arbitrarily faster
    // than the reader: the reader's readKernelSlot is never touched.
    // magnitudeDb: array of dB values at linearly-spaced frequency bins (0..sr/2).
    // numBins: number of magnitude bins (typically firLength/2 + 1 = 2049).
    void rebuildFromMagnitude(const float* magnitudeDb, int numBins)
    {
        auto& writeBuf = kernelSlots[(size_t)writeKernelSlot];

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

        // Pre-compute FFT of the FIR kernel into the writer's private slot.
        std::fill(writeBuf.begin(), writeBuf.end(), 0.0f);
        for (int i = 0; i < firLength; ++i)
            writeBuf[(size_t)i] = fir[(size_t)i];

        fft.performRealOnlyForwardTransform(writeBuf.data());

        // Publish via swap-chain: swap writer's slot with midKernelSlot.
        // Release so the audio thread's acquire in processBlock() sees the
        // filled kernel bytes.
        writeKernelSlot = midKernelSlot.exchange(writeKernelSlot,
                                                 std::memory_order_release);
        kernelFresh.store(true, std::memory_order_release);
        needsRebuild = false;
    }

    // Process a block of audio using overlap-add convolution.
    // inputL/R: pointers to input samples (also used as output).
    // numSamples: number of samples in this block.
    //
    // At the top of each block, if the background rebuild thread has
    // published a new kernel (kernelFresh=true), we atomically swap the
    // reader's private slot with midKernelSlot, which gives us the latest
    // rebuild and returns our previously-held slot to the pool. The reader
    // then uses its private slot exclusively for the rest of the block —
    // the writer cannot touch it.
    //
    // If no kernel has been published yet (cold-start before the background
    // rebuild thread completes its first pass), we pass audio through
    // unchanged rather than apply a zero kernel.
    void processBlock(float* inputL, float* inputR, int numSamples)
    {
        if (kernelFresh.exchange(false, std::memory_order_acquire))
        {
            // Swap in the latest-published kernel; return our old slot.
            readKernelSlot = midKernelSlot.exchange(readKernelSlot,
                                                    std::memory_order_acquire);
            kernelHasBeenLoaded = true;
        }

        if (!kernelHasBeenLoaded)
            return; // pass-through until first kernel is ready

        const float* kernel = kernelSlots[(size_t)readKernelSlot].data();

        const int maxChunk = fftSize - firLength;
        int offset = 0;
        while (offset < numSamples)
        {
            const int chunkSize = std::min(maxChunk, numSamples - offset);
            processChunk(inputL + offset, inputR + offset, chunkSize, kernel);
            offset += chunkSize;
        }
    }

    bool getNeedsRebuild() const { return needsRebuild; }
    bool isKernelReady() const
    {
        // Reader sees a kernel once it has successfully swapped at least one
        // in from the publisher.
        return kernelHasBeenLoaded || kernelFresh.load(std::memory_order_acquire);
    }

private:
    juce::dsp::FFT fft;
    double sr = 44100.0;
    bool needsRebuild = true;
    bool kernelHasBeenLoaded = false;   // audio-thread local

    // Swap-chain triple-buffer for the FIR kernel. See class header comment.
    std::array<std::array<float, fftSize * 2>, kNumKernelSlots> kernelSlots {};
    std::atomic<int>  midKernelSlot   { 1 };
    std::atomic<bool> kernelFresh     { false };

    // Private per-thread slot indices. writeKernelSlot is only touched by
    // the rebuild thread; readKernelSlot is only touched by the audio thread.
    int writeKernelSlot = 0;
    int readKernelSlot  = 2;

    // Scratch buffer for rebuildFromMagnitude (background thread only)
    std::array<float, fftSize * 2> rebuildFreqBuf {};

    // Scratch buffer for processChannel (audio thread only)
    std::array<float, fftSize * 2> channelBuf {};

    // Overlap buffers for overlap-add
    std::array<float, fftSize> overlapL {};
    std::array<float, fftSize> overlapR {};

    void processChunk(float* L, float* R, int n, const float* kernel)
    {
        processChannel(L, n, overlapL, kernel);
        processChannel(R, n, overlapR, kernel);
    }

    void processChannel(float* data, int n, std::array<float, fftSize>& overlap,
                        const float* kernel)
    {
        // Zero-pad input to fftSize (JUCE real-only FFT needs 2*fftSize floats)
        std::fill(channelBuf.begin(), channelBuf.end(), 0.0f);
        for (int i = 0; i < n; ++i)
            channelBuf[(size_t)i] = data[i];

        // Forward FFT of input
        fft.performRealOnlyForwardTransform(channelBuf.data());

        // Complex multiply with FIR kernel in frequency domain.
        // JUCE real-only format: [Re0, ReN/2, Re1, Im1, Re2, Im2, ...]
        channelBuf[0] *= kernel[0];               // DC
        channelBuf[1] *= kernel[1];               // Nyquist
        for (int i = 1; i < fftSize / 2; ++i)
        {
            const int ri = i * 2;
            const int ii = i * 2 + 1;
            const float ar = channelBuf[(size_t)ri], ai = channelBuf[(size_t)ii];
            const float br = kernel[(size_t)ri],     bi = kernel[(size_t)ii];
            channelBuf[(size_t)ri] = ar * br - ai * bi;
            channelBuf[(size_t)ii] = ar * bi + ai * br;
        }

        fft.performRealOnlyInverseTransform(channelBuf.data());

        for (int i = 0; i < n; ++i)
        {
            data[i] = channelBuf[(size_t)i] + overlap[(size_t)i];
            overlap[(size_t)i] = 0.0f;
        }

        for (int i = n; i < fftSize; ++i)
            overlap[(size_t)(i - n)] += channelBuf[(size_t)i];
    }
};
