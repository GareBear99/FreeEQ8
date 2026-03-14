#include "PluginEditor.h"

static juce::String bandId(int idx, const char* suffix)
{
    return "b" + juce::String(idx) + "_" + suffix;
}

// ── Knob initializer ───────────────────────────────────────────────
void FreeEQ8AudioProcessorEditor::initKnob(juce::Slider& s, juce::Colour c, bool large)
{
    s.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    s.setTextBoxStyle(juce::Slider::TextBoxBelow, false, large ? 70 : 52, 14);
    s.setColour(juce::Slider::rotarySliderFillColourId, c);
    s.setColour(juce::Slider::textBoxTextColourId, juce::Colours::white.withAlpha(0.85f));
    s.setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    addAndMakeVisible(s);
}

// ── Dark-style combo box helper ────────────────────────────────────
static void styleCombo(juce::ComboBox& cb)
{
    cb.setColour(juce::ComboBox::backgroundColourId,  juce::Colour(0xFF1C2040));
    cb.setColour(juce::ComboBox::outlineColourId,     juce::Colours::white.withAlpha(0.15f));
    cb.setColour(juce::ComboBox::textColourId,        juce::Colours::white.withAlpha(0.9f));
    cb.setColour(juce::ComboBox::arrowColourId,       juce::Colours::white.withAlpha(0.5f));
}

