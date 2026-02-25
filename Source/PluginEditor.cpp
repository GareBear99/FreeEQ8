#include "PluginEditor.h"

static juce::String bandId(int idx, const char* suffix)
{
    return "b" + juce::String(idx) + "_" + suffix;
}

FreeEQ8AudioProcessorEditor::FreeEQ8AudioProcessorEditor(FreeEQ8AudioProcessor& p)
: juce::AudioProcessorEditor(&p), proc(p), responseCurve(p),
  levelMeter(p.meterPeakL, p.meterPeakR, p.meterRmsL, p.meterRmsR)
{
    setSize(900, 620);
    setResizable(true, true);
    setResizeLimits(700, 500, 1400, 900);

    // Response curve
    addAndMakeVisible(responseCurve);

    auto typeChoices    = juce::StringArray { "Bell", "LowShelf", "HighShelf", "HighPass", "LowPass" };
    auto slopeChoices   = juce::StringArray { "12 dB", "24 dB", "48 dB" };
    auto channelChoices = juce::StringArray { "Both", "L / Mid", "R / Side" };

    auto initLabel = [&](juce::Label& label, const juce::String& text)
    {
        label.setText(text, juce::dontSendNotification);
        label.setJustificationType(juce::Justification::centred);
        label.setFont(juce::Font(10.0f));
        label.setColour(juce::Label::textColourId, juce::Colours::white.withAlpha(0.5f));
        addAndMakeVisible(label);
    };

    for (int i = 1; i <= 8; ++i)
    {
        auto& b = ui[(size_t)i - 1];

        // On button with band color
        b.on.setButtonText(juce::String(i));
        b.on.setColour(juce::ToggleButton::tickColourId, ResponseCurveComponent::getBandColour(i - 1));
        addAndMakeVisible(b.on);

        // Solo button
        b.solo.setButtonText("S");
        b.solo.setColour(juce::ToggleButton::tickColourId, juce::Colour(0xFFFFD54F));
        addAndMakeVisible(b.solo);

        b.type.addItemList(typeChoices, 1);
        addAndMakeVisible(b.type);

        b.slope.addItemList(slopeChoices, 1);
        addAndMakeVisible(b.slope);

        b.channel.addItemList(channelChoices, 1);
        addAndMakeVisible(b.channel);

        auto initSlider = [&](juce::Slider& s, const juce::String& name)
        {
            s.setName(name);
            s.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
            s.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 16);
            s.setColour(juce::Slider::rotarySliderFillColourId, ResponseCurveComponent::getBandColour(i - 1));
            addAndMakeVisible(s);
        };

        initSlider(b.freq, "Freq");
        initSlider(b.q, "Q");
        initSlider(b.gain, "Gain");

        initLabel(b.freqLabel, "Freq");
        initLabel(b.qLabel, "Q");
        initLabel(b.gainLabel, "Gain");

        b.onAtt      = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(proc.apvts, bandId(i,"on"), b.on);
        b.soloAtt    = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(proc.apvts, bandId(i,"solo"), b.solo);
        b.typeAtt    = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(proc.apvts, bandId(i,"type"), b.type);
        b.slopeAtt   = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(proc.apvts, bandId(i,"slope"), b.slope);
        b.channelAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(proc.apvts, bandId(i,"ch"), b.channel);
        b.freqAtt    = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(proc.apvts, bandId(i,"freq"), b.freq);
        b.qAtt       = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(proc.apvts, bandId(i,"q"), b.q);
        b.gainAtt    = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(proc.apvts, bandId(i,"gain"), b.gain);
    }
    
    // Global controls
    auto initGlobalSlider = [&](juce::Slider& s, const juce::String& name)
    {
        s.setName(name);
        s.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        s.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 16);
        s.setColour(juce::Slider::rotarySliderFillColourId, juce::Colour(0xFF90CAF9));
        addAndMakeVisible(s);
    };

    initGlobalSlider(outputGainSlider, "Output");
    initGlobalSlider(scaleSlider, "Scale");

    initLabel(outputGainLabel, "Output");
    initLabel(scaleLabel, "Scale");
    
    adaptiveQButton.setButtonText("Adaptive Q");
    addAndMakeVisible(adaptiveQButton);
    
    outputGainAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(proc.apvts, "output_gain", outputGainSlider);
    scaleAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(proc.apvts, "scale", scaleSlider);
    adaptiveQAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(proc.apvts, "adaptive_q", adaptiveQButton);

    // Oversampling selector
    oversamplingSelector.addItemList({ "1x", "2x", "4x", "8x" }, 1);
    addAndMakeVisible(oversamplingSelector);
    oversamplingAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(proc.apvts, "oversampling", oversamplingSelector);

    // Processing mode selector
    procModeSelector.addItemList({ "Stereo", "Mid-Side" }, 1);
    addAndMakeVisible(procModeSelector);
    procModeAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(proc.apvts, "proc_mode", procModeSelector);

    // Level meter
    addAndMakeVisible(levelMeter);

    // Preset controls
    addAndMakeVisible(presetSelector);
    presetSelector.setTextWhenNothingSelected("-- Presets --");
    presetSelector.onChange = [this] { onPresetSelected(); };

    saveButton.onClick = [this] { onSaveClicked(); };
    addAndMakeVisible(saveButton);

    deleteButton.onClick = [this] { onDeleteClicked(); };
    addAndMakeVisible(deleteButton);

    refreshPresetList();

    // Spectrum pre/post toggle
    spectrumPostToggle.setButtonText("Post EQ");
    spectrumPostToggle.setToggleState(true, juce::dontSendNotification);
    spectrumPostToggle.onClick = [this] { showPostSpectrum = spectrumPostToggle.getToggleState(); };
    addAndMakeVisible(spectrumPostToggle);

    // Timer for spectrum updates
    startTimerHz(30);
}

