#pragma once
#include <juce_dsp/juce_dsp.h>
#include "Biquad.h"

    // EQBand with lightweight parameter smoothing.
    // We update biquad coefficients at a fixed interval while smoothing is active,
    // which avoids per-sample coefficient rebuild cost.
    struct EQBand
    {
        bool enabled = true;
        Biquad::Type type = Biquad::Type::Bell;

        float freqHz = 1000.0f;
        float Q = 1.0f;
        float gainDb = 0.0f;

        // Targets coming from parameters
        float targetFreqHz = 1000.0f;
        float targetQ = 1.0f;
        float targetGainDb = 0.0f;

        // Smoothers
        juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> freqSm;
        juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> qSm;
        juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> gainSm;

        // Coefficient update interval while smoothing (in samples)
        int coeffUpdateInterval = 16;
        int intervalCounter = 0;

        Biquad biquad;

        void reset(double sampleRate)
        {
            biquad.reset();
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
                        float newFreqHz, float newQ, float newGainDb)
        {
            enabled = isEnabled;
            type = newType;

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
                biquad.set(type, sampleRate, freqHz, Q, gainDb);
            }

            intervalCounter = 0;
        }

        inline void maybeUpdateCoeffs(double sampleRate)
        {
            if (!enabled) return;

            // If smoothing is active, update at a fixed interval
            if (freqSm.isSmoothing() || qSm.isSmoothing() || gainSm.isSmoothing())
            {
                if (intervalCounter++ >= coeffUpdateInterval)
                {
                    intervalCounter = 0;
                    freqHz = freqSm.getNextValue();
                    Q      = qSm.getNextValue();
                    gainDb = gainSm.getNextValue();
                    biquad.set(type, sampleRate, freqHz, Q, gainDb);
                }
                else
                {
                    // Advance smoothers without rebuilding coefficients
                    (void)freqSm.getNextValue();
                    (void)qSm.getNextValue();
                    (void)gainSm.getNextValue();
                }
            }
        }

        inline void process(float& l, float& r)
        {
            if (!enabled) return;
            l = biquad.processL(l);
            r = biquad.processR(r);
        }
    };
