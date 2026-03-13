#pragma once
#include <juce_dsp/juce_dsp.h>
#include "Biquad.h"
#include "../Config.h"
#include <array>

// Channel routing for Mid/Side and L/R independent processing.
enum class ChannelRoute { Both = 0, LeftOrMid = 1, RightOrSide = 2 };

// Saturation / waveshaper modes. FreeEQ8 only uses Tanh; ProEQ8 adds Tube/Tape/Transistor.
enum class SaturationType { Tanh = 0, Tube = 1, Tape = 2, Transistor = 3 };

// EQBand with lightweight parameter smoothing and cascaded biquads.
// Supports 1/2/4 cascaded stages for 12/24/48 dB/oct slopes.
struct EQBand
{
    bool enabled = true;
    Biquad::Type type = Biquad::Type::Bell;
    ChannelRoute channelRoute = ChannelRoute::Both;

    float freqHz = 1000.0f;
    float Q = 1.0f;
    float gainDb = 0.0f;

    // Targets coming from parameters
    float targetFreqHz = 1000.0f;
    float targetQ = 1.0f;
    float targetGainDb = 0.0f;

    // Drive / saturation (0 = off, 1 = full)
    float driveAmount = 0.0f;
    SaturationType satType = SaturationType::Tanh;

    // Dynamic EQ state
    bool dynEnabled = false;
    float dynThreshDb = -20.0f;
    float dynRatio = 4.0f;
    float dynAttackMs = 10.0f;
    float dynReleaseMs = 100.0f;
    float envLevel = 0.0f;       // current envelope level (linear)
    float dynGainMod = 0.0f;     // current dynamic gain modulation in dB

    // Cascaded biquad stages: 1 = 12 dB/oct, 2 = 24 dB/oct, 4 = 48 dB/oct
    int numStages = 1;
    static constexpr int maxStages = 4;
    std::array<Biquad, maxStages> biquads;

    // Smoothers
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> freqSm;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> qSm;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> gainSm;

    // Coefficient update interval while smoothing (in samples)
    int coeffUpdateInterval = 16;
    int intervalCounter = 0;

    void reset(double sampleRate)
    {
        for (auto& bq : biquads)
            bq.reset();
        scBiquad.reset();

        freqSm.reset(sampleRate, 0.02);   // 20ms
        qSm.reset(sampleRate, 0.02);
        gainSm.reset(sampleRate, 0.02);

        freqSm.setCurrentAndTargetValue(freqHz);
        qSm.setCurrentAndTargetValue(Q);
        gainSm.setCurrentAndTargetValue(gainDb);

        targetFreqHz = freqHz;
        targetQ = Q;
        targetGainDb = gainDb;

        envLevel = 0.0f;
        dynGainMod = 0.0f;
        intervalCounter = 0;
    }

    void beginBlock(double sampleRate, bool isEnabled, Biquad::Type newType,
                    float newFreqHz, float newQ, float newGainDb,
                    int newNumStages = 1, ChannelRoute newRoute = ChannelRoute::Both)
    {
        enabled = isEnabled;
        type = newType;
        numStages = std::clamp(newNumStages, 1, maxStages);
        channelRoute = newRoute;

        targetFreqHz = newFreqHz;
        targetQ = newQ;
        targetGainDb = newGainDb;

        // If targets changed, set smoothers
        if (freqSm.getTargetValue() != targetFreqHz) freqSm.setTargetValue(targetFreqHz);
        if (qSm.getTargetValue() != targetQ)         qSm.setTargetValue(targetQ);
        if (gainSm.getTargetValue() != targetGainDb) gainSm.setTargetValue(targetGainDb);

        // If not smoothing, update coefficients once per block
        if (!enabled) return;

        if (!(freqSm.isSmoothing() || qSm.isSmoothing() || gainSm.isSmoothing()))
        {
            freqHz = targetFreqHz;
            Q = targetQ;
            gainDb = targetGainDb;
            setAllStages(sampleRate);
        }

        intervalCounter = 0;
    }

    inline void maybeUpdateCoeffs(double sampleRate)
    {
        if (!enabled) return;

        // Force periodic coefficient updates when dynamic EQ is active (dynGainMod changes per-sample)
        if (dynEnabled || freqSm.isSmoothing() || qSm.isSmoothing() || gainSm.isSmoothing())
        {
            if (intervalCounter++ >= coeffUpdateInterval)
            {
                intervalCounter = 0;
                freqHz = freqSm.getNextValue();
                Q      = qSm.getNextValue();
                gainDb = gainSm.getNextValue();
                setAllStages(sampleRate);
            }
            else
            {
                (void)freqSm.getNextValue();
                (void)qSm.getNextValue();
                (void)gainSm.getNextValue();
            }
        }
    }

