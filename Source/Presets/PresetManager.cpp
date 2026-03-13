#include "PresetManager.h"

static juce::String bandId(int idx, const char* suffix)
{
    return "b" + juce::String(idx) + "_" + suffix;
}

PresetManager::PresetManager(juce::AudioProcessorValueTreeState& state)
    : apvts(state)
{
    getPresetDirectory().createDirectory();
    ensureFactoryPresets();
}

juce::File PresetManager::getPresetDirectory() const
{
#if JUCE_MAC
    return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
        .getChildFile("Application Support")
        .getChildFile("FreeEQ8")
        .getChildFile("Presets");
#else
    return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
        .getChildFile("FreeEQ8")
        .getChildFile("Presets");
#endif
}

juce::String PresetManager::sanitizeFilename(const juce::String& name)
{
    return name.removeCharacters("\\/:*?\"<>|");
}

bool PresetManager::savePreset(const juce::String& name)
{
    if (name.isEmpty()) return false;

    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    if (!xml) return false;

    auto file = getPresetDirectory().getChildFile(sanitizeFilename(name) + ".xml");
    if (xml->writeTo(file))
    {
        currentPresetName = name;
        return true;
    }
    return false;
}

bool PresetManager::loadPreset(const juce::String& name)
{
    auto file = getPresetDirectory().getChildFile(sanitizeFilename(name) + ".xml");
    if (!file.existsAsFile()) return false;

    auto xml = juce::parseXML(file);
    if (!xml) return false;

    auto tree = juce::ValueTree::fromXml(*xml);
    if (!tree.isValid()) return false;

    apvts.replaceState(tree);
    currentPresetName = name;
    return true;
}

bool PresetManager::deletePreset(const juce::String& name)
{
    auto file = getPresetDirectory().getChildFile(sanitizeFilename(name) + ".xml");
    if (file.existsAsFile())
    {
        file.deleteFile();
        if (currentPresetName == name)
            currentPresetName.clear();
        return true;
    }
    return false;
}

juce::StringArray PresetManager::getPresetList() const
{
    juce::StringArray presets;
    auto dir = getPresetDirectory();
    auto files = dir.findChildFiles(juce::File::findFiles, false, "*.xml");

    for (auto& f : files)
        presets.add(f.getFileNameWithoutExtension());

    presets.sort(true);
    return presets;
}