// ── Constructor ────────────────────────────────────────────────────
FreeEQ8AudioProcessorEditor::FreeEQ8AudioProcessorEditor(FreeEQ8AudioProcessor& p)
    : juce::AudioProcessorEditor(&p), proc(p), responseCurve(p),
      levelMeter(p.meterPeakL, p.meterPeakR, p.meterRmsL, p.meterRmsR)
{
    setSize(900, 620);
    setResizable(true, true);
    setResizeLimits(750, 550, 1400, 900);

    addAndMakeVisible(responseCurve);

    // ── Band selector buttons ──
    for (int i = 0; i < kNumBands; ++i)
    {
        auto& btn = bandBtns[(size_t)i];
        btn.setButtonText(juce::String(i + 1));
        btn.setClickingTogglesState(false);
        btn.setColour(juce::TextButton::buttonColourId,
                      ResponseCurveComponent::getBandColour(i).withAlpha(0.25f));
        btn.setColour(juce::TextButton::buttonOnColourId,
                      ResponseCurveComponent::getBandColour(i));
        btn.setColour(juce::TextButton::textColourOnId, juce::Colours::white);
        btn.setColour(juce::TextButton::textColourOffId, juce::Colours::white.withAlpha(0.7f));
        btn.onClick = [this, i] { selectBand(i); };
        addAndMakeVisible(btn);
    }

    // ── Selected band controls ──
    bandOn.setButtonText("On");
    bandOn.setColour(juce::ToggleButton::tickColourId, juce::Colour(0xFF8BC34A));
    addAndMakeVisible(bandOn);

    bandSolo.setButtonText("Solo");
    bandSolo.setColour(juce::ToggleButton::tickColourId, juce::Colour(0xFFFFD54F));
    addAndMakeVisible(bandSolo);

    typeBox.addItemList({ "Bell", "LowShelf", "HighShelf", "HighPass", "LowPass", "Bandpass" }, 1);
    styleCombo(typeBox);
    addAndMakeVisible(typeBox);

    slopeBox.addItemList({ "12 dB", "24 dB", "48 dB" }, 1);
    styleCombo(slopeBox);
    addAndMakeVisible(slopeBox);

    channelBox.addItemList({ "Both", "L / Mid", "R / Side" }, 1);
    styleCombo(channelBox);
    addAndMakeVisible(channelBox);

    linkBox.addItemList({ "--", "A", "B" }, 1);
    styleCombo(linkBox);
    addAndMakeVisible(linkBox);

    auto bandCol = ResponseCurveComponent::getBandColour(0);
    initKnob(freqKnob,  bandCol, true);
    initKnob(gainKnob,  bandCol, true);
    initKnob(qKnob,     bandCol, true);
    initKnob(driveKnob, bandCol, false);

#if PROEQ8
    satModeBox.addItemList({ "Tanh", "Tube", "Tape", "Transistor" }, 1);
    styleCombo(satModeBox);
    satModeBox.setTooltip("Saturation mode (Tanh/Tube/Tape/Transistor)");
    addAndMakeVisible(satModeBox);
#endif

    // Dynamic EQ
    dynOn.setButtonText("Dyn");
    dynOn.setColour(juce::ToggleButton::tickColourId, juce::Colour(0xFFFF5722));
    addAndMakeVisible(dynOn);

    auto dynCol = juce::Colour(0xFFFF5722);
    initKnob(dynThreshKnob,  dynCol, false);
    initKnob(dynRatioKnob,   dynCol, false);
    initKnob(dynAttackKnob,  dynCol, false);
    initKnob(dynReleaseKnob, dynCol, false);

    // ── Global controls ──
    auto globalCol = juce::Colour(0xFF90CAF9);
    initKnob(outputGainSlider, globalCol, true);
    initKnob(scaleSlider,      globalCol, false);

    adaptiveQBtn.setButtonText("Adaptive Q");
    addAndMakeVisible(adaptiveQBtn);
    linPhaseBtn.setButtonText("Lin Phase");
    addAndMakeVisible(linPhaseBtn);
    autoGainBtn.setButtonText("Auto Gain");
    autoGainBtn.setTooltip("Loudness-compensated bypass (match output RMS to input)");
    addAndMakeVisible(autoGainBtn);

    oversamplingBox.addItemList({ "1x", "2x", "4x", "8x" }, 1);
    styleCombo(oversamplingBox);
    addAndMakeVisible(oversamplingBox);
    procModeBox.addItemList({ "Stereo", "Mid-Side" }, 1);
    styleCombo(procModeBox);
    addAndMakeVisible(procModeBox);

    // Global attachments (permanent)
    outputGainAtt  = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(proc.apvts, "output_gain", outputGainSlider);
    scaleAtt       = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(proc.apvts, "scale", scaleSlider);
    adaptiveQAtt   = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(proc.apvts, "adaptive_q", adaptiveQBtn);
    linPhaseAtt    = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(proc.apvts, "linear_phase", linPhaseBtn);
    autoGainAtt    = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(proc.apvts, "auto_gain", autoGainBtn);
    oversamplingAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(proc.apvts, "oversampling", oversamplingBox);
    procModeAtt    = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(proc.apvts, "proc_mode", procModeBox);

    // ── Toolbar buttons ──
    undoBtn.onClick = [this] { proc.undoManager.undo(); };
    redoBtn.onClick = [this] { proc.undoManager.redo(); };
    addAndMakeVisible(undoBtn);
    addAndMakeVisible(redoBtn);

    matchCapBtn.onClick = [this] {
        if (proc.matchEQ.isCapturing()) proc.matchEQ.stopCapture();
        else proc.matchEQ.startCapture();
    };
    matchAppBtn.onClick = [this] { proc.matchEQ.setMatchActive(!proc.matchEQ.isMatchActive()); };
    matchClrBtn.onClick = [this] { proc.matchEQ.clear(); };
    addAndMakeVisible(matchCapBtn);
    addAndMakeVisible(matchAppBtn);
    addAndMakeVisible(matchClrBtn);

    addAndMakeVisible(levelMeter);

    // ── Presets ──
    styleCombo(presetBox);
    addAndMakeVisible(presetBox);
    presetBox.setTextWhenNothingSelected("-- Presets --");
    presetBox.onChange = [this] { onPresetSelected(); };
    saveBtn.onClick = [this] { onSaveClicked(); };
    delBtn.onClick  = [this] { onDeleteClicked(); };
    addAndMakeVisible(saveBtn);
    addAndMakeVisible(delBtn);
    refreshPresetList();

    // ── Spectrum toggle ──
    postEqToggle.setButtonText("Post EQ");
    postEqToggle.setToggleState(true, juce::dontSendNotification);
    postEqToggle.onClick = [this] { showPostSpectrum = postEqToggle.getToggleState(); };
    addAndMakeVisible(postEqToggle);

    // ── Tooltips ──
    freqKnob.setTooltip("Band frequency (20 Hz - 20 kHz)");
    gainKnob.setTooltip("Boost/cut (-24 dB to +24 dB)");
    qKnob.setTooltip("Bandwidth (0.1 = wide, 24 = narrow)");
    driveKnob.setTooltip("Per-band saturation (tanh waveshaper)");
    typeBox.setTooltip("Filter type");
    slopeBox.setTooltip("Filter slope (12/24/48 dB/oct)");
    channelBox.setTooltip("Per-band channel routing");
    linkBox.setTooltip("Band link group (A or B)");
    bandOn.setTooltip("Enable/disable this band");
    bandSolo.setTooltip("Solo — audition this band only");
    dynOn.setTooltip("Enable dynamic EQ for this band");
    dynThreshKnob.setTooltip("Dynamic EQ threshold (-60 to 0 dB)");
    dynRatioKnob.setTooltip("Dynamic EQ ratio (1:1 to 20:1)");
    dynAttackKnob.setTooltip("Dynamic EQ attack (0.1 - 100 ms)");
    dynReleaseKnob.setTooltip("Dynamic EQ release (1 - 1000 ms)");
    outputGainSlider.setTooltip("Master output gain (-24 to +24 dB)");
    scaleSlider.setTooltip("Scale all band gains (0.1x to 2x)");
    adaptiveQBtn.setTooltip("Auto-widen Q with increasing gain");
    linPhaseBtn.setTooltip("Linear phase mode (adds 2048 samples latency)");
    oversamplingBox.setTooltip("Oversampling factor (higher = cleaner, more CPU)");
    procModeBox.setTooltip("Stereo or Mid-Side processing");
    matchCapBtn.setTooltip("Capture reference spectrum");
    matchAppBtn.setTooltip("Apply match EQ correction");
    matchClrBtn.setTooltip("Clear match EQ data");
    undoBtn.setTooltip("Undo last parameter change");
    redoBtn.setTooltip("Redo last undone change");
    postEqToggle.setTooltip("Toggle pre/post EQ spectrum display");

#if PROEQ8
    // ── A/B comparison buttons ──
    abBtn.onClick = [this] { toggleAB(); };
    abBtn.setTooltip("Switch between A/B settings");
    addAndMakeVisible(abBtn);

    copyABBtn.onClick = [this] {
        if (proc.isSlotA)
            proc.copySnapshot(true);   // A → B
        else
            proc.copySnapshot(false);  // B → A
    };
    copyABBtn.setTooltip("Copy current slot to the other");
    addAndMakeVisible(copyABBtn);

    // Store initial state into slot A
    proc.storeSnapshot(true);

    // License activation button
    licenseBtn.setTooltip("Enter license key to activate ProEQ8");
    licenseBtn.onClick = [this] { showActivationDialog(); };
    if (proc.licenseValidator.isActivated())
        licenseBtn.setButtonText("Licensed");
    addAndMakeVisible(licenseBtn);
#endif

    // ── Update checker ──
    updateChecker.checkAsync();

    // ── Initial band selection ──
    rebindBandControls(0);
    selectBand(0);
    startTimerHz(30);
}

