#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_dsp/juce_dsp.h>
#include "../DSP/Biquad.h"
#include "../Config.h"
#include "../DSP/EQBand.h"

class FreeEQ8AudioProcessor;

class ResponseCurveComponent : public juce::Component,
                               public juce::Timer
{
public:
    explicit ResponseCurveComponent(FreeEQ8AudioProcessor& processor);
    ~ResponseCurveComponent() override = default;

    void paint(juce::Graphics& g) override;
    void resized() override;
    void timerCallback() override;

    // Mouse interaction for draggable nodes
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;
    void mouseUp(const juce::MouseEvent& e) override;
    void mouseMove(const juce::MouseEvent& e) override;

    // Spectrum analyzer data - call from the editor to push FFT magnitudes
    void pushSpectrumData(const float* magnitudes, int numBins, double sampleRate);

    int getSelectedBand() const { return selectedBand; }
    void setSelectedBand(int band) { selectedBand = band; repaint(); }

    // Band colors
    static juce::Colour getBandColour(int bandIndex);

private:
    FreeEQ8AudioProcessor& proc;

    // Frequency response calculation
    static constexpr int numPoints = 512;
    float magnitudes[numPoints] = {};
    float perBandMagnitudes[kNumBands][numPoints] = {};

    // Spectrum analyzer display data
    static constexpr int maxSpectrumBins = 2048;
    float spectrumMagnitudes[maxSpectrumBins] = {};
    float smoothedSpectrum[maxSpectrumBins] = {};
    int currentSpectrumSize = 0;
    double spectrumSampleRate = 44100.0;

    // Draggable nodes
    int selectedBand = -1;   // Currently selected band (-1 = none)
    int hoveredBand = -1;    // Band under cursor
    bool dragging = false;
    bool shiftDrag = false;  // Shift+drag adjusts Q
    float dragStartQ = 1.0f;
    float dragStartY = 0.0f;

    // Coordinate mapping
    float freqToX(float freqHz) const;
    float xToFreq(float x) const;
    float dbToY(float db) const;
    float yToDb(float y) const;

    // Compute magnitude response for a single biquad at a given frequency
    static float computeMagnitudeDb(const Biquad& bq, double freq, double sampleRate);

    // Update all magnitude arrays from current processor state
    void updateResponseCurve();

    // Paint helpers
    void paintGrid(juce::Graphics& g);
    void paintSpectrum(juce::Graphics& g);
    void paintResponseCurve(juce::Graphics& g);
    void paintBandCurves(juce::Graphics& g);
    void paintNodes(juce::Graphics& g);
#if PROEQ8
    void paintPianoRoll(juce::Graphics& g);
    void paintCollisionWarnings(juce::Graphics& g);
#endif

    // Hit test for band nodes
    int hitTestNode(float x, float y) const;

    // Display range
    static constexpr float minFreq = 20.0f;
    static constexpr float maxFreq = 20000.0f;
    static constexpr float minDb = -24.0f;
    static constexpr float maxDb = 24.0f;
    static constexpr float nodeRadius = 7.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ResponseCurveComponent)
};
