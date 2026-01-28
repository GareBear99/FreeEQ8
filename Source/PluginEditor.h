#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"

class FreeEQ8AudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit FreeEQ8AudioProcessorEditor(FreeEQ8AudioProcessor&);
    ~FreeEQ8AudioProcessorEditor() override = default;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    FreeEQ8AudioProcessor& proc;

    struct BandUI
    {
        juce::ToggleButton on;
        juce::ComboBox type;
        juce::Slider freq, q, gain;

        std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> onAtt;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> typeAtt;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> freqAtt, qAtt, gainAtt;
    };

    std::array<BandUI, 8> ui;
    
    // Global controls
    juce::Slider outputGainSlider;
    juce::Slider scaleSlider;
    juce::ToggleButton adaptiveQButton;
    
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> outputGainAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> scaleAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> adaptiveQAtt;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FreeEQ8AudioProcessorEditor)
};