// ── Rebind controls to a specific band ─────────────────────────────
void FreeEQ8AudioProcessorEditor::rebindBandControls(int band)
{
    const int idx = band + 1;

    // Destroy old attachments first
    bandOnAtt.reset(); bandSoloAtt.reset(); dynOnAtt.reset();
    typeAtt.reset(); slopeAtt.reset(); channelAtt.reset(); linkAtt.reset();
    freqAtt.reset(); gainAtt.reset(); qAtt.reset(); driveAtt.reset();
    dynThreshAtt.reset(); dynRatioAtt.reset(); dynAttackAtt.reset(); dynReleaseAtt.reset();

    // Create new attachments
    bandOnAtt    = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(proc.apvts, bandId(idx, "on"),   bandOn);
    bandSoloAtt  = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(proc.apvts, bandId(idx, "solo"), bandSolo);
    typeAtt      = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(proc.apvts, bandId(idx, "type"),  typeBox);
    slopeAtt     = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(proc.apvts, bandId(idx, "slope"), slopeBox);
    channelAtt   = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(proc.apvts, bandId(idx, "ch"),   channelBox);
    linkAtt      = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(proc.apvts, bandId(idx, "link"), linkBox);
    freqAtt      = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(proc.apvts, bandId(idx, "freq"),  freqKnob);
    gainAtt      = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(proc.apvts, bandId(idx, "gain"),  gainKnob);
    qAtt         = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(proc.apvts, bandId(idx, "q"),     qKnob);
    driveAtt     = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(proc.apvts, bandId(idx, "drive"), driveKnob);
#if PROEQ8
    satModeAtt.reset();
    satModeAtt   = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(proc.apvts, bandId(idx, "sat_mode"), satModeBox);
#endif
    dynOnAtt     = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(proc.apvts, bandId(idx, "dyn_on"),      dynOn);
    dynThreshAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(proc.apvts, bandId(idx, "dyn_thresh"),  dynThreshKnob);
    dynRatioAtt  = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(proc.apvts, bandId(idx, "dyn_ratio"),   dynRatioKnob);
    dynAttackAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(proc.apvts, bandId(idx, "dyn_attack"),  dynAttackKnob);
    dynReleaseAtt= std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(proc.apvts, bandId(idx, "dyn_release"), dynReleaseKnob);

    // Update knob colors to match band
    auto col = ResponseCurveComponent::getBandColour(band);
    freqKnob.setColour(juce::Slider::rotarySliderFillColourId, col);
    gainKnob.setColour(juce::Slider::rotarySliderFillColourId, col);
    qKnob.setColour(juce::Slider::rotarySliderFillColourId, col);
    driveKnob.setColour(juce::Slider::rotarySliderFillColourId, col);
}

