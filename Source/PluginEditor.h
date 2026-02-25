#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"
#include "UI/ResponseCurveComponent.h"

class FreeEQ8AudioProcessorEditor : public juce::AudioProcessorEditor,
                                    public juce::Timer
{
public:
    explicit FreeEQ8AudioProcessorEditor(FreeEQ8AudioProcessor&);
    ~FreeEQ8AudioProcessorEditor() override = default;

    void paint(juce::Graphics&) override;
    void resized() override;
    void timerCallback() override;

private:
    FreeEQ8AudioProcessor& proc;

    // Response curve + spectrum display
    ResponseCurveComponent responseCurve;

    struct BandUI
    {
        juce::ToggleButton on;
        juce::ToggleButton solo;
        juce::ComboBox type;
        juce::Slider freq, q, gain;
        juce::Label freqLabel, qLabel, gainLabel;

        std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> onAtt;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> soloAtt;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> typeAtt;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> freqAtt, qAtt, gainAtt;
    };

    std::array<BandUI, 8> ui;
    
    // Global controls
    juce::Slider outputGainSlider;
    juce::Slider scaleSlider;
    juce::ToggleButton adaptiveQButton;
    juce::Label outputGainLabel, scaleLabel;
    
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> outputGainAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> scaleAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> adaptiveQAtt;

    // Preset controls
    juce::ComboBox presetSelector;
    juce::TextButton saveButton { "Save" };
    juce::TextButton deleteButton { "Del" };

    void refreshPresetList();
    void onPresetSelected();
    void onSaveClicked();
    void onDeleteClicked();

    // Spectrum pre/post toggle
    juce::ToggleButton spectrumPostToggle;
    bool showPostSpectrum = true;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FreeEQ8AudioProcessorEditor)
};
