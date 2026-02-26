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
    ResponseCurveComponent responseCurve;

    // ── Band selection ───────────────────────────────────────────
    int selectedBand = 0;
    std::array<juce::TextButton, 8> bandBtns;
    void selectBand(int band);
    void rebindBandControls(int band);

    // ── Selected band controls (single set, rebound per selection) ──
    juce::ToggleButton bandOn, bandSolo, dynOn;
    juce::ComboBox typeBox, slopeBox, channelBox, linkBox;
    juce::Slider freqKnob, gainKnob, qKnob, driveKnob;
    juce::Slider dynThreshKnob, dynRatioKnob, dynAttackKnob, dynReleaseKnob;

    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>  bandOnAtt, bandSoloAtt, dynOnAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> typeAtt, slopeAtt, channelAtt, linkAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>  freqAtt, gainAtt, qAtt, driveAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>  dynThreshAtt, dynRatioAtt, dynAttackAtt, dynReleaseAtt;

    // ── Global controls ──────────────────────────────────────────
    juce::Slider outputGainSlider, scaleSlider;
    juce::ToggleButton adaptiveQBtn, linPhaseBtn;
    juce::ComboBox oversamplingBox, procModeBox;

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>  outputGainAtt, scaleAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>  adaptiveQAtt, linPhaseAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> oversamplingAtt, procModeAtt;

    // ── Toolbar ──────────────────────────────────────────────────
    juce::TextButton undoBtn { "Undo" }, redoBtn { "Redo" };
    juce::TextButton matchCapBtn { "Capture" }, matchAppBtn { "Match" }, matchClrBtn { "Clear" };

    // ── Presets ──────────────────────────────────────────────────
    juce::ComboBox presetBox;
    juce::TextButton saveBtn { "Save" }, delBtn { "Del" };
    void refreshPresetList();
    void onPresetSelected();
    void onSaveClicked();
    void onDeleteClicked();

    // ── Spectrum / Meter ─────────────────────────────────────────
    juce::ToggleButton postEqToggle;
    bool showPostSpectrum = true;
    LevelMeter levelMeter;

    // ── Helpers ──────────────────────────────────────────────────
    void initKnob(juce::Slider& s, juce::Colour c, bool large);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FreeEQ8AudioProcessorEditor)
};
