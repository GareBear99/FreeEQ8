#include "ResponseCurveComponent.h"
#include "../PluginProcessor.h"

static juce::String bandId(int idx, const char* suffix)
{
    return "b" + juce::String(idx) + "_" + suffix;
}

// ── Band colours (EQ Eight–style palette) ──────────────────────────
juce::Colour ResponseCurveComponent::getBandColour(int i)
{
    static const juce::Colour colours[] = {
        juce::Colour(0xFFE84855),  // red
        juce::Colour(0xFFFF9B42),  // orange
        juce::Colour(0xFFFFEB3B),  // yellow
        juce::Colour(0xFF8BC34A),  // green
        juce::Colour(0xFF26C6DA),  // cyan
        juce::Colour(0xFF42A5F5),  // blue
        juce::Colour(0xFFAB47BC),  // purple
        juce::Colour(0xFFEC407A),  // pink
    };
    return colours[i % 8];
}

// ── Constructor ────────────────────────────────────────────────────
ResponseCurveComponent::ResponseCurveComponent(FreeEQ8AudioProcessor& processor)
    : proc(processor)
{
    startTimerHz(30);
    setMouseCursor(juce::MouseCursor::CrosshairCursor);
}

// ── Coordinate mapping ─────────────────────────────────────────────
float ResponseCurveComponent::freqToX(float freqHz) const
{
    const float w = (float)getWidth();
    const float logMin = std::log10(minFreq);
    const float logMax = std::log10(maxFreq);
    return w * (std::log10(freqHz) - logMin) / (logMax - logMin);
}

float ResponseCurveComponent::xToFreq(float x) const
{
    const float w = (float)getWidth();
    const float logMin = std::log10(minFreq);
    const float logMax = std::log10(maxFreq);
    return std::pow(10.0f, logMin + (x / w) * (logMax - logMin));
}

float ResponseCurveComponent::dbToY(float db) const
{
    const float h = (float)getHeight();
    // 0 dB is at center
    return h * 0.5f * (1.0f - db / maxDb);
}

float ResponseCurveComponent::yToDb(float y) const
{
    const float h = (float)getHeight();
    return maxDb * (1.0f - 2.0f * y / h);
}

// ── Magnitude response from biquad coefficients ────────────────────
float ResponseCurveComponent::computeMagnitudeDb(const Biquad& bq, double freq, double sampleRate)
{
    // H(z) = (b0 + b1*z^-1 + b2*z^-2) / (1 + a1*z^-1 + a2*z^-2)
    // Evaluate at z = e^(j*omega), omega = 2*pi*freq/sampleRate
    const double omega = 2.0 * M_PI * freq / sampleRate;
    const double cosw  = std::cos(omega);
    const double cos2w = std::cos(2.0 * omega);
    const double sinw  = std::sin(omega);
    const double sin2w = std::sin(2.0 * omega);

    const double numReal = bq.b0 + bq.b1 * cosw + bq.b2 * cos2w;
    const double numImag = -(bq.b1 * sinw + bq.b2 * sin2w);
    const double denReal = 1.0 + bq.a1 * cosw + bq.a2 * cos2w;
    const double denImag = -(bq.a1 * sinw + bq.a2 * sin2w);

    const double numMagSq = numReal * numReal + numImag * numImag;
    const double denMagSq = denReal * denReal + denImag * denImag;

    if (denMagSq < 1e-30)
        return 0.0f;

    const double magSq = numMagSq / denMagSq;
    return (float)(10.0 * std::log10(std::max(magSq, 1e-30)));
}

