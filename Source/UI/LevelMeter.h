#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <atomic>
#include <cmath>
#include <algorithm>

// Stereo level meter with peak hold and RMS display.
// Reads from external atomic floats supplied by the audio processor.
class LevelMeter : public juce::Component, public juce::Timer
{
public:
    LevelMeter(std::atomic<float>& peakLRef, std::atomic<float>& peakRRef,
               std::atomic<float>& rmsLRef, std::atomic<float>& rmsRRef)
        : peakLSrc(peakLRef), peakRSrc(peakRRef),
          rmsLSrc(rmsLRef), rmsRSrc(rmsRRef)
    {
        startTimerHz(30);
    }

    void paint(juce::Graphics& g) override
    {
        const auto bounds = getLocalBounds().toFloat();
        g.fillAll(juce::Colour(0xFF0D0D1A));

        const float meterGap = 3.0f;
        const float labelH = 12.0f;
        const float meterW = (bounds.getWidth() - meterGap) * 0.5f;
        const float meterH = bounds.getHeight() - labelH - 2.0f;
        const float meterTop = 0.0f;

        // Left meter
        paintSingleMeter(g, bounds.getX(), meterTop, meterW, meterH,
                         displayRmsL, displayPeakL, peakHoldL);
        // Right meter
        paintSingleMeter(g, bounds.getX() + meterW + meterGap, meterTop, meterW, meterH,
                         displayRmsR, displayPeakR, peakHoldR);

        // Labels
        g.setColour(juce::Colours::white.withAlpha(0.5f));
        g.setFont(9.0f);
        g.drawText("L", (int)bounds.getX(), (int)(meterTop + meterH + 1), (int)meterW, (int)labelH,
                   juce::Justification::centred);
        g.drawText("R", (int)(bounds.getX() + meterW + meterGap), (int)(meterTop + meterH + 1),
                   (int)meterW, (int)labelH, juce::Justification::centred);
    }

    void timerCallback() override
    {
        const float newPeakL = peakLSrc.load(std::memory_order_relaxed);
        const float newPeakR = peakRSrc.load(std::memory_order_relaxed);
        const float newRmsL  = rmsLSrc.load(std::memory_order_relaxed);
        const float newRmsR  = rmsRSrc.load(std::memory_order_relaxed);

        // Smooth decay
        const float peakDecay = 0.85f;
        const float rmsDecay  = 0.80f;

        displayPeakL = std::max(newPeakL, displayPeakL * peakDecay);
        displayPeakR = std::max(newPeakR, displayPeakR * peakDecay);
        displayRmsL  = std::max(newRmsL, displayRmsL * rmsDecay);
        displayRmsR  = std::max(newRmsR, displayRmsR * rmsDecay);

        // Peak hold (stays for ~1 second, then decays)
        if (newPeakL >= peakHoldL) { peakHoldL = newPeakL; peakHoldTimerL = 30; }
        else if (--peakHoldTimerL <= 0) peakHoldL *= 0.95f;

        if (newPeakR >= peakHoldR) { peakHoldR = newPeakR; peakHoldTimerR = 30; }
        else if (--peakHoldTimerR <= 0) peakHoldR *= 0.95f;

        repaint();
    }

private:
    std::atomic<float>& peakLSrc;
    std::atomic<float>& peakRSrc;
    std::atomic<float>& rmsLSrc;
    std::atomic<float>& rmsRSrc;

    float displayPeakL = 0.0f, displayPeakR = 0.0f;
    float displayRmsL = 0.0f, displayRmsR = 0.0f;
    float peakHoldL = 0.0f, peakHoldR = 0.0f;
    int peakHoldTimerL = 0, peakHoldTimerR = 0;

    // Map dB to a 0..1 range for the meter.  -60 dB = 0, 0 dB = 1, +6 dB = top clipping zone.
    static float dbToNorm(float db)
    {
        return std::clamp((db + 60.0f) / 66.0f, 0.0f, 1.0f);  // -60 to +6
    }

    static float ampToDb(float amp)
    {
        return 20.0f * std::log10(std::max(amp, 1e-7f));
    }

    void paintSingleMeter(juce::Graphics& g, float x, float y, float w, float h,
                           float rmsAmp, float peakAmp, float holdAmp) const
    {
        // Background
        g.setColour(juce::Colour(0xFF1A1A2E));
        g.fillRoundedRectangle(x, y, w, h, 2.0f);

        const float rmsDb  = ampToDb(rmsAmp);
        const float peakDb = ampToDb(peakAmp);
        const float holdDb = ampToDb(holdAmp);

        const float rmsNorm  = dbToNorm(rmsDb);
        const float peakNorm = dbToNorm(peakDb);
        const float holdNorm = dbToNorm(holdDb);

        // RMS bar (filled, dimmer)
        {
            const float barH = h * rmsNorm;
            auto colour = rmsDb > -6.0f ? juce::Colour(0xFFFF9B42)
                        : rmsDb > -20.0f ? juce::Colour(0xFF8BC34A)
                        : juce::Colour(0xFF42A5F5);
            g.setColour(colour.withAlpha(0.5f));
            g.fillRoundedRectangle(x + 1, y + h - barH, w - 2, barH, 1.0f);
        }

        // Peak bar (filled, brighter, thinner overlay)
        {
            const float barH = h * peakNorm;
            auto colour = peakDb > 0.0f ? juce::Colour(0xFFE84855)
                        : peakDb > -6.0f ? juce::Colour(0xFFFF9B42)
                        : peakDb > -20.0f ? juce::Colour(0xFF8BC34A)
                        : juce::Colour(0xFF42A5F5);
            g.setColour(colour.withAlpha(0.8f));
            g.fillRoundedRectangle(x + 1, y + h - barH, w - 2, barH, 1.0f);
        }

        // Peak hold line
        {
            const float holdY = y + h - h * holdNorm;
            auto colour = holdDb > 0.0f ? juce::Colour(0xFFE84855)
                        : juce::Colour(0xFFFFFFFF);
            g.setColour(colour.withAlpha(0.9f));
            g.fillRect(x + 1, holdY, w - 2, 2.0f);
        }

        // Border
        g.setColour(juce::Colours::white.withAlpha(0.15f));
        g.drawRoundedRectangle(x, y, w, h, 2.0f, 1.0f);
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LevelMeter)
};
