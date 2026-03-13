#include "PluginProcessor.h"
#include "PluginEditor.h"

static juce::String bandId(int idx, const char* suffix)
{
    return "b" + juce::String(idx) + "_" + suffix;
}

FreeEQ8AudioProcessor::FreeEQ8AudioProcessor()
: AudioProcessor(BusesProperties().withInput ("Input",  juce::AudioChannelSet::stereo(), true)
                                  .withOutput("Output", juce::AudioChannelSet::stereo(), true))
, apvts(*this, &undoManager, "STATE", createParams())
{
    presetManager = std::make_unique<PresetManager>(apvts);

    // Register for parameter changes to support band linking + latency updates
    for (int i = 1; i <= 8; ++i)
    {
        apvts.addParameterListener(bandId(i, "freq"), this);
        apvts.addParameterListener(bandId(i, "gain"), this);
        apvts.addParameterListener(bandId(i, "q"),    this);
    }
    apvts.addParameterListener("linear_phase", this);

    initLinkTracking();
}

FreeEQ8AudioProcessor::~FreeEQ8AudioProcessor()
{
    apvts.removeParameterListener("linear_phase", this);
    for (int i = 1; i <= 8; ++i)
    {
        apvts.removeParameterListener(bandId(i, "freq"), this);
        apvts.removeParameterListener(bandId(i, "gain"), this);
        apvts.removeParameterListener(bandId(i, "q"),    this);
    }
}

void FreeEQ8AudioProcessor::initLinkTracking()
{
    for (int i = 1; i <= 8; ++i)
    {
        lastLinkedFreq[i - 1] = apvts.getRawParameterValue(bandId(i, "freq"))->load();
        lastLinkedGain[i - 1] = apvts.getRawParameterValue(bandId(i, "gain"))->load();
        lastLinkedQ[i - 1]    = apvts.getRawParameterValue(bandId(i, "q"))->load();
    }
}

void FreeEQ8AudioProcessor::parameterChanged(const juce::String& parameterID, float newValue)
{
    // Handle linear phase latency update (safe: parameterChanged is called on message thread)
    if (parameterID == "linear_phase")
    {
        setLatencySamples(newValue > 0.5f ? LinearPhaseEngine::latency : 0);
        return;
    }

    if (propagatingLink) return;
    if (!parameterID.startsWith("b")) return;

    const int underscoreIdx = parameterID.indexOf("_");
    if (underscoreIdx < 0) return;

    const int bandIdx = parameterID.substring(1, underscoreIdx).getIntValue();
    const auto suffix = parameterID.substring(underscoreIdx + 1);
    if (bandIdx < 1 || bandIdx > 8) return;

    // Check this band's link group (0 = none, 1 = A, 2 = B)
    const int linkGroup = (int)apvts.getRawParameterValue(bandId(bandIdx, "link"))->load();
    if (linkGroup == 0) return;

    const int ai = bandIdx - 1;

    if (suffix == "freq")
    {
        const float oldVal = lastLinkedFreq[ai];
        lastLinkedFreq[ai] = newValue;
        if (oldVal < 1.0f) return;
        const float ratio = newValue / oldVal;

        propagatingLink = true;
        for (int i = 1; i <= 8; ++i)
        {
            if (i == bandIdx) continue;
            if ((int)apvts.getRawParameterValue(bandId(i, "link"))->load() != linkGroup) continue;

            const float other = apvts.getRawParameterValue(bandId(i, "freq"))->load();
            const float updated = std::clamp(other * ratio, 20.0f, 20000.0f);
            lastLinkedFreq[i - 1] = updated;
            if (auto* p = apvts.getParameter(bandId(i, "freq")))
                p->setValueNotifyingHost(p->convertTo0to1(updated));
        }
        propagatingLink = false;
    }
    else if (suffix == "gain")
    {
        const float delta = newValue - lastLinkedGain[ai];
        lastLinkedGain[ai] = newValue;

        propagatingLink = true;
        for (int i = 1; i <= 8; ++i)
        {
            if (i == bandIdx) continue;
            if ((int)apvts.getRawParameterValue(bandId(i, "link"))->load() != linkGroup) continue;

            const float other = apvts.getRawParameterValue(bandId(i, "gain"))->load();
            const float updated = std::clamp(other + delta, -24.0f, 24.0f);
            lastLinkedGain[i - 1] = updated;
            if (auto* p = apvts.getParameter(bandId(i, "gain")))
                p->setValueNotifyingHost(p->convertTo0to1(updated));
        }
        propagatingLink = false;
    }
    else if (suffix == "q")
    {
        const float delta = newValue - lastLinkedQ[ai];
        lastLinkedQ[ai] = newValue;

        propagatingLink = true;
        for (int i = 1; i <= 8; ++i)
        {
            if (i == bandIdx) continue;
            if ((int)apvts.getRawParameterValue(bandId(i, "link"))->load() != linkGroup) continue;

            const float other = apvts.getRawParameterValue(bandId(i, "q"))->load();
            const float updated = std::clamp(other + delta, 0.1f, 24.0f);
            lastLinkedQ[i - 1] = updated;
            if (auto* p = apvts.getParameter(bandId(i, "q")))
                p->setValueNotifyingHost(p->convertTo0to1(updated));
        }
        propagatingLink = false;
    }
}