// ── Select band ────────────────────────────────────────────────────
void FreeEQ8AudioProcessorEditor::selectBand(int band)
{
    selectedBand = band;
    rebindBandControls(band);
    responseCurve.setSelectedBand(band);

    for (int i = 0; i < kNumBands; ++i)
    {
        bool sel = (i == band);
        bandBtns[(size_t)i].setColour(juce::TextButton::buttonColourId,
            ResponseCurveComponent::getBandColour(i).withAlpha(sel ? 0.85f : 0.2f));
    }
    repaint();
}

// ── Timer ──────────────────────────────────────────────────────────
void FreeEQ8AudioProcessorEditor::timerCallback()
{
    auto& fifo = showPostSpectrum ? proc.spectrumFifo : proc.preSpectrumFifo;
    if (fifo.processIfReady())
    {
        responseCurve.pushSpectrumData(
            fifo.getMagnitudes(), fifo.getNumBins(),
            proc.getSampleRate() > 0 ? proc.getSampleRate() : 44100.0);
    }

    // Sync band selection from response curve (when user clicks nodes)
    int curveSel = responseCurve.getSelectedBand();
    if (curveSel >= 0 && curveSel != selectedBand)
        selectBand(curveSel);

    // Check for updates
    if (!hasUpdate && updateChecker.isUpdateAvailable())
    {
        hasUpdate = true;
        repaint();
    }

    // Update band button on/off appearance
    for (int i = 0; i < kNumBands; ++i)
    {
        bool on = proc.apvts.getRawParameterValue(bandId(i + 1, "on"))->load() > 0.5f;
        bandBtns[(size_t)i].setAlpha(on ? 1.0f : 0.4f);
    }
}