// ── Update response curve from processor state ─────────────────────
void ResponseCurveComponent::updateResponseCurve()
{
    const double sr = proc.getSampleRate() > 0 ? proc.getSampleRate() : 44100.0;
    const float scale = proc.apvts.getRawParameterValue("scale")->load();

    // Clear composite
    std::fill(std::begin(magnitudes), std::end(magnitudes), 0.0f);

    for (int b = 0; b < 8; ++b)
    {
        const int idx = b + 1;
        const bool on = proc.apvts.getRawParameterValue(bandId(idx, "on"))->load() > 0.5f;

        if (!on)
        {
            std::fill(std::begin(perBandMagnitudes[b]), std::end(perBandMagnitudes[b]), 0.0f);
            continue;
        }

        const int t = (int)proc.apvts.getRawParameterValue(bandId(idx, "type"))->load();
        const float freq = proc.apvts.getRawParameterValue(bandId(idx, "freq"))->load();
        const float q    = proc.apvts.getRawParameterValue(bandId(idx, "q"))->load();
        const float gain = proc.apvts.getRawParameterValue(bandId(idx, "gain"))->load() * scale;

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

        // Slope: number of cascaded stages
        const int slopeIdx = (int)proc.apvts.getRawParameterValue(bandId(idx, "slope"))->load();
        static const int slopeToStages[] = { 1, 2, 4 };
        const int numStages = slopeToStages[std::clamp(slopeIdx, 0, 2)];

        // Build a temporary biquad with current coefficients
        Biquad tempBq;
        tempBq.set(tp, sr, freq, q, gain);

        for (int i = 0; i < numPoints; ++i)
        {
            const float logMin = std::log10(minFreq);
            const float logMax = std::log10(maxFreq);
            const float f = std::pow(10.0f, logMin + (float)i / (float)(numPoints - 1) * (logMax - logMin));

            // Cascaded stages multiply the magnitude (add in dB)
            const float mag = computeMagnitudeDb(tempBq, f, sr) * (float)numStages;
            perBandMagnitudes[b][i] = mag;
            magnitudes[i] += mag;
        }
    }
}

// ── Timer callback ─────────────────────────────────────────────────
void ResponseCurveComponent::timerCallback()
{
    updateResponseCurve();
    repaint();
}

void ResponseCurveComponent::resized() {}

// ── Paint ──────────────────────────────────────────────────────────
void ResponseCurveComponent::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xFF1A1A2E));

    paintGrid(g);
    paintSpectrum(g);
    paintBandCurves(g);
    paintResponseCurve(g);
    paintNodes(g);

    // Border
    g.setColour(juce::Colours::white.withAlpha(0.3f));
    g.drawRect(getLocalBounds(), 1);
}

// ── Grid ───────────────────────────────────────────────────────────
void ResponseCurveComponent::paintGrid(juce::Graphics& g)
{
    const float h = (float)getHeight();
    g.setColour(juce::Colours::white.withAlpha(0.08f));

    // Frequency grid lines
    const float freqLines[] = { 50, 100, 200, 500, 1000, 2000, 5000, 10000 };
    for (float f : freqLines)
    {
        const float x = freqToX(f);
        g.drawVerticalLine((int)x, 0.0f, h);
    }

    // dB grid lines
    const float dbLines[] = { -18, -12, -6, 0, 6, 12, 18 };
    for (float db : dbLines)
    {
        const float y = dbToY(db);
        if (db == 0.0f)
        {
            g.setColour(juce::Colours::white.withAlpha(0.2f));
            g.drawHorizontalLine((int)y, 0.0f, (float)getWidth());
            g.setColour(juce::Colours::white.withAlpha(0.08f));
        }
        else
        {
            g.drawHorizontalLine((int)y, 0.0f, (float)getWidth());
        }
    }

    // Frequency labels
    g.setColour(juce::Colours::white.withAlpha(0.35f));
    g.setFont(10.0f);

    const std::pair<float, const char*> freqLabels[] = {
        { 100,  "100" }, { 1000, "1k" }, { 10000, "10k" }
    };
    for (auto& [f, label] : freqLabels)
    {
        const float x = freqToX(f);
        g.drawText(label, (int)x - 15, (int)h - 14, 30, 12, juce::Justification::centred);
    }

    // dB labels
    const std::pair<float, const char*> dbLabels[] = {
        { 12, "+12" }, { 6, "+6" }, { 0, "0" }, { -6, "-6" }, { -12, "-12" }
    };
    for (auto& [db, label] : dbLabels)
    {
        const float y = dbToY(db);
        g.drawText(label, 2, (int)y - 6, 28, 12, juce::Justification::left);
    }
}