void FreeEQ8AudioProcessorEditor::timerCallback()
{
    // Process FFT and push to response curve
    auto& fifo = showPostSpectrum ? proc.spectrumFifo : proc.preSpectrumFifo;
    if (fifo.processIfReady())
    {
        responseCurve.pushSpectrumData(
            fifo.getMagnitudes(),
            fifo.getNumBins(),
            proc.getSampleRate() > 0 ? proc.getSampleRate() : 44100.0);
    }
}

void FreeEQ8AudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xFF0D0D1A));

    const int titleH = 36;
    const int w = getWidth();

    // Title bar background
    g.setColour(juce::Colour(0xFF16213E));
    g.fillRect(0, 0, w, titleH);

    g.setColour(juce::Colours::white.withAlpha(0.9f));
    g.setFont(16.0f);
    g.drawText("FreeEQ8", 12, 8, 120, 20, juce::Justification::left);

    g.setFont(11.0f);
    g.setColour(juce::Colours::white.withAlpha(0.4f));
    g.drawText("8-Band Parametric EQ", 90, 8, 200, 20, juce::Justification::left);

    // Band control section separator
    const int curveH = std::max(140, (int)(getHeight() * 0.35f));
    const int controlsTop = titleH + curveH + 4;
    g.setColour(juce::Colours::white.withAlpha(0.1f));
    g.drawHorizontalLine(controlsTop - 1, 0.0f, (float)w);

    // Band number labels at top of each column
    g.setFont(11.0f);
    const int globalW = 130;
    const int meterW = 36;
    const int colW = (w - globalW - meterW) / 8;
    for (int i = 0; i < 8; ++i)
    {
        g.setColour(ResponseCurveComponent::getBandColour(i).withAlpha(0.7f));
        const int x = i * colW + 6;
        g.drawText("Band " + juce::String(i + 1), x, controlsTop + 2, colW - 12, 14, juce::Justification::centred);
    }
}