juce::AudioProcessorValueTreeState::ParameterLayout FreeEQ8AudioProcessor::createParams()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;
    params.reserve(8 * 14 + 8);  // 14 per band + 8 globals

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

    // Linear phase mode
    params.push_back(std::make_unique<juce::AudioParameterBool>(
        "linear_phase", "Linear Phase", false));

    auto typeChoices    = juce::StringArray { "Bell", "LowShelf", "HighShelf", "HighPass", "LowPass", "Bandpass" };
    auto slopeChoices   = juce::StringArray { "12 dB", "24 dB", "48 dB" };
    auto channelChoices = juce::StringArray { "Both", "L / Mid", "R / Side" };
    auto linkChoices    = juce::StringArray { "--", "A", "B" };

    for (int i = 1; i <= 8; ++i)
    {
        params.push_back(std::make_unique<juce::AudioParameterBool>(bandId(i,"on"), "Band " + juce::String(i) + " On", true));
        params.push_back(std::make_unique<juce::AudioParameterBool>(bandId(i,"solo"), "Band " + juce::String(i) + " Solo", false));
        params.push_back(std::make_unique<juce::AudioParameterChoice>(bandId(i,"type"), "Band " + juce::String(i) + " Type", typeChoices, 0));
        params.push_back(std::make_unique<juce::AudioParameterChoice>(bandId(i,"slope"), "Band " + juce::String(i) + " Slope", slopeChoices, 0));
        params.push_back(std::make_unique<juce::AudioParameterChoice>(bandId(i,"ch"), "Band " + juce::String(i) + " Channel", channelChoices, 0));
        params.push_back(std::make_unique<juce::AudioParameterChoice>(bandId(i,"link"), "Band " + juce::String(i) + " Link", linkChoices, 0));

        // Default frequencies spread logarithmically across the spectrum
        static const float defaultFreqs[] = { 0.f, 80.f, 250.f, 500.f, 1000.f, 2000.f, 4000.f, 8000.f, 12000.f };
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            bandId(i,"freq"), "Band " + juce::String(i) + " Freq",
            juce::NormalisableRange<float>(20.0f, 20000.0f, 0.001f, 0.5f),
            defaultFreqs[i]));

        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            bandId(i,"q"), "Band " + juce::String(i) + " Q",
            juce::NormalisableRange<float>(0.1f, 24.0f, 0.001f, 0.5f),
            1.0f));

        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            bandId(i,"gain"), "Band " + juce::String(i) + " Gain",
            juce::NormalisableRange<float>(-24.0f, 24.0f, 0.01f, 1.0f),
            0.0f));

        // Drive / saturation per band
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            bandId(i,"drive"), "Band " + juce::String(i) + " Drive",
            juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f, 1.0f),
            0.0f));

        // Dynamic EQ per band
        params.push_back(std::make_unique<juce::AudioParameterBool>(bandId(i,"dyn_on"), "Band " + juce::String(i) + " Dyn On", false));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            bandId(i,"dyn_thresh"), "Band " + juce::String(i) + " Threshold",
            juce::NormalisableRange<float>(-60.0f, 0.0f, 0.1f, 1.0f),
            -20.0f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            bandId(i,"dyn_ratio"), "Band " + juce::String(i) + " Ratio",
            juce::NormalisableRange<float>(1.0f, 20.0f, 0.1f, 0.5f),
            4.0f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            bandId(i,"dyn_attack"), "Band " + juce::String(i) + " Attack",
            juce::NormalisableRange<float>(0.1f, 100.0f, 0.1f, 0.5f),
            10.0f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            bandId(i,"dyn_release"), "Band " + juce::String(i) + " Release",
            juce::NormalisableRange<float>(1.0f, 1000.0f, 1.0f, 0.5f),
            100.0f));
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
    maxBlockSize = samplesPerBlock;

    for (auto& b : bands)
        b.reset(sr);

    // Initialise oversampler
    const int osOrder = (int) apvts.getRawParameterValue("oversampling")->load();
    rebuildOversampler(osOrder, sampleRate, samplesPerBlock);

    // Linear phase engine
    linearPhaseEngine.prepare(sampleRate, samplesPerBlock);

    // Update latency based on current linear-phase setting
    const bool linPhase = apvts.getRawParameterValue("linear_phase")->load() > 0.5f;
    setLatencySamples(linPhase ? LinearPhaseEngine::latency : 0);

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
            case 5: tp = Biquad::Type::Bandpass; break;
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

    // Check oversampling parameter — defer rebuild if changed
    // (avoid heap allocation on audio thread; rebuild happens in prepareToPlay or
    //  on next block after the pending flag is set)
    const int osOrder = (int) apvts.getRawParameterValue("oversampling")->load();
    if (osOrder != currentOversamplingOrder)
    {
        // Build the new oversampler here but only if sample rate is known.
        // This is still on the audio thread, but it's safer to do it once
        // (on parameter change) than every block. A fully correct solution
        // would defer to a non-RT thread, but this matches JUCE plugin norms.
        rebuildOversampler(osOrder, sr, buffer.getNumSamples());
    }

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

        // Drive
        const float drive = apvts.getRawParameterValue(bandId(i + 1, "drive"))->load() / 100.0f;

        // Dynamic EQ params
        const bool dynOn     = apvts.getRawParameterValue(bandId(i + 1, "dyn_on"))->load() > 0.5f;
        const float dynThr   = apvts.getRawParameterValue(bandId(i + 1, "dyn_thresh"))->load();
        const float dynRat   = apvts.getRawParameterValue(bandId(i + 1, "dyn_ratio"))->load();
        const float dynAtk   = apvts.getRawParameterValue(bandId(i + 1, "dyn_attack"))->load();
        const float dynRel   = apvts.getRawParameterValue(bandId(i + 1, "dyn_release"))->load();

        b.driveAmount   = drive;
        b.dynEnabled    = dynOn;
        b.dynThreshDb   = dynThr;
        b.dynRatio      = dynRat;
        b.dynAttackMs   = dynAtk;
        b.dynReleaseMs  = dynRel;

        b.beginBlock(effectiveSR, effectiveEnabled, b.type, b.targetFreqHz, effectiveQ, scaledGain,
                     numStages, route);
    }

    auto* L = buffer.getWritePointer(0);
    auto* R = buffer.getWritePointer(1);
    const int n = buffer.getNumSamples();

    // Push pre-EQ samples to spectrum FIFO
    preSpectrumFifo.pushBlock(L, R, n);

    // Linear phase mode
    const bool linearPhase = apvts.getRawParameterValue("linear_phase")->load() > 0.5f;

    // --- Oversampled EQ processing (minimum-phase biquad path) ---
    auto processEQ = [&](float* left, float* right, int numSamples, double processSR)
    {
        // Mid/Side encode
        if (midSideMode)
        {
            for (int i = 0; i < numSamples; ++i)
            {
                const float l = left[i];
                const float r = right[i];
                left[i]  = (l + r) * 0.5f;
                right[i] = (l - r) * 0.5f;
            }
        }

        for (int i = 0; i < numSamples; ++i)
        {
            float l = left[i];
            float r = right[i];

            for (auto& b : bands)
            {
                b.updateDynamicEnvelope(l, r, processSR);
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
                left[i]  = m + s;
                right[i] = m - s;
            }
        }
    };

    if (linearPhase)
    {
        // Linear phase path: build magnitude response, convolve via FIR
        buildLinearPhaseMagnitude();

        // Apply output gain to the buffer before linear phase processing
        // (output gain is baked into the magnitude response)
        linearPhaseEngine.processBlock(L, R, n);

        // Apply output gain separately
        for (int i = 0; i < n; ++i)
        {
            L[i] *= outputGain;
            R[i] *= outputGain;
        }
    }
    else if (oversampler != nullptr)
    {
        juce::dsp::AudioBlock<float> block(buffer);
        auto osBlock = oversampler->processSamplesUp(block);

        auto* osL = osBlock.getChannelPointer(0);
        auto* osR = osBlock.getChannelPointer(1);
        const int osN = (int) osBlock.getNumSamples();

        processEQ(osL, osR, osN, effectiveSR);

        oversampler->processSamplesDown(block);

        L = buffer.getWritePointer(0);
        R = buffer.getWritePointer(1);
    }
    else
    {
        processEQ(L, R, n, sr);
    }

    // Match EQ: capture reference if active, apply correction if matching
    matchEQ.pushSamples(L, R, n);
    if (matchEQ.isMatchActive())
        matchEQ.applyCorrection(L, R, n, sr);

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
    {
        apvts.replaceState(juce::ValueTree::fromXml(*xmlState));

        // Re-sync link tracking from restored state so the first
        // linked-parameter change after loading doesn't use stale baselines.
        initLinkTracking();

        // Update latency to match restored linear-phase setting
        const bool linPhase = apvts.getRawParameterValue("linear_phase")->load() > 0.5f;
        setLatencySamples(linPhase ? LinearPhaseEngine::latency : 0);
    }
}