// ── Spectrum analyzer ──────────────────────────────────────────────
void ResponseCurveComponent::pushSpectrumData(const float* mags, int numBins, double sr)
{
    const int n = std::min(numBins, maxSpectrumBins);
    std::copy(mags, mags + n, spectrumMagnitudes);
    currentSpectrumSize = n;
    spectrumSampleRate = sr;
}

void ResponseCurveComponent::paintSpectrum(juce::Graphics& g)
{
    if (currentSpectrumSize == 0)
        return;

    const float w = (float)getWidth();
    const float h = (float)getHeight();
    const float binWidth = (float)(spectrumSampleRate / (2.0 * currentSpectrumSize));

    // Smooth the spectrum with exponential decay
    const float decay = 0.85f;
    for (int i = 0; i < currentSpectrumSize; ++i)
        smoothedSpectrum[i] = std::max(spectrumMagnitudes[i], smoothedSpectrum[i] * decay);

    juce::Path spectrumPath;
    bool started = false;

    for (int i = 1; i < currentSpectrumSize; ++i)
    {
        const float freq = (float)i * binWidth;
        if (freq < minFreq || freq > maxFreq)
            continue;

        const float x = freqToX(freq);
        // Map dB: spectrum is in dB, map -90..0 dB to bottom..mid
        const float db = juce::jlimit(-90.0f, 0.0f, smoothedSpectrum[i]);
        // Map so that -90 dB = bottom, 0 dB = center (0 dB line)
        const float normDb = juce::jmap(db, -90.0f, 0.0f, minDb, 0.0f);
        const float y = dbToY(normDb);

        if (!started)
        {
            spectrumPath.startNewSubPath(x, y);
            started = true;
        }
        else
        {
            spectrumPath.lineTo(x, y);
        }
    }

    if (started)
    {
        // Close path to bottom
        spectrumPath.lineTo(w, h);
        spectrumPath.lineTo(0.0f, h);
        spectrumPath.closeSubPath();

        g.setColour(juce::Colour(0xFF42A5F5).withAlpha(0.12f));
        g.fillPath(spectrumPath);

        // Outline
        juce::Path outlinePath;
        bool outlineStarted = false;
        for (int i = 1; i < currentSpectrumSize; ++i)
        {
            const float freq = (float)i * binWidth;
            if (freq < minFreq || freq > maxFreq)
                continue;
            const float x = freqToX(freq);
            const float db = juce::jlimit(-90.0f, 0.0f, smoothedSpectrum[i]);
            const float normDb = juce::jmap(db, -90.0f, 0.0f, minDb, 0.0f);
            const float y = dbToY(normDb);
            if (!outlineStarted) { outlinePath.startNewSubPath(x, y); outlineStarted = true; }
            else outlinePath.lineTo(x, y);
        }
        g.setColour(juce::Colour(0xFF42A5F5).withAlpha(0.25f));
        g.strokePath(outlinePath, juce::PathStrokeType(1.0f));
    }
}

// ── Per-band curves (subtle fills) ─────────────────────────────────
void ResponseCurveComponent::paintBandCurves(juce::Graphics& g)
{
    const float w = (float)getWidth();
    const float zeroY = dbToY(0.0f);

    for (int b = 0; b < 8; ++b)
    {
        const int idx = b + 1;
        const bool on = proc.apvts.getRawParameterValue(bandId(idx, "on"))->load() > 0.5f;
        if (!on) continue;

        const bool isSelected = (b == selectedBand);
        const float alpha = isSelected ? 0.15f : 0.06f;

        juce::Path fillPath;
        fillPath.startNewSubPath(0.0f, zeroY);

        for (int i = 0; i < numPoints; ++i)
        {
            const float x = (float)i / (float)(numPoints - 1) * w;
            const float y = dbToY(perBandMagnitudes[b][i]);
            fillPath.lineTo(x, y);
        }

        fillPath.lineTo(w, zeroY);
        fillPath.closeSubPath();

        g.setColour(getBandColour(b).withAlpha(alpha));
        g.fillPath(fillPath);
    }
}