void FreeEQ8AudioProcessorEditor::resized()
{
    auto bounds = getLocalBounds();
    const int w = bounds.getWidth();
    const int h = bounds.getHeight();

    const int titleH = 36;
    const int curveH = std::max(140, (int)(h * 0.35f));
    const int globalW = 130;
    const int meterW = 36;
    const int pad = 4;

    // Response curve
    responseCurve.setBounds(0, titleH, w, curveH);

    // Spectrum toggle in title bar
    spectrumPostToggle.setBounds(w - 100, 8, 90, 20);

    // Preset controls in title bar
    presetSelector.setBounds(w - 400, 6, 130, 24);
    saveButton.setBounds(w - 260, 6, 45, 24);
    deleteButton.setBounds(w - 210, 6, 35, 24);

    // Band controls section
    const int controlsTop = titleH + curveH + 18;
    const int controlsH = h - controlsTop;
    const int colW = (w - globalW - meterW) / 8;

    for (int i = 0; i < 8; ++i)
    {
        auto& b = ui[(size_t)i];

        const int x = i * colW + pad;
        const int cw = colW - pad * 2;

        int y = controlsTop;

        // On + Solo buttons side by side
        const int btnW = (cw - 2) / 2;
        b.on.setBounds(x, y, btnW, 22);
        b.solo.setBounds(x + btnW + 2, y, btnW, 22);
        y += 24;

        // Filter type
        b.type.setBounds(x, y, cw, 20);
        y += 22;

        // Slope + Channel side by side
        const int halfW = (cw - 2) / 2;
        b.slope.setBounds(x, y, halfW, 20);
        b.channel.setBounds(x + halfW + 2, y, halfW, 20);
        y += 24;

        // Distribute knobs in remaining space
        const int remainH = h - y - 4;
        const int knobH = std::max(50, remainH / 3 - 12);

        b.freqLabel.setBounds(x, y, cw, 12);
        b.freq.setBounds(x, y + 10, cw, knobH);
        y += knobH + 12;

        b.qLabel.setBounds(x, y, cw, 12);
        b.q.setBounds(x, y + 10, cw, knobH);
        y += knobH + 12;

        b.gainLabel.setBounds(x, y, cw, 12);
        b.gain.setBounds(x, y + 10, cw, knobH);
    }

    // Global controls on right side
    const int globalX = 8 * colW + pad * 2;
    int gy = controlsTop;

    outputGainLabel.setBounds(globalX, gy, globalW - meterW, 14);
    outputGainSlider.setBounds(globalX, gy + 12, globalW - meterW, 64);
    gy += 78;

    scaleLabel.setBounds(globalX, gy, globalW - meterW, 14);
    scaleSlider.setBounds(globalX, gy + 12, globalW - meterW, 64);
    gy += 78;

    adaptiveQButton.setBounds(globalX, gy, globalW - meterW, 20);
    gy += 24;

    oversamplingSelector.setBounds(globalX, gy, globalW - meterW, 20);
    gy += 24;

    procModeSelector.setBounds(globalX, gy, globalW - meterW, 20);

    // Level meter on far right
    levelMeter.setBounds(w - meterW, controlsTop, meterW - 4, controlsH - 8);
}

// ── Preset management ──────────────────────────────────────────────
void FreeEQ8AudioProcessorEditor::refreshPresetList()
{
    presetSelector.clear(juce::dontSendNotification);

    if (proc.presetManager)
    {
        auto presets = proc.presetManager->getPresetList();
        for (int i = 0; i < presets.size(); ++i)
            presetSelector.addItem(presets[i], i + 1);

        // Select current preset if any
        auto current = proc.presetManager->getCurrentPreset();
        if (current.isNotEmpty())
        {
            int idx = presets.indexOf(current);
            if (idx >= 0)
                presetSelector.setSelectedId(idx + 1, juce::dontSendNotification);
        }
    }
}

void FreeEQ8AudioProcessorEditor::onPresetSelected()
{
    const auto name = presetSelector.getText();
    if (name.isNotEmpty() && proc.presetManager)
        proc.presetManager->loadPreset(name);
}

void FreeEQ8AudioProcessorEditor::onSaveClicked()
{
    auto name = proc.presetManager ? proc.presetManager->getCurrentPreset() : juce::String();

    auto* editor = new juce::AlertWindow("Save Preset", "Enter preset name:", juce::AlertWindow::NoIcon);
    editor->addTextEditor("name", name, "Preset Name:");
    editor->addButton("Save", 1);
    editor->addButton("Cancel", 0);

    editor->enterModalState(true, juce::ModalCallbackFunction::create(
        [this, editor](int result)
        {
            if (result == 1)
            {
                auto presetName = editor->getTextEditorContents("name");
                if (presetName.isNotEmpty() && proc.presetManager)
                {
                    proc.presetManager->savePreset(presetName);
                    refreshPresetList();
                }
            }
            delete editor;
        }), true);
}

void FreeEQ8AudioProcessorEditor::onDeleteClicked()
{
    const auto name = presetSelector.getText();
    if (name.isNotEmpty() && proc.presetManager)
    {
        proc.presetManager->deletePreset(name);
        refreshPresetList();
    }
}
