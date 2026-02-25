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

        // Set band params
        for (int i = 1; i <= 8; ++i)
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
        }

        std::unique_ptr<juce::XmlElement> xml(state.createXml());
        if (xml) xml->writeTo(file);
    }
}
