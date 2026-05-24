#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_dsp/juce_dsp.h>
#include "../DSP/Biquad.h"
#include "../DSP/ResonanceDetector.h"
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

    // ── v2.2.3 additions ───────────────────────────────────────────────

    // Compact / mini-window mode (v2.2.3)
    // Identical DSP math (freqToX, dbToY, drag response) — only visual
    // density changes: fewer grid labels, smaller node labels, reduced
    // analyser resolution. Matching Ableton EQ Eight principle: one param
    // truth source, multiple view densities.
    void setCompactMode(bool compact);
    bool isCompactMode() const noexcept { return compactMode; }

    // Smart EQ suggestion overlay — called from editor timerCallback()
    // after ResonanceDetector::analyse(). Renders glowing suggestion nodes
    // on the response curve. Click to apply to next unused band.
    using Suggestions = std::array<ResonanceDetector::Suggestion, ResonanceDetector::kMaxSuggestions>;
    void setSuggestions(const Suggestions& s) { suggestions = s; }

    // Pre-ring warning flag — set when DrumPunch mode + linear phase active
    void setPreRingWarning(bool show) { showPreRingWarning = show; }

private:
    FreeEQ8AudioProcessor& proc;

    // Frequency response calculation
    static constexpr int numPoints     = 512;
    static constexpr int numPointsCompact = 128;  // compact mode — enough for visual
    float magnitudes[numPoints] = {};
    float perBandMagnitudes[kNumBands][numPoints] = {};

    // Spectrum analyzer display data
    static constexpr int maxSpectrumBins = 2048;
    float spectrumMagnitudes[maxSpectrumBins] = {};
    float smoothedSpectrum[maxSpectrumBins] = {};
    int currentSpectrumSize = 0;
    double spectrumSampleRate = 44100.0;

    // Draggable nodes
    int selectedBand = -1;
    int hoveredBand  = -1;
    bool dragging    = false;
    bool shiftDrag   = false;
    float dragStartQ = 1.0f;
    float dragStartY = 0.0f;

    // v2.2.3 state
    bool compactMode       = false;
    bool showPreRingWarning = false;
    Suggestions suggestions {};

    // Cached background image (grid + labels) — rebuilt only on resize/mode change
    // setBufferedToImage(true) on the static grid layer is handled in setCompactMode()
    juce::Image cachedGrid;
    bool gridDirty = true;

    // Coordinate mapping (NEVER changes between full/compact — Ableton parity rule)
    float freqToX(float freqHz) const;
    float xToFreq(float x) const;
    float dbToY(float db) const;
    float yToDb(float y) const;

    // Magnitude computation
    static float computeMagnitudeDb(const Biquad& bq, double freq, double sampleRate);

    // Update magnitude arrays
    void updateResponseCurve();

    // Paint helpers
    void paintGrid(juce::Graphics& g);
    void paintSpectrum(juce::Graphics& g);
    void paintResponseCurve(juce::Graphics& g);
    void paintBandCurves(juce::Graphics& g);
    void paintNodes(juce::Graphics& g);
    void paintSuggestions(juce::Graphics& g);    // v2.2.3 — suggestion overlay
    void paintPreRingWarning(juce::Graphics& g); // v2.2.3 — DrumPunch+LP warning
#if PROEQ8
    void paintPianoRoll(juce::Graphics& g);
    void paintCollisionWarnings(juce::Graphics& g);
#endif

    int hitTestNode(float x, float y) const;
    int hitTestSuggestion(float x, float y) const; // v2.2.3

    // Display range — CONSTANT regardless of view mode (Ableton parity)
    static constexpr float minFreq   = 20.0f;
    static constexpr float maxFreq   = 20000.0f;
    static constexpr float minDb     = -24.0f;
    static constexpr float maxDb     = 24.0f;
    static constexpr float nodeRadius = 7.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ResponseCurveComponent)
};