// ── Paint ──────────────────────────────────────────────────────────
void FreeEQ8AudioProcessorEditor::paint(juce::Graphics& g)
{
    const int w = getWidth();
    const int h = getHeight();
    const int titleH = 32;
    const int curveH = std::max(180, (int)(h * 0.52f));
    const int stripH = 28;
    const int sidebarW = 155;
    const int meterW = 36;
    const int controlsTop = titleH + curveH + stripH;

    g.fillAll(juce::Colour(0xFF0D0D1A));

    // Title bar
    g.setColour(juce::Colour(0xFF16213E));
    g.fillRect(0, 0, w, titleH);
    g.setColour(juce::Colours::white.withAlpha(0.9f));
    g.setFont(15.0f);
    g.drawText(kProductName, 12, 6, 90, 20, juce::Justification::left);
    g.setFont(10.0f);
    g.setColour(juce::Colours::white.withAlpha(0.4f));
    g.drawText(kProductTag, 82, 6, 160, 20, juce::Justification::left);

    // Band strip background
    g.setColour(juce::Colour(0xFF12172E));
    g.fillRect(0, titleH + curveH, w, stripH);

    // Controls panel background
    g.setColour(juce::Colour(0xFF111528));
    g.fillRect(0, controlsTop, w, h - controlsTop);

    // Separator lines
    g.setColour(juce::Colours::white.withAlpha(0.08f));
    g.drawHorizontalLine(controlsTop, 0.0f, (float)w);

    // Vertical separator between band panel and global sidebar
    const int sepX = w - sidebarW - meterW;
    g.setColour(juce::Colours::white.withAlpha(0.1f));
    g.drawVerticalLine(sepX, (float)controlsTop, (float)h);

    // ── Row 1 labels — computed from the SAME layout constants as resized() ──
    g.setFont(10.0f);
    g.setColour(juce::Colours::white.withAlpha(0.45f));
    const int ly = controlsTop + 4;  // label y (12px above controls at controlsTop+16)

    // Mirror the x-offsets from resized() Row 1:
    const int knobL = 80, knobS = 56;
    int lx = 6;                              // On/Solo column
    lx += 48;                                // past On/Solo → typeBox start
    g.drawText("Type", lx, ly, 86, 12, juce::Justification::centred);
    lx += 92;                                // past typeBox → freqKnob start
    g.drawText("Freq", lx, ly, knobL, 12, juce::Justification::centred);
    lx += knobL + 2;
    g.drawText("Gain", lx, ly, knobL, 12, juce::Justification::centred);
    lx += knobL + 2;
    g.drawText("Q",    lx, ly, knobL, 12, juce::Justification::centred);
    lx += knobL + 2;
    g.drawText("Slope", lx, ly, 78, 12, juce::Justification::centred);
    lx += 84;
    g.drawText("Drive", lx, ly, knobS, 12, juce::Justification::centred);

    // ── Row 2 labels — match resized() Row 2 positions ──
    const int r2ly = controlsTop + 4 + 90; // label row 2
    g.drawText("Channel", 6,  r2ly, 72, 12, juce::Justification::centred);
    g.drawText("Link",    82, r2ly, 58, 12, juce::Justification::centred);

    // Dynamic EQ section
    int dlx = 155;
    g.drawText("Dyn", dlx, r2ly, 50, 12, juce::Justification::centred);
    dlx += 55;
    g.drawText("Thr",   dlx, r2ly, knobS, 12, juce::Justification::centred);
    dlx += knobS + 4;
    g.drawText("Ratio", dlx, r2ly, knobS, 12, juce::Justification::centred);
    dlx += knobS + 4;
    g.drawText("Atk",   dlx, r2ly, knobS, 12, juce::Justification::centred);
    dlx += knobS + 4;
    g.drawText("Rel",   dlx, r2ly, knobS, 12, juce::Justification::centred);

    // Sidebar labels
    const int sx = w - sidebarW - meterW + 4;
    g.drawText("Output",  sx, controlsTop + 4,  60, 12, juce::Justification::centred);
    g.drawText("Scale",   sx + 65, controlsTop + 4,  50, 12, juce::Justification::centred);

    // Band indicator on selected band
    auto bandCol = ResponseCurveComponent::getBandColour(selectedBand);
    g.setColour(bandCol);
    g.fillRoundedRectangle(2.0f, (float)controlsTop + 1, 3.0f, 14.0f, 1.5f);

    // ── Update banner ──
    if (hasUpdate)
    {
        g.setColour(juce::Colour(0xFF2196F3));
        g.fillRect(0, h - 20, w, 20);
        g.setColour(juce::Colours::white);
        g.setFont(11.0f);
        g.drawText("Update available: v" + updateChecker.getLatestVersion()
                   + " — visit GitHub to download",
                   0, h - 20, w, 20, juce::Justification::centred);
    }
}

