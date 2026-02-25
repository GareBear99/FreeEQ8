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
    presetManager = std::make_unique<PresetManager>(apvts);
}

juce::AudioProcessorValueTreeState::ParameterLayout FreeEQ8AudioProcessor::createParams()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;
    params.reserve(8 * 8 + 6);  // 8 per band + 6 globals

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

    // Oversampling: 1x / 2x / 4x / 8x
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        "oversampling", "Oversampling",
        juce::StringArray { "1x", "2x", "4x", "8x" }, 0));

    // Processing mode: Stereo / Mid-Side
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        "proc_mode", "Processing Mode",
        juce::StringArray { "Stereo", "Mid-Side" }, 0));

    auto typeChoices    = juce::StringArray { "Bell", "LowShelf", "HighShelf", "HighPass", "LowPass" };
    auto slopeChoices   = juce::StringArray { "12 dB", "24 dB", "48 dB" };
    auto channelChoices = juce::StringArray { "Both", "L / Mid", "R / Side" };

    for (int i = 1; i <= 8; ++i)
    {
        params.push_back(std::make_unique<juce::AudioParameterBool>(bandId(i,"on"), "Band " + juce::String(i) + " On", true));
        params.push_back(std::make_unique<juce::AudioParameterBool>(bandId(i,"solo"), "Band " + juce::String(i) + " Solo", false));
        params.push_back(std::make_unique<juce::AudioParameterChoice>(bandId(i,"type"), "Band " + juce::String(i) + " Type", typeChoices, 0));
        params.push_back(std::make_unique<juce::AudioParameterChoice>(bandId(i,"slope"), "Band " + juce::String(i) + " Slope", slopeChoices, 0));
        params.push_back(std::make_unique<juce::AudioParameterChoice>(bandId(i,"ch"), "Band " + juce::String(i) + " Channel", channelChoices, 0));

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

    // Initialise oversampler
    const int osOrder = (int) apvts.getRawParameterValue("oversampling")->load();
    rebuildOversampler(osOrder, sampleRate, samplesPerBlock);

    // Prime coefficients from current params
    syncBandsFromParams();
}

void FreeEQ8AudioProcessor::rebuildOversampler(int order, double sampleRate, int samplesPerBlock)
{
    currentOversamplingOrder = order;
    if (order == 0)
    {
        oversampler.reset();
        return;
    }

    oversampler = std::make_unique<juce::dsp::Oversampling<float>>(
        2, order,
        juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR,
        true);
    oversampler->initProcessing((size_t) samplesPerBlock);
}

