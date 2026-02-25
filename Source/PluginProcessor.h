#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include "DSP/EQBand.h"
#include "DSP/SpectrumFIFO.h"
#include "DSP/LinearPhaseEngine.h"
#include "DSP/MatchEQ.h"
#include "Presets/PresetManager.h"
#include <atomic>

class FreeEQ8AudioProcessor : public juce::AudioProcessor,
                               public juce::AudioProcessorValueTreeState::Listener
{
public:
    FreeEQ8AudioProcessor();
    ~FreeEQ8AudioProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;

    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "FreeEQ8"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    // Undo manager (shared with APVTS)
    juce::UndoManager undoManager { 30000, 30 };
    juce::AudioProcessorValueTreeState apvts;

    // Spectrum analyzer FIFO (post-EQ by default)
    SpectrumFIFO spectrumFifo;
    SpectrumFIFO preSpectrumFifo;

    // Preset manager
    std::unique_ptr<PresetManager> presetManager;

    // Output metering (read from UI thread)
    std::atomic<float> meterPeakL { 0.0f };
    std::atomic<float> meterPeakR { 0.0f };
    std::atomic<float> meterRmsL  { 0.0f };
    std::atomic<float> meterRmsR  { 0.0f };

    // Linear phase engine (public for UI to check state)
    LinearPhaseEngine linearPhaseEngine;

    // Match EQ (public for UI capture/match control)
    MatchEQ matchEQ;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParams();

    std::array<EQBand, 8> bands;
    double sr = 44100.0;
    int maxBlockSize = 512;

    // Oversampling
    std::unique_ptr<juce::dsp::Oversampling<float>> oversampler;
    int currentOversamplingOrder = 0;

    void syncBandsFromParams();
    void rebuildOversampler(int order, double sampleRate, int samplesPerBlock);
    void buildLinearPhaseMagnitude();

    // Band linking
    void parameterChanged(const juce::String& parameterID, float newValue) override;
    bool propagatingLink = false;
    float lastLinkedFreq[8] {};
    float lastLinkedGain[8] {};
    float lastLinkedQ[8] {};
    void initLinkTracking();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FreeEQ8AudioProcessor)
};
