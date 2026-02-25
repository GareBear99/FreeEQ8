#pragma once
#include <juce_dsp/juce_dsp.h>
#include "Biquad.h"
#include <array>

// Channel routing for Mid/Side and L/R independent processing.
enum class ChannelRoute { Both = 0, LeftOrMid = 1, RightOrSide = 2 };

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

        freqSm.reset(sampleRate, 0.02);   // 20ms
        qSm.reset(sampleRate, 0.02);
        gainSm.reset(sampleRate, 0.02);

        freqSm.setCurrentAndTargetValue(freqHz);
        qSm.setCurrentAndTargetValue(Q);
        gainSm.setCurrentAndTargetValue(gainDb);

        targetFreqHz = freqHz;
        targetQ = Q;
        targetGainDb = gainDb;

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

        if (freqSm.isSmoothing() || qSm.isSmoothing() || gainSm.isSmoothing())
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

    inline void process(float& l, float& r)
    {
        if (!enabled) return;

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
    }

private:
    void setAllStages(double sampleRate)
    {
        for (int s = 0; s < numStages; ++s)
            biquads[(size_t)s].set(type, sampleRate, freqHz, Q, gainDb);
    }
};
