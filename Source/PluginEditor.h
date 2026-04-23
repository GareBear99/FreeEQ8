#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "Config.h"
#include "PluginProcessor.h"
#include "UI/ResponseCurveComponent.h"
#include "UI/LevelMeter.h"
#include "UpdateChecker.h"

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
    std::array<juce::TextButton, kNumBands> bandBtns;
    void selectBand(int band);
    void rebindBandControls(int band);

    // ── Selected band controls (single set, rebound per selection) ──
    juce::ToggleButton bandOn, bandSolo, dynOn;
    juce::ComboBox typeBox, slopeBox, channelBox, linkBox;
    juce::Slider freqKnob, gainKnob, qKnob, driveKnob;
#if PROEQ8
    juce::ComboBox satModeBox;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> satModeAtt;
#endif
    juce::Slider dynThreshKnob, dynRatioKnob, dynAttackKnob, dynReleaseKnob;

    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>  bandOnAtt, bandSoloAtt, dynOnAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> typeAtt, slopeAtt, channelAtt, linkAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>  freqAtt, gainAtt, qAtt, driveAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>  dynThreshAtt, dynRatioAtt, dynAttackAtt, dynReleaseAtt;

    // ── Global controls ──────────────────────────────────────────
    juce::Slider outputGainSlider, scaleSlider;
    juce::ToggleButton adaptiveQBtn, linPhaseBtn, autoGainBtn;
    juce::ComboBox oversamplingBox, procModeBox;

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>  outputGainAtt, scaleAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>  adaptiveQAtt, linPhaseAtt, autoGainAtt;
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

    // ── A/B comparison (Pro only) ────────────────────────────────
#if PROEQ8
    juce::TextButton abBtn { "A" }, copyABBtn { "A\u2192B" };
    void toggleAB();
    // License activation
    juce::TextButton licenseBtn { "Activate" };
    void showActivationDialog();
#endif

    // ── Update checker ───────────────────────────────────────────
    UpdateChecker updateChecker;
    bool hasUpdate = false;

    // ── Helpers ──────────────────────────────────────────────────
    void initKnob(juce::Slider& s, juce::Colour c, bool large);

    // Tooltip window — auto-shows tooltips for any child component with setTooltip()
    juce::TooltipWindow tooltipWindow { this, 500 };

    // ── A2: lifetime-safe modal dialogs + async callbacks ─────────────
    // activeDialog is non-null while a modal AlertWindow is up; it's cleared
    // from the ModalCallbackFunction. Using a unique_ptr instead of
    // `new ... + delete dlg` eliminates the prior latent double-free risk
    // (the old code called `delete dlg` inside a callback registered with
    // `deleteWhenDismissed = true`).
    std::unique_ptr<juce::AlertWindow> activeDialog;

    // Background jobs (license activate/deactivate/reverify) capture a weak
    // reference to this editor. If the editor is destroyed before the HTTP
    // round-trip finishes, the posted MessageManager::callAsync lambda sees
    // weak.get() == nullptr and does nothing instead of dereferencing a
    // dangling this pointer.
    JUCE_DECLARE_WEAK_REFERENCEABLE (FreeEQ8AudioProcessorEditor)

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FreeEQ8AudioProcessorEditor)
};