void FreeEQ8AudioProcessor::syncBandsFromParams()
{
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
    const bool adaptiveQ = apvts.getRawParameterValue("adaptive_q")->load() > 0.5f;
    const int procMode = (int) apvts.getRawParameterValue("proc_mode")->load();
    const bool midSideMode = (procMode == 1);

    // Check oversampling parameter — rebuild if changed
    const int osOrder = (int) apvts.getRawParameterValue("oversampling")->load();
    if (osOrder != currentOversamplingOrder)
        rebuildOversampler(osOrder, sr, buffer.getNumSamples());

    // Check if any band is soloed
    int soloedBand = -1;
    for (int i = 1; i <= 8; ++i)
    {
        if (apvts.getRawParameterValue(bandId(i, "solo"))->load() > 0.5f)
        {
            soloedBand = i - 1;
            break;
        }
    }

    // Read per-band slope and channel route, then set up beginBlock
    const double effectiveSR = (oversampler != nullptr)
        ? sr * std::pow(2.0, currentOversamplingOrder)
        : sr;

    for (int i = 0; i < 8; ++i)
    {
        auto& b = bands[(size_t)i];

        bool effectiveEnabled = b.enabled;
        if (soloedBand >= 0 && i != soloedBand)
            effectiveEnabled = false;

        float scaledGain = b.targetGainDb * scale;

        float effectiveQ = b.targetQ;
        if (adaptiveQ)
        {
            const float adaptiveFactor = 0.12f;
            effectiveQ = b.targetQ * (1.0f + std::abs(scaledGain) * adaptiveFactor);
            effectiveQ = std::clamp(effectiveQ, 0.1f, 24.0f);
        }

        // Slope: 0 = 12 dB (1 stage), 1 = 24 dB (2 stages), 2 = 48 dB (4 stages)
        const int slopeIdx = (int) apvts.getRawParameterValue(bandId(i + 1, "slope"))->load();
        static const int slopeToStages[] = { 1, 2, 4 };
        const int numStages = slopeToStages[std::clamp(slopeIdx, 0, 2)];

        // Channel routing
        const int chIdx = (int) apvts.getRawParameterValue(bandId(i + 1, "ch"))->load();
        const ChannelRoute route = static_cast<ChannelRoute>(std::clamp(chIdx, 0, 2));

        b.beginBlock(effectiveSR, effectiveEnabled, b.type, b.targetFreqHz, effectiveQ, scaledGain,
                     numStages, route);
    }

    auto* L = buffer.getWritePointer(0);
    auto* R = buffer.getWritePointer(1);
    const int n = buffer.getNumSamples();

    // Push pre-EQ samples to spectrum FIFO
    preSpectrumFifo.pushBlock(L, R, n);

    // --- Oversampled EQ processing ---
    auto processEQ = [&](float* left, float* right, int numSamples, double processSR)
    {
        // Mid/Side encode
        if (midSideMode)
        {
            for (int i = 0; i < numSamples; ++i)
            {
                const float l = left[i];
                const float r = right[i];
                left[i]  = (l + r) * 0.5f;  // Mid
                right[i] = (l - r) * 0.5f;  // Side
            }
        }

        for (int i = 0; i < numSamples; ++i)
        {
            float l = left[i];
            float r = right[i];

            for (auto& b : bands)
            {
                b.maybeUpdateCoeffs(processSR);
                b.process(l, r);
            }

            l *= outputGain;
            r *= outputGain;

            left[i]  = l;
            right[i] = r;
        }

        // Mid/Side decode
        if (midSideMode)
        {
            for (int i = 0; i < numSamples; ++i)
            {
                const float m = left[i];
                const float s = right[i];
                left[i]  = m + s;  // L
                right[i] = m - s;  // R
            }
        }
    };

    if (oversampler != nullptr)
    {
        // Create a dsp::AudioBlock from the buffer
        juce::dsp::AudioBlock<float> block(buffer);
        auto osBlock = oversampler->processSamplesUp(block);

        auto* osL = osBlock.getChannelPointer(0);
        auto* osR = osBlock.getChannelPointer(1);
        const int osN = (int) osBlock.getNumSamples();

        processEQ(osL, osR, osN, effectiveSR);

        oversampler->processSamplesDown(block);

        // Re-fetch pointers after downsample
        L = buffer.getWritePointer(0);
        R = buffer.getWritePointer(1);
    }
    else
    {
        processEQ(L, R, n, sr);
    }

    // Push post-EQ samples to spectrum FIFO
    spectrumFifo.pushBlock(L, R, n);

    // --- Output metering ---
    {
        float peakL = 0.0f, peakR = 0.0f;
        float sumSqL = 0.0f, sumSqR = 0.0f;
        for (int i = 0; i < n; ++i)
        {
            const float al = std::abs(L[i]);
            const float ar = std::abs(R[i]);
            if (al > peakL) peakL = al;
            if (ar > peakR) peakR = ar;
            sumSqL += L[i] * L[i];
            sumSqR += R[i] * R[i];
        }
        meterPeakL.store(peakL, std::memory_order_relaxed);
        meterPeakR.store(peakR, std::memory_order_relaxed);
        meterRmsL.store(std::sqrt(sumSqL / (float) n), std::memory_order_relaxed);
        meterRmsR.store(std::sqrt(sumSqR / (float) n), std::memory_order_relaxed);
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