double FreeEQ8AudioProcessor::getTailLengthSeconds() const
{
    // Linear phase FIR convolution produces a tail equal to the FIR length.
    // Match EQ overlap-add also contributes but its tail is shorter.
    const bool linPhase = apvts.getRawParameterValue("linear_phase")->load() > 0.5f;
    if (linPhase && sr > 0)
        return (double)LinearPhaseEngine::firLength / sr;
    return 0.0;
}

juce::AudioProcessorEditor* FreeEQ8AudioProcessor::createEditor()
{
    return new FreeEQ8AudioProcessorEditor(*this);
}

void FreeEQ8AudioProcessor::buildLinearPhaseMagnitude()
{
    // Build composite magnitude response for the linear phase FIR.
    // Use the same logic as ResponseCurveComponent but at FFT resolution.
    // NOTE: Linear phase mode does not support dynamic EQ, per-band drive,
    // or Mid/Side processing — those features require per-sample biquad state.
    const int numBins = LinearPhaseEngine::firLength / 2 + 1;

    // Use pre-allocated member buffer (avoid heap allocation on audio thread)
    linPhaseMagBuf.resize((size_t)numBins);
    std::fill(linPhaseMagBuf.begin(), linPhaseMagBuf.end(), 0.0f);
    float* magDb = linPhaseMagBuf.data();
    const float scale = apvts.getRawParameterValue("scale")->load();

    for (int b = 0; b < 8; ++b)
    {
        const int idx = b + 1;
        const bool on = apvts.getRawParameterValue(bandId(idx, "on"))->load() > 0.5f;
        if (!on) continue;

        const int t = (int)apvts.getRawParameterValue(bandId(idx, "type"))->load();
        Biquad::Type tp = Biquad::Type::Bell;
        switch (t)
        {
            case 0: tp = Biquad::Type::Bell; break;
            case 1: tp = Biquad::Type::LowShelf; break;
            case 2: tp = Biquad::Type::HighShelf; break;
            case 3: tp = Biquad::Type::HighPass; break;
            case 4: tp = Biquad::Type::LowPass; break;
            case 5: tp = Biquad::Type::Bandpass; break;
        }

        const float freq = apvts.getRawParameterValue(bandId(idx, "freq"))->load();
        float q    = apvts.getRawParameterValue(bandId(idx, "q"))->load();
        const float gain = apvts.getRawParameterValue(bandId(idx, "gain"))->load() * scale;

        // Apply adaptive Q in linear phase magnitude build
        const bool adaptiveQ = apvts.getRawParameterValue("adaptive_q")->load() > 0.5f;
        if (adaptiveQ)
        {
            const float adaptiveFactor = 0.12f;
            q = q * (1.0f + std::abs(gain) * adaptiveFactor);
            q = std::clamp(q, 0.1f, 24.0f);
        }

        const int slopeIdx = (int)apvts.getRawParameterValue(bandId(idx, "slope"))->load();
        static const int slopeToStages[] = { 1, 2, 4 };
        const int numStages = slopeToStages[std::clamp(slopeIdx, 0, 2)];

        Biquad tempBq;
        tempBq.set(tp, sr, freq, q, gain);

        for (int i = 0; i < numBins; ++i)
        {
            const double f = (double)i / (double)(numBins - 1) * sr * 0.5;
            if (f < 1.0) continue;

            const double omega = 2.0 * M_PI * f / sr;
            const double cosw  = std::cos(omega);
            const double cos2w = std::cos(2.0 * omega);
            const double sinw  = std::sin(omega);
            const double sin2w = std::sin(2.0 * omega);

            const double numReal = tempBq.b0 + tempBq.b1 * cosw + tempBq.b2 * cos2w;
            const double numImag = -(tempBq.b1 * sinw + tempBq.b2 * sin2w);
            const double denReal = 1.0 + tempBq.a1 * cosw + tempBq.a2 * cos2w;
            const double denImag = -(tempBq.a1 * sinw + tempBq.a2 * sin2w);

            const double numMagSq = numReal * numReal + numImag * numImag;
            const double denMagSq = denReal * denReal + denImag * denImag;

            if (denMagSq < 1e-30) continue;

            const float singleDb = (float)(10.0 * std::log10(std::max(numMagSq / denMagSq, 1e-30)));
            magDb[(size_t)i] += singleDb * (float)numStages;
        }
    }

    linearPhaseEngine.rebuildFromMagnitude(magDb, numBins);
}

// This creates new instances of the plugin.
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new FreeEQ8AudioProcessor();
}
