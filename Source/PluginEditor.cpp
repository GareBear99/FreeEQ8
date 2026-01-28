#include "PluginEditor.h"

static juce::String bandId(int idx, const char* suffix)
{
    return "b" + juce::String(idx) + "_" + suffix;
}

FreeEQ8AudioProcessorEditor::FreeEQ8AudioProcessorEditor(FreeEQ8AudioProcessor& p)
: juce::AudioProcessorEditor(&p), proc(p)
{
    setSize(900, 420);

    auto typeChoices = juce::StringArray { "Bell", "LowShelf", "HighShelf", "HighPass", "LowPass" };

    for (int i = 1; i <= 8; ++i)
    {
        auto& b = ui[(size_t)i - 1];

        b.on.setButtonText("On");
        addAndMakeVisible(b.on);

        b.type.addItemList(typeChoices, 1);
        addAndMakeVisible(b.type);

        auto initSlider = [&](juce::Slider& s, const juce::String& name)
        {
            s.setName(name);
            s.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
            s.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 70, 18);
            addAndMakeVisible(s);
        };

        initSlider(b.freq, "Freq");
        initSlider(b.q, "Q");
        initSlider(b.gain, "Gain");

        b.onAtt   = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(proc.apvts, bandId(i,"on"), b.on);
        b.typeAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(proc.apvts, bandId(i,"type"), b.type);
        b.freqAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(proc.apvts, bandId(i,"freq"), b.freq);
        b.qAtt    = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(proc.apvts, bandId(i,"q"), b.q);
        b.gainAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(proc.apvts, bandId(i,"gain"), b.gain);
    }
    
    // Global controls
    outputGainSlider.setName("Output");
    outputGainSlider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    outputGainSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 70, 18);
    addAndMakeVisible(outputGainSlider);
    
    scaleSlider.setName("Scale");
    scaleSlider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    scaleSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 70, 18);
    addAndMakeVisible(scaleSlider);
    
    adaptiveQButton.setButtonText("Adaptive Q");
    addAndMakeVisible(adaptiveQButton);
    
    outputGainAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(proc.apvts, "output_gain", outputGainSlider);
    scaleAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(proc.apvts, "scale", scaleSlider);
    adaptiveQAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(proc.apvts, "adaptive_q", adaptiveQButton);
}

void FreeEQ8AudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colours::black.withAlpha(0.92f));
    g.setColour(juce::Colours::white);
    g.setFont(18.0f);
    g.drawText("FreeEQ8 (EQ8-inspired base)", 12, 10, getWidth() - 24, 24, juce::Justification::left);

    g.setFont(12.0f);
    g.setColour(juce::Colours::white.withAlpha(0.7f));
    g.drawText("Original code • 8-band parametric EQ • VST3/AU", 12, 34, getWidth() - 24, 18, juce::Justification::left);
}

void FreeEQ8AudioProcessorEditor::resized()
{
    const int top = 60;
    const int colW = (getWidth() - 140) / 8; // Reserve space for global controls
    const int pad = 8;

    for (int i = 0; i < 8; ++i)
    {
        auto& b = ui[(size_t)i];

        const int x = i * colW + pad;
        const int w = colW - pad * 2;

        b.on.setBounds(x, top, w, 22);
        b.type.setBounds(x, top + 26, w, 22);

        const int knobY = top + 56;
        const int knobH = 90;

        b.freq.setBounds(x, knobY, w, knobH);
        b.q.setBounds   (x, knobY + knobH, w, knobH);
        b.gain.setBounds(x, knobY + knobH * 2, w, knobH);
    }
    
    // Global controls on the right side
    const int globalX = 8 * colW + pad * 2;
    const int globalW = 100;
    
    outputGainSlider.setBounds(globalX, top + 56, globalW, 90);
    scaleSlider.setBounds(globalX, top + 146, globalW, 90);
    adaptiveQButton.setBounds(globalX, top + 246, globalW, 22);
}
