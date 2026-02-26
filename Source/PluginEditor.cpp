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
    for (int i = 0; i < 8; ++i)
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

    typeBox.addItemList({ "Bell", "LowShelf", "HighShelf", "HighPass", "LowPass" }, 1);
    addAndMakeVisible(typeBox);

    slopeBox.addItemList({ "12 dB", "24 dB", "48 dB" }, 1);
    addAndMakeVisible(slopeBox);

    channelBox.addItemList({ "Both", "L / Mid", "R / Side" }, 1);
    addAndMakeVisible(channelBox);

    linkBox.addItemList({ "--", "A", "B" }, 1);
    addAndMakeVisible(linkBox);

    auto bandCol = ResponseCurveComponent::getBandColour(0);
    initKnob(freqKnob,  bandCol, true);
    initKnob(gainKnob,  bandCol, true);
    initKnob(qKnob,     bandCol, true);
    initKnob(driveKnob, bandCol, false);

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

    oversamplingBox.addItemList({ "1x", "2x", "4x", "8x" }, 1);
    addAndMakeVisible(oversamplingBox);
    procModeBox.addItemList({ "Stereo", "Mid-Side" }, 1);
    addAndMakeVisible(procModeBox);

    // Global attachments (permanent)
    outputGainAtt  = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(proc.apvts, "output_gain", outputGainSlider);
    scaleAtt       = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(proc.apvts, "scale", scaleSlider);
    adaptiveQAtt   = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(proc.apvts, "adaptive_q", adaptiveQBtn);
    linPhaseAtt    = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(proc.apvts, "linear_phase", linPhaseBtn);
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

    for (int i = 0; i < 8; ++i)
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

    // Update band button on/off appearance
    for (int i = 0; i < 8; ++i)
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
    const int controlsTop = titleH + curveH + stripH;

    g.fillAll(juce::Colour(0xFF0D0D1A));

    // Title bar
    g.setColour(juce::Colour(0xFF16213E));
    g.fillRect(0, 0, w, titleH);
    g.setColour(juce::Colours::white.withAlpha(0.9f));
    g.setFont(15.0f);
    g.drawText("FreeEQ8", 12, 6, 90, 20, juce::Justification::left);
    g.setFont(10.0f);
    g.setColour(juce::Colours::white.withAlpha(0.4f));
    g.drawText("8-Band Parametric EQ", 82, 6, 160, 20, juce::Justification::left);

    // Band strip background
    g.setColour(juce::Colour(0xFF12172E));
    g.fillRect(0, titleH + curveH, w, stripH);

    // Controls panel background
    g.setColour(juce::Colour(0xFF111528));
    g.fillRect(0, controlsTop, w, h - controlsTop);

    // Separator lines
    g.setColour(juce::Colours::white.withAlpha(0.08f));
    g.drawHorizontalLine(controlsTop, 0.0f, (float)w);

    // ── Control labels (painted) ──
    const int sidebarW = 155;
    const int meterW = 36;
    const int panelW = w - sidebarW - meterW;
    const int panelX = 0;
    const int cy = controlsTop + 4;

    g.setFont(10.0f);
    g.setColour(juce::Colours::white.withAlpha(0.45f));

    // Row 1 labels
    int lx = panelX + 90;
    g.drawText("Type",  panelX + 6,  cy, 40, 12, juce::Justification::left);
    g.drawText("Freq",  lx,          cy, 40, 12, juce::Justification::centred);
    g.drawText("Gain",  lx + 80,     cy, 40, 12, juce::Justification::centred);
    g.drawText("Q",     lx + 160,    cy, 40, 12, juce::Justification::centred);
    g.drawText("Slope", lx + 230,    cy, 40, 12, juce::Justification::centred);
    g.drawText("Drive", lx + 295,    cy, 40, 12, juce::Justification::centred);

    // Row 2 labels
    int r2y = cy + 90;
    g.drawText("Channel", panelX + 6,  r2y, 50, 12, juce::Justification::left);
    g.drawText("Link",    panelX + 80, r2y, 30, 12, juce::Justification::left);

    // Dynamic section
    int dynX = panelX + 155;
    g.drawText("Dynamic EQ", dynX, r2y, 80, 12, juce::Justification::left);
    g.drawText("Thr",  dynX + 80,  r2y, 30, 12, juce::Justification::centred);
    g.drawText("Ratio", dynX + 140, r2y, 35, 12, juce::Justification::centred);
    g.drawText("Atk",  dynX + 200, r2y, 30, 12, juce::Justification::centred);
    g.drawText("Rel",  dynX + 260, r2y, 30, 12, juce::Justification::centred);

    // Sidebar labels
    int sx = w - sidebarW - meterW + 4;
    g.drawText("Output",  sx, controlsTop + 4,  60, 12, juce::Justification::centred);
    g.drawText("Scale",   sx + 65, controlsTop + 4,  50, 12, juce::Justification::centred);

    // Band indicator on selected band
    auto bandCol = ResponseCurveComponent::getBandColour(selectedBand);
    g.setColour(bandCol);
    g.fillRoundedRectangle((float)panelX + 2, (float)controlsTop + 1, 3.0f, 14.0f, 1.5f);
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
        const int btnW = std::min(60, (w - 20) / 8);
        const int stripY = titleH + curveH;
        const int totalW = btnW * 8;
        const int startX = (w - totalW) / 2;
        for (int i = 0; i < 8; ++i)
            bandBtns[(size_t)i].setBounds(startX + i * btnW, stripY + 2, btnW - 2, stripH - 4);
    }

    // ── Selected band controls ──
    const int cy = controlsTop + 16;
    const int knobL = 80;  // large knob
    const int knobS = 56;  // small knob
    int x = 6;

    // Row 1: On/Solo | Type | Freq | Gain | Q | Slope | Drive
    bandOn.setBounds(x, cy, 42, 20);
    bandSolo.setBounds(x, cy + 22, 42, 20);
    x += 48;

    typeBox.setBounds(x, cy + 6, 82, 22);
    x += 88;

    freqKnob.setBounds(x, cy - 4, knobL, knobL);
    x += knobL + 2;
    gainKnob.setBounds(x, cy - 4, knobL, knobL);
    x += knobL + 2;
    qKnob.setBounds(x, cy - 4, knobL, knobL);
    x += knobL + 2;

    slopeBox.setBounds(x, cy + 6, 62, 22);
    x += 68;

    driveKnob.setBounds(x, cy, knobS, knobS);

    // Row 2: Channel | Link | Dynamic EQ
    const int r2y = cy + 90 + 12;
    channelBox.setBounds(6, r2y, 68, 20);
    linkBox.setBounds(80, r2y, 50, 20);

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