// ── Composite response curve ───────────────────────────────────────
void ResponseCurveComponent::paintResponseCurve(juce::Graphics& g)
{
    const float w = (float)getWidth();

    juce::Path curvePath;
    for (int i = 0; i < numPoints; ++i)
    {
        const float x = (float)i / (float)(numPoints - 1) * w;
        const float y = dbToY(magnitudes[i]);

        if (i == 0)
            curvePath.startNewSubPath(x, y);
        else
            curvePath.lineTo(x, y);
    }

    g.setColour(juce::Colours::white.withAlpha(0.85f));
    g.strokePath(curvePath, juce::PathStrokeType(2.0f));
}

// ── Band nodes ─────────────────────────────────────────────────────
void ResponseCurveComponent::paintNodes(juce::Graphics& g)
{
    for (int b = 0; b < 8; ++b)
    {
        const int idx = b + 1;
        const bool on = proc.apvts.getRawParameterValue(bandId(idx, "on"))->load() > 0.5f;
        if (!on) continue;

        const float freq = proc.apvts.getRawParameterValue(bandId(idx, "freq"))->load();
        const float gain = proc.apvts.getRawParameterValue(bandId(idx, "gain"))->load()
                           * proc.apvts.getRawParameterValue("scale")->load();

        const float x = freqToX(freq);
        const float y = dbToY(gain);
        const float r = nodeRadius;

        const bool isSelected = (b == selectedBand);
        const bool isHovered  = (b == hoveredBand);

        auto colour = getBandColour(b);

        if (isSelected)
        {
            // Glow
            g.setColour(colour.withAlpha(0.25f));
            g.fillEllipse(x - r * 2.0f, y - r * 2.0f, r * 4.0f, r * 4.0f);
        }

        g.setColour(colour.withAlpha(isHovered || isSelected ? 1.0f : 0.7f));
        g.fillEllipse(x - r, y - r, r * 2.0f, r * 2.0f);

        g.setColour(juce::Colours::white.withAlpha(0.9f));
        g.drawEllipse(x - r, y - r, r * 2.0f, r * 2.0f, 1.5f);

        // Band number label
        g.setColour(juce::Colours::white);
        g.setFont(10.0f);
        g.drawText(juce::String(idx), (int)(x - r), (int)(y - r), (int)(r * 2.0f), (int)(r * 2.0f),
                   juce::Justification::centred);

        // Channel routing indicator (small letter below node)
        const int chIdx = (int) proc.apvts.getRawParameterValue(bandId(idx, "ch"))->load();
        if (chIdx > 0)
        {
            const int modeIdx = (int) proc.apvts.getRawParameterValue("proc_mode")->load();
            const char* labels[] = { "", "L", "R" };
            const char* msLabels[] = { "", "M", "S" };
            const char* label = (modeIdx == 1) ? msLabels[chIdx] : labels[chIdx];

            g.setColour(colour.withAlpha(0.9f));
            g.setFont(8.0f);
            g.drawText(label, (int)(x - r), (int)(y + r + 1), (int)(r * 2.0f), 10,
                       juce::Justification::centred);
        }
    }
}

// ── Hit-testing ────────────────────────────────────────────────────
int ResponseCurveComponent::hitTestNode(float mx, float my) const
{
    const float hitRadius = nodeRadius + 5.0f;

    for (int b = 0; b < 8; ++b)
    {
        const int idx = b + 1;
        const bool on = proc.apvts.getRawParameterValue(bandId(idx, "on"))->load() > 0.5f;
        if (!on) continue;

        const float freq = proc.apvts.getRawParameterValue(bandId(idx, "freq"))->load();
        const float gain = proc.apvts.getRawParameterValue(bandId(idx, "gain"))->load()
                           * proc.apvts.getRawParameterValue("scale")->load();

        const float nx = freqToX(freq);
        const float ny = dbToY(gain);

        const float dx = mx - nx;
        const float dy = my - ny;
        if (dx * dx + dy * dy <= hitRadius * hitRadius)
            return b;
    }

    return -1;
}