// Factory presets - create a set of useful starting points
void PresetManager::ensureFactoryPresets()
{
    auto dir = getPresetDirectory();

    struct FactoryPreset
    {
        const char* name;
        struct BandSetting { bool on; int type; float freq; float q; float gain; };
        BandSetting bands[8];
        float outputGain;
        float scale;
    };

    static const FactoryPreset factories[] = {
        { "Default (Flat)",
            { {true,0,80,1,0}, {true,0,250,1,0}, {true,0,500,1,0}, {true,0,1000,1,0},
              {true,0,2000,1,0}, {true,0,4000,1,0}, {true,0,8000,1,0}, {true,0,12000,1,0} },
            0.0f, 1.0f },

        { "Vocal Presence",
            { {true,3,80,0.7f,0}, {true,0,250,2,-3}, {true,0,1000,1,2}, {true,0,3500,1.5f,4},
              {true,0,5000,2,3}, {true,2,10000,1,2}, {false,0,8000,1,0}, {false,0,12000,1,0} },
            0.0f, 1.0f },

        { "Bass Boost",
            { {true,1,100,1,6}, {true,0,200,1.5f,3}, {true,0,400,2,-2}, {false,0,1000,1,0},
              {false,0,2000,1,0}, {false,0,4000,1,0}, {false,0,8000,1,0}, {false,0,12000,1,0} },
            0.0f, 1.0f },

        { "Hi-Fi Smile",
            { {true,1,80,1,4}, {true,0,250,1,-2}, {true,0,500,1,-1}, {true,0,1000,1,0},
              {true,0,2000,1,0}, {true,0,4000,1,2}, {true,2,8000,1,4}, {false,0,12000,1,0} },
            0.0f, 1.0f },

        { "Scooped Mids",
            { {true,1,100,1,3}, {true,0,400,1,-2}, {true,0,800,0.8f,-4}, {true,0,1200,0.8f,-4},
              {true,0,2000,1,-2}, {true,0,4000,1,2}, {true,2,8000,1,3}, {false,0,12000,1,0} },
            0.0f, 1.0f },

        { "Telephone",
            { {true,3,300,1,0}, {true,0,800,1,3}, {true,0,2000,1,4}, {true,4,3500,1,0},
              {false,0,2000,1,0}, {false,0,4000,1,0}, {false,0,8000,1,0}, {false,0,12000,1,0} },
            -3.0f, 1.0f },

        { "Air & Sparkle",
            { {true,3,40,0.7f,0}, {false,0,250,1,0}, {false,0,500,1,0}, {false,0,1000,1,0},
              {true,0,5000,1,2}, {true,0,8000,1.5f,3}, {true,2,12000,1,4}, {true,0,16000,2,2} },
            0.0f, 1.0f },

        { "Kick Drum",
            { {true,3,30,0.7f,0}, {true,0,60,1.5f,4}, {true,0,200,3,-3}, {true,0,3000,2,2},
              {true,4,10000,1,0}, {false,0,4000,1,0}, {false,0,8000,1,0}, {false,0,12000,1,0} },
            0.0f, 1.0f },

        // ── Additional factory presets ────────────────────────────────────
        { "Vocal Clarity",
            { {true,3,100,0.7f,0}, {true,0,300,2,-2}, {true,0,800,1.5f,1}, {true,0,2500,1.5f,3},
              {true,0,5000,2,2}, {true,0,7000,3,-2}, {true,2,12000,1,3}, {false,0,16000,1,0} },
            0.0f, 1.0f },

        { "Kick Punch",
            { {true,3,30,0.7f,0}, {true,0,55,2,5}, {true,0,100,1.5f,-2}, {true,0,400,3,-4},
              {true,0,2500,2,3}, {true,4,8000,0.7f,0}, {false,0,10000,1,0}, {false,0,12000,1,0} },
            0.0f, 1.0f },

        { "Acoustic Guitar Warmth",
            { {true,3,80,0.7f,0}, {true,0,200,1.5f,2}, {true,0,500,2,-2}, {true,0,2000,1,1},
              {true,0,3500,1.5f,2}, {true,2,10000,1,1}, {false,0,12000,1,0}, {false,0,16000,1,0} },
            0.0f, 1.0f },

        { "Bass Guitar DI",
            { {true,3,30,0.7f,0}, {true,0,80,1.5f,4}, {true,0,250,2,-3}, {true,0,700,2.5f,2},
              {true,0,1500,3,-2}, {true,4,6000,0.7f,0}, {false,0,8000,1,0}, {false,0,12000,1,0} },
            0.0f, 1.0f },

        { "Drum Bus Glue",
            { {true,1,60,1,2}, {true,0,200,1,-1}, {true,0,800,0.8f,-2}, {true,0,3000,1,1},
              {true,2,10000,1,3}, {false,0,5000,1,0}, {false,0,8000,1,0}, {false,0,12000,1,0} },
            -1.0f, 1.0f },

        { "De-Harsh",
            { {false,0,200,1,0}, {false,0,500,1,0}, {true,0,2500,3,-3}, {true,0,4000,4,-4},
              {true,0,6000,3,-3}, {true,0,8000,2,-2}, {false,0,10000,1,0}, {false,0,12000,1,0} },
            0.0f, 1.0f },

        { "Broadcast Voice",
            { {true,3,80,0.7f,0}, {true,0,180,2,3}, {true,0,400,2,-2}, {true,0,1200,1,1},
              {true,0,3000,1.5f,3}, {true,0,6000,3,-2}, {true,2,10000,1,2}, {true,4,14000,0.7f,0} },
            0.0f, 1.0f },

        { "Master Gentle Tilt",
            { {true,1,80,0.7f,2}, {true,0,300,0.5f,-1}, {true,0,1000,0.5f,-1}, {true,0,3000,0.5f,0},
              {true,0,6000,0.5f,1}, {true,2,12000,0.7f,2}, {false,0,8000,1,0}, {false,0,16000,1,0} },
            0.0f, 1.0f },
    };

    for (auto& fp : factories)
    {
        auto file = dir.getChildFile(juce::String(fp.name) + ".xml");
        if (file.existsAsFile())
            continue; // Don't overwrite user-modified factory presets

        // Build a ValueTree that matches APVTS state
        auto state = apvts.copyState();

        // Set global params
        {
            auto child = state.getChildWithProperty("id", "output_gain");
            if (child.isValid())
                child.setProperty("value", fp.outputGain, nullptr);
        }
        {
            auto child = state.getChildWithProperty("id", "scale");
            if (child.isValid())
                child.setProperty("value", fp.scale, nullptr);
        }

        // Set band params (including drive/slope/channel/dynamic to defaults)
        for (int i = 1; i <= kNumBands; ++i)
        {
            auto& bs = fp.bands[i - 1];
            auto setParam = [&](const char* suffix, float val)
            {
                auto id = bandId(i, suffix);
                auto child = state.getChildWithProperty("id", juce::String(id));
                if (child.isValid())
                    child.setProperty("value", val, nullptr);
            };

            setParam("on",   bs.on ? 1.0f : 0.0f);
            setParam("type", (float)bs.type);
            setParam("freq", bs.freq);
            setParam("q",    bs.q);
            setParam("gain", bs.gain);

            // Ensure factory presets have clean defaults for all params
            setParam("solo",        0.0f);    // solo off
            setParam("slope",       0.0f);    // 12 dB/oct
            setParam("ch",          0.0f);    // Both channels
            setParam("link",        0.0f);    // No link group
            setParam("drive",       0.0f);    // No drive
            setParam("dyn_on",      0.0f);    // Dynamic EQ off
            setParam("dyn_thresh", -20.0f);   // Default threshold
            setParam("dyn_ratio",   4.0f);    // Default ratio
            setParam("dyn_attack", 10.0f);    // Default attack
            setParam("dyn_release",100.0f);   // Default release
        }

        std::unique_ptr<juce::XmlElement> xml(state.createXml());
        if (xml) xml->writeTo(file);
    }
}
