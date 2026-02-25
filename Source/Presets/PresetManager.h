#pragma once
#include <juce_audio_processors/juce_audio_processors.h>

class PresetManager
{
public:
    explicit PresetManager(juce::AudioProcessorValueTreeState& apvts);

    // Preset directory
    juce::File getPresetDirectory() const;

    // Save current state as preset
    bool savePreset(const juce::String& name);

    // Load a preset by name
    bool loadPreset(const juce::String& name);

    // Delete a preset by name
    bool deletePreset(const juce::String& name);

    // Get list of all available presets (user + factory)
    juce::StringArray getPresetList() const;

    // Get/set current preset name
    const juce::String& getCurrentPreset() const { return currentPresetName; }
    void setCurrentPreset(const juce::String& name) { currentPresetName = name; }

    // Create factory presets if they don't exist
    void ensureFactoryPresets();

private:
    juce::AudioProcessorValueTreeState& apvts;
    juce::String currentPresetName;

    static juce::String sanitizeFilename(const juce::String& name);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PresetManager)
};
