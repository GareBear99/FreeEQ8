#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include "DSP/EQBand.h"
#include "DSP/SpectrumFIFO.h"
#include "Presets/PresetManager.h"
#include <atomic>

class FreeEQ8AudioProcessor : public juce::AudioProcessor
{
public:
    FreeEQ8AudioProcessor();
    ~FreeEQ8AudioProcessor() override = default;

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

    juce::AudioProcessorValueTreeState apvts;

    // Spectrum analyzer FIFO (post-EQ by default)
    SpectrumFIFO spectrumFifo;
    SpectrumFIFO preSpectrumFifo;  // Pre-EQ spectrum

    // Preset manager
    std::unique_ptr<PresetManager> presetManager;

    // Output metering (read from UI thread)
    std::atomic<float> meterPeakL { 0.0f };
    std::atomic<float> meterPeakR { 0.0f };
    std::atomic<float> meterRmsL  { 0.0f };
    std::atomic<float> meterRmsR  { 0.0f };

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParams();

    std::array<EQBand, 8> bands;
    double sr = 44100.0;

    // Oversampling
    std::unique_ptr<juce::dsp::Oversampling<float>> oversampler;
    int currentOversamplingOrder = 0;  // 0 = 1x, 1 = 2x, 2 = 4x, 3 = 8x

    void syncBandsFromParams();
    void rebuildOversampler(int order, double sampleRate, int samplesPerBlock);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FreeEQ8AudioProcessor)
};