// ── Resized ────────────────────────────────────────────────────────
void FreeEQ8AudioProcessorEditor::resized()
{
    const int w = getWidth();
    const int h = getHeight();
    const int titleH = 32;
    const int curveH = std::max(180, (int)(h * 0.52f));
    const int stripH = 28;
    const int sidebarW = 155;
    const int meterW = 36;
    const int controlsTop = titleH + curveH + stripH;
    const int controlsH = h - controlsTop;
    const int panelW = w - sidebarW - meterW;

    // Response curve
    responseCurve.setBounds(0, titleH, w, curveH);

    // Title bar controls
    postEqToggle.setBounds(w - 90, 6, 80, 20);
    presetBox.setBounds(w - 370, 5, 120, 22);
    saveBtn.setBounds(w - 242, 5, 42, 22);
    delBtn.setBounds(w - 196, 5, 32, 22);

    // Band selector strip
    {
        const int btnW = std::min(60, (w - 20) / kNumBands);
        const int stripY = titleH + curveH;
        const int totalW = btnW * kNumBands;
        const int startX = (w - totalW) / 2;
        for (int i = 0; i < kNumBands; ++i)
            bandBtns[(size_t)i].setBounds(startX + i * btnW, stripY + 2, btnW - 2, stripH - 4);
    }

    // ── Selected band controls ──
    const int cy = controlsTop + 16;
    const int knobL = 80;  // large knob
    const int knobS = 56;  // small knob
    int x = 6;

    // Row 1: On/Solo | Type | Freq | Gain | Q | Slope | Drive
    bandOn.setBounds(x, cy, 44, 20);
    bandSolo.setBounds(x, cy + 22, 44, 20);
    x += 48;

    typeBox.setBounds(x, cy + 6, 86, 22);
    x += 92;

    freqKnob.setBounds(x, cy - 4, knobL, knobL);
    x += knobL + 2;
    gainKnob.setBounds(x, cy - 4, knobL, knobL);
    x += knobL + 2;
    qKnob.setBounds(x, cy - 4, knobL, knobL);
    x += knobL + 2;

    slopeBox.setBounds(x, cy + 6, 78, 22);
    x += 84;

    driveKnob.setBounds(x, cy, knobS, knobS);
#if PROEQ8
    x += knobS + 4;
    satModeBox.setBounds(x, cy + 6, 82, 22);
#endif

    // Row 2: Channel | Link | Dynamic EQ
    const int r2y = cy + 90 + 12;
    channelBox.setBounds(6, r2y, 72, 20);
    linkBox.setBounds(82, r2y, 58, 20);

    int dynX = 155;
    dynOn.setBounds(dynX, r2y, 50, 20);
    dynX += 55;
    dynThreshKnob.setBounds(dynX, r2y - 8, knobS, knobS);
    dynX += knobS + 4;
    dynRatioKnob.setBounds(dynX, r2y - 8, knobS, knobS);
    dynX += knobS + 4;
    dynAttackKnob.setBounds(dynX, r2y - 8, knobS, knobS);
    dynX += knobS + 4;
    dynReleaseKnob.setBounds(dynX, r2y - 8, knobS, knobS);

    // ── Global sidebar ──
    const int sx = w - sidebarW - meterW + 4;
    const int sw = sidebarW - 8;
    int sy = controlsTop + 16;

    outputGainSlider.setBounds(sx, sy, 60, 70);
    scaleSlider.setBounds(sx + 65, sy, 55, 55);
    sy += 74;

    const int halfW = (sw - 4) / 2;
    adaptiveQBtn.setBounds(sx, sy, sw, 18);
    sy += 20;
    linPhaseBtn.setBounds(sx, sy, sw, 18);
    sy += 20;
    autoGainBtn.setBounds(sx, sy, sw, 18);
    sy += 22;
    oversamplingBox.setBounds(sx, sy, halfW, 18);
    procModeBox.setBounds(sx + halfW + 4, sy, halfW, 18);
    sy += 22;

    // Match EQ
    matchCapBtn.setBounds(sx, sy, sw / 3 - 1, 18);
    matchAppBtn.setBounds(sx + sw / 3 + 1, sy, sw / 3 - 1, 18);
    matchClrBtn.setBounds(sx + 2 * (sw / 3) + 2, sy, sw / 3 - 1, 18);
    sy += 22;

    // Undo/Redo
    undoBtn.setBounds(sx, sy, halfW, 18);
    redoBtn.setBounds(sx + halfW + 4, sy, halfW, 18);
    sy += 22;

#if PROEQ8
    // A/B comparison
    abBtn.setBounds(sx, sy, halfW, 18);
    copyABBtn.setBounds(sx + halfW + 4, sy, halfW, 18);
    sy += 22;
    licenseBtn.setBounds(sx, sy, sw, 18);
#endif

    // Level meter
    levelMeter.setBounds(w - meterW, controlsTop, meterW - 4, controlsH);
}