    // Update dynamic EQ envelope from the input signal (call per sample, before process).
    inline void updateDynamicEnvelope(float l, float r, double sampleRate)
    {
        if (!dynEnabled || !enabled) { dynGainMod = 0.0f; return; }

        // Sidechain: bandpass-filter the input at the band frequency
        const float scMono = (l + r) * 0.5f;
        const float scFiltered = scBiquad.processL(scMono);
        const float rectified = std::abs(scFiltered);

        // One-pole envelope follower
        const float attackCoeff  = 1.0f - std::exp(-1.0f / (float)(sampleRate * dynAttackMs * 0.001f));
        const float releaseCoeff = 1.0f - std::exp(-1.0f / (float)(sampleRate * dynReleaseMs * 0.001f));

        if (rectified > envLevel)
            envLevel += attackCoeff * (rectified - envLevel);
        else
            envLevel += releaseCoeff * (rectified - envLevel);

        // Compute gain reduction
        const float envDb = 20.0f * std::log10(std::max(envLevel, 1e-7f));
        if (envDb > dynThreshDb)
        {
            const float overDb = envDb - dynThreshDb;
            dynGainMod = -overDb * (1.0f - 1.0f / dynRatio);
        }
        else
        {
            dynGainMod = 0.0f;
        }
    }

    inline void process(float& l, float& r)
    {
        if (!enabled) return;

        // Dynamic gain modulation is baked into biquad coefficients via setAllStages(),
        // so no separate pre-filter volume adjustment is needed here.

        switch (channelRoute)
        {
            case ChannelRoute::Both:
                for (int s = 0; s < numStages; ++s)
                {
                    l = biquads[(size_t)s].processL(l);
                    r = biquads[(size_t)s].processR(r);
                }
                break;

            case ChannelRoute::LeftOrMid:
                for (int s = 0; s < numStages; ++s)
                    l = biquads[(size_t)s].processL(l);
                break;

            case ChannelRoute::RightOrSide:
                for (int s = 0; s < numStages; ++s)
                    r = biquads[(size_t)s].processR(r);
                break;
        }

        // Apply saturation/drive
        if (driveAmount > 0.001f)
            applySaturation(l, r);
    }

    // Saturation waveshaper — applies gain-compensated nonlinearity
    inline void applySaturation(float& l, float& r) const
    {
        const float d = 1.0f + driveAmount * 9.0f; // 1x to 10x drive

        switch (satType)
        {
            case SaturationType::Tanh:
            {
                const float invTanhD = 1.0f / std::tanh(d);
                l = std::tanh(l * d) * invTanhD;
                r = std::tanh(r * d) * invTanhD;
            } break;

            case SaturationType::Tube:
            {
                // Asymmetric soft clip: even harmonics from positive bias
                auto tube = [d](float x) -> float {
                    float xd = x * d;
                    // Positive half clips softer (tube-style asymmetry)
                    if (xd >= 0.0f)
                        return xd / (1.0f + xd);
                    else
                        return xd / (1.0f - 0.5f * xd);
                };
                float normL = tube(1.0f);  // normalise so unity input → unity output
                l = tube(l) / normL;
                r = tube(r) / normL;
            } break;

            case SaturationType::Tape:
            {
                // Arctangent saturation with bias — warm, smooth
                const float invAtanD = 1.0f / std::atan(d);
                l = std::atan(l * d) * invAtanD;
                r = std::atan(r * d) * invAtanD;
            } break;

            case SaturationType::Transistor:
            {
                // Hard clip — aggressive, odd harmonics
                const float invD = 1.0f / d;
                l = std::clamp(l * d, -1.0f, 1.0f) * invD * d;  // clip then restore level
                l = std::clamp(l, -1.0f, 1.0f);
                r = std::clamp(r * d, -1.0f, 1.0f) * invD * d;
                r = std::clamp(r, -1.0f, 1.0f);
            } break;
        }
    }

private:
    // Sidechain bandpass for dynamic EQ envelope
    Biquad scBiquad;

    void setAllStages(double sampleRate)
    {
        // Incorporate dynamic gain modulation into the filter coefficients
        const float effectiveGainDb = gainDb + dynGainMod;

        for (int s = 0; s < numStages; ++s)
            biquads[(size_t)s].set(type, sampleRate, freqHz, Q, effectiveGainDb);

        // Sidechain bandpass tracks band center frequency for envelope detection
        scBiquad.set(Biquad::Type::Bandpass, sampleRate, freqHz, 2.0, 0.0);
    }
};