// ── Mouse interaction ──────────────────────────────────────────────
void ResponseCurveComponent::mouseDown(const juce::MouseEvent& e)
{
    const int hit = hitTestNode((float)e.x, (float)e.y);

    if (e.mods.isPopupMenu() && hit >= 0)
    {
        // Right-click context menu
        selectedBand = hit;
        const int idx = hit + 1;

        juce::PopupMenu menu;
        menu.addItem(1, "Enable/Disable Band " + juce::String(idx));
        menu.addSeparator();
        menu.addItem(10, "Bell");
        menu.addItem(11, "Low Shelf");
        menu.addItem(12, "High Shelf");
        menu.addItem(13, "High Pass");
        menu.addItem(14, "Low Pass");
        menu.addItem(15, "Bandpass");

        menu.showMenuAsync(juce::PopupMenu::Options(), [this, idx](int result)
        {
            if (result == 1)
            {
                auto* p = proc.apvts.getRawParameterValue(bandId(idx, "on"));
                auto* param = proc.apvts.getParameter(bandId(idx, "on"));
                if (param) param->setValueNotifyingHost(p->load() > 0.5f ? 0.0f : 1.0f);
            }
            else if (result >= 10 && result <= 15)
            {
                auto* param = proc.apvts.getParameter(bandId(idx, "type"));
                if (param)
                    param->setValueNotifyingHost(param->convertTo0to1((float)(result - 10)));
            }
        });
        return;
    }

    selectedBand = hit;
    if (hit >= 0)
    {
        dragging = true;
        shiftDrag = e.mods.isShiftDown();
        if (shiftDrag)
        {
            dragStartQ = proc.apvts.getRawParameterValue(bandId(hit + 1, "q"))->load();
            dragStartY = (float)e.y;
        }
    }
}

void ResponseCurveComponent::mouseDrag(const juce::MouseEvent& e)
{
    if (!dragging || selectedBand < 0) return;

    const int idx = selectedBand + 1;

    if (shiftDrag)
    {
        // Shift+drag: adjust Q (up = higher Q = narrower)
        const float deltaY = dragStartY - (float)e.y;
        const float qMultiplier = std::pow(2.0f, deltaY / 100.0f);
        const float newQ = std::clamp(dragStartQ * qMultiplier, 0.1f, 24.0f);

        auto* param = proc.apvts.getParameter(bandId(idx, "q"));
        if (param) param->setValueNotifyingHost(param->convertTo0to1(newQ));
    }
    else
    {
        // Normal drag: freq (X) and gain (Y)
        const float newFreq = std::clamp(xToFreq((float)e.x), minFreq, maxFreq);
        const float scale = proc.apvts.getRawParameterValue("scale")->load();
        float newGain = std::clamp(yToDb((float)e.y), minDb, maxDb);
        if (scale > 0.001f) newGain /= scale;
        newGain = std::clamp(newGain, -24.0f, 24.0f);

        auto* freqParam = proc.apvts.getParameter(bandId(idx, "freq"));
        auto* gainParam = proc.apvts.getParameter(bandId(idx, "gain"));
        if (freqParam) freqParam->setValueNotifyingHost(freqParam->convertTo0to1(newFreq));
        if (gainParam) gainParam->setValueNotifyingHost(gainParam->convertTo0to1(newGain));
    }
}

void ResponseCurveComponent::mouseUp(const juce::MouseEvent&)
{
    dragging = false;
    shiftDrag = false;
}

void ResponseCurveComponent::mouseMove(const juce::MouseEvent& e)
{
    const int hit = hitTestNode((float)e.x, (float)e.y);
    if (hit != hoveredBand)
    {
        hoveredBand = hit;
        setMouseCursor(hit >= 0 ? juce::MouseCursor::PointingHandCursor
                                : juce::MouseCursor::CrosshairCursor);
        repaint();
    }
}