// ── Preset management ──────────────────────────────────────────────
void FreeEQ8AudioProcessorEditor::refreshPresetList()
{
    presetBox.clear(juce::dontSendNotification);
    if (proc.presetManager)
    {
        auto presets = proc.presetManager->getPresetList();
        for (int i = 0; i < presets.size(); ++i)
            presetBox.addItem(presets[i], i + 1);

        auto current = proc.presetManager->getCurrentPreset();
        if (current.isNotEmpty())
        {
            int idx = presets.indexOf(current);
            if (idx >= 0)
                presetBox.setSelectedId(idx + 1, juce::dontSendNotification);
        }
    }
}

void FreeEQ8AudioProcessorEditor::onPresetSelected()
{
    const auto name = presetBox.getText();
    if (name.isNotEmpty() && proc.presetManager)
        proc.presetManager->loadPreset(name);
}

void FreeEQ8AudioProcessorEditor::onSaveClicked()
{
    auto name = proc.presetManager ? proc.presetManager->getCurrentPreset() : juce::String();
    auto* dlg = new juce::AlertWindow("Save Preset", "Enter preset name:", juce::AlertWindow::NoIcon);
    dlg->addTextEditor("name", name, "Preset Name:");
    dlg->addButton("Save", 1);
    dlg->addButton("Cancel", 0);

    dlg->enterModalState(true, juce::ModalCallbackFunction::create(
        [this, dlg](int result) {
            if (result == 1) {
                auto presetName = dlg->getTextEditorContents("name");
                if (presetName.isNotEmpty() && proc.presetManager) {
                    proc.presetManager->savePreset(presetName);
                    refreshPresetList();
                }
            }
            delete dlg;
        }), true);
}

void FreeEQ8AudioProcessorEditor::onDeleteClicked()
{
    const auto name = presetBox.getText();
    if (name.isNotEmpty() && proc.presetManager)
    {
        proc.presetManager->deletePreset(name);
        refreshPresetList();
    }
}

#if PROEQ8
void FreeEQ8AudioProcessorEditor::showActivationDialog()
{
    if (proc.licenseValidator.isActivated())
    {
        // Already activated — show deactivation option
        auto* dlg = new juce::AlertWindow("ProEQ8 License",
            "Licensed to: " + proc.licenseValidator.getEmail(),
            juce::AlertWindow::InfoIcon);
        dlg->addButton("OK", 0);
        dlg->addButton("Deactivate", 1);
        dlg->enterModalState(true, juce::ModalCallbackFunction::create(
            [this, dlg](int result) {
                if (result == 1)
                {
                    proc.licenseValidator.deactivate();
                    licenseBtn.setButtonText("Activate");
                }
                delete dlg;
            }), true);
        return;
    }

    auto* dlg = new juce::AlertWindow("Activate ProEQ8",
        "Enter your license key:", juce::AlertWindow::NoIcon);
    dlg->addTextEditor("key", "", "License Key:");
    dlg->addButton("Activate", 1);
    dlg->addButton("Cancel", 0);

    dlg->enterModalState(true, juce::ModalCallbackFunction::create(
        [this, dlg](int result) {
            if (result == 1)
            {
                auto key = dlg->getTextEditorContents("key");
                if (proc.licenseValidator.activate(key))
                    licenseBtn.setButtonText("Licensed");
                else
                    juce::AlertWindow::showMessageBoxAsync(
                        juce::AlertWindow::WarningIcon,
                        "Activation Failed",
                        "Invalid or expired license key.");
            }
            delete dlg;
        }), true);
}

void FreeEQ8AudioProcessorEditor::toggleAB()
{
    // Store current into the active slot, then switch
    proc.storeSnapshot(proc.isSlotA);
    proc.isSlotA = !proc.isSlotA;
    proc.recallSnapshot(proc.isSlotA);

    abBtn.setButtonText(proc.isSlotA ? "A" : "B");
    copyABBtn.setButtonText(proc.isSlotA ? "A\xe2\x86\x92" "B" : "B\xe2\x86\x92" "A");
    rebindBandControls(selectedBand);
    repaint();
}
#endif
