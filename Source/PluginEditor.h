#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"
#include "UI/ResponseCurveComponent.h"
#include "UI/LevelMeter.h"

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
        juce::ComboBox slope;
        juce::ComboBox channel;
        juce::ComboBox link;
        juce::Slider freq, q, gain, drive;
        juce::Label freqLabel, qLabel, gainLabel;
        // Dynamic EQ
        juce::ToggleButton dynOn;
        juce::Slider dynThresh;

        std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> onAtt;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> soloAtt;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> typeAtt;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> slopeAtt;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> channelAtt;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> linkAtt;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> freqAtt, qAtt, gainAtt, driveAtt;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> dynOnAtt;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> dynThreshAtt;
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

    // Oversampling + Processing mode + Linear phase
    juce::ComboBox oversamplingSelector;
    juce::ComboBox procModeSelector;
    juce::ToggleButton linearPhaseButton;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> oversamplingAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> procModeAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> linearPhaseAtt;

    // Undo/Redo
    juce::TextButton undoButton { "Undo" };
    juce::TextButton redoButton { "Redo" };

    // Match EQ
    juce::TextButton matchCaptureButton { "Capture" };
    juce::TextButton matchApplyButton { "Match" };
    juce::TextButton matchClearButton { "Clear" };

    // Level meter
    LevelMeter levelMeter;

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
