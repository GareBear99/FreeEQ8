#include "PluginProcessor.h"
#include "PluginEditor.h"

static juce::String bandId(int idx, const char* suffix)
{
    return "b" + juce::String(idx) + "_" + suffix;
}

FreeEQ8AudioProcessor::FreeEQ8AudioProcessor()
: AudioProcessor(BusesProperties().withInput ("Input",  juce::AudioChannelSet::stereo(), true)
                                  .withOutput("Output", juce::AudioChannelSet::stereo(), true))
, apvts(*this, nullptr, "STATE", createParams())
{
}

juce::AudioProcessorValueTreeState::ParameterLayout FreeEQ8AudioProcessor::createParams()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;
    params.reserve(8 * 5 + 3); // +3 for output gain, scale, adaptive Q

    // Global parameters
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "output_gain", "Output Gain",
        juce::NormalisableRange<float>(-24.0f, 24.0f, 0.01f, 1.0f),
        0.0f));
    
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "scale", "Scale",
        juce::NormalisableRange<float>(0.1f, 2.0f, 0.01f, 1.0f),
        1.0f));
    
    params.push_back(std::make_unique<juce::AudioParameterBool>(
        "adaptive_q", "Adaptive Q", false));

    auto typeChoices = juce::StringArray { "Bell", "LowShelf", "HighShelf", "HighPass", "LowPass" };

    for (int i = 1; i <= 8; ++i)
    {
        params.push_back(std::make_unique<juce::AudioParameterBool>(bandId(i,"on"), "Band " + juce::String(i) + " On", true));
        params.push_back(std::make_unique<juce::AudioParameterChoice>(bandId(i,"type"), "Band " + juce::String(i) + " Type", typeChoices, 0));

        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            bandId(i,"freq"), "Band " + juce::String(i) + " Freq",
            juce::NormalisableRange<float>(20.0f, 20000.0f, 0.001f, 0.5f),
            (i == 1 ? 80.0f : i == 8 ? 12000.0f : 1000.0f)));

        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            bandId(i,"q"), "Band " + juce::String(i) + " Q",
            juce::NormalisableRange<float>(0.1f, 24.0f, 0.001f, 0.5f),
            1.0f));

        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            bandId(i,"gain"), "Band " + juce::String(i) + " Gain",
            juce::NormalisableRange<float>(-24.0f, 24.0f, 0.01f, 1.0f),
            0.0f));
    }

    return { params.begin(), params.end() };
}

bool FreeEQ8AudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
    return true;
}

void FreeEQ8AudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    sr = sampleRate;

    for (auto& b : bands)
        b.reset(sr);

    // Prime coefficients from current params
    syncBandsFromParams();
}

void FreeEQ8AudioProcessor::syncBandsFromParams()
{
    // Cache targets from APVTS. Coeff update/smoothing happens in beginBlock().
    for (int i = 1; i <= 8; ++i)
    {
        auto& b = bands[(size_t)i - 1];

        const bool on = apvts.getRawParameterValue(bandId(i,"on"))->load() > 0.5f;

        const int t = (int) apvts.getRawParameterValue(bandId(i,"type"))->load();
        Biquad::Type tp = Biquad::Type::Bell;
        switch (t)
        {
            case 0: tp = Biquad::Type::Bell; break;
            case 1: tp = Biquad::Type::LowShelf; break;
            case 2: tp = Biquad::Type::HighShelf; break;
            case 3: tp = Biquad::Type::HighPass; break;
            case 4: tp = Biquad::Type::LowPass; break;
            default: tp = Biquad::Type::Bell; break;
        }

        const float freq = apvts.getRawParameterValue(bandId(i,"freq"))->load();
        const float q    = apvts.getRawParameterValue(bandId(i,"q"))->load();
        const float gain = apvts.getRawParameterValue(bandId(i,"gain"))->load();

        // Store targets into band; beginBlock will apply smoothing + coeff refresh.
        b.enabled = on;
        b.type = tp;
        b.targetFreqHz = freq;
        b.targetQ = q;
        b.targetGainDb = gain;
    }
}

void FreeEQ8AudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    if (buffer.getNumChannels() < 2)
        return;

    // Pull params each block
    syncBandsFromParams();
    
    // Get global parameters
    const float scale = apvts.getRawParameterValue("scale")->load();
    const float outputGainDb = apvts.getRawParameterValue("output_gain")->load();
    const float outputGain = std::pow(10.0f, outputGainDb / 20.0f);

    // Apply per-band beginBlock (smoothing setup + initial coeff update)
    for (auto& b : bands)
    {
        // Apply scale to gain
        float scaledGain = b.targetGainDb * scale;
        b.beginBlock(sr, b.enabled, b.type, b.targetFreqHz, b.targetQ, scaledGain);
    }

    auto* L = buffer.getWritePointer(0);
    auto* R = buffer.getWritePointer(1);
    const int n = buffer.getNumSamples();

    for (int i = 0; i < n; ++i)
    {
        float l = L[i];
        float r = R[i];

        for (auto& b : bands)
        {
            b.maybeUpdateCoeffs(sr);
            b.process(l, r);
        }

        // Apply output gain
        l *= outputGain;
        r *= outputGain;

        L[i] = l;
        R[i] = r;
    }
}


void FreeEQ8AudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary(*xml, destData);
}

void FreeEQ8AudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState (getXmlFromBinary(data, sizeInBytes));
    if (xmlState && xmlState->hasTagName(apvts.state.getType()))
        apvts.replaceState(juce::ValueTree::fromXml(*xmlState));
}

juce::AudioProcessorEditor* FreeEQ8AudioProcessor::createEditor()
{
    return new FreeEQ8AudioProcessorEditor(*this);
}

// This creates new instances of the plugin.
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new FreeEQ8AudioProcessor();
}
