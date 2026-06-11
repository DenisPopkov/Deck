#include "CompareWaveformDisplay.h"
#include "UiTheme.h"

namespace cassette
{
namespace
{
constexpr int kHeaderHeight = 44;
}

CompareWaveformDisplay::CompareWaveformDisplay()
{
    initAudioDropForwarding(this);
    onDragHoverChanged = [this](DropPayloadKind kind) { setDragHighlight(kind != DropPayloadKind::None, kind); };
}

namespace
{
constexpr int kMaxWaveformPoints = 4096;

juce::AudioBuffer<float> decimatePeaks(const juce::AudioBuffer<float>& src, int maxPoints = kMaxWaveformPoints)
{
    const int n = src.getNumSamples();
    const int ch = src.getNumChannels();
    if (n == 0 || ch == 0)
        return {};

    if (n <= maxPoints)
    {
        juce::AudioBuffer<float> copy;
        copy.makeCopyOf(src);
        return copy;
    }

    juce::AudioBuffer<float> out(ch, maxPoints);
    for (int p = 0; p < maxPoints; ++p)
    {
        const int start = static_cast<int>((static_cast<juce::int64>(p) * n) / maxPoints);
        const int end = static_cast<int>((static_cast<juce::int64>(p + 1) * n) / maxPoints);
        for (int c = 0; c < ch; ++c)
        {
            float peak = 0.0f;
            for (int i = start; i < end && i < n; ++i)
                peak = juce::jmax(peak, std::abs(src.getSample(c, i)));
            out.setSample(c, p, peak);
        }
    }
    return out;
}
}

void CompareWaveformDisplay::setBeforeBuffer(const juce::AudioBuffer<float>& buffer, double sr)
{
    before = decimatePeaks(buffer);
    sampleRate = sr;
    hasBefore = buffer.getNumSamples() > 0;
    repaint();
}

void CompareWaveformDisplay::setAfterBuffer(const juce::AudioBuffer<float>& buffer, double sr)
{
    after = decimatePeaks(buffer);
    hasAfter = buffer.getNumSamples() > 0;
    repaint();
}

void CompareWaveformDisplay::clearAfter()
{
    after.setSize(0, 0);
    hasAfter = false;
    repaint();
}

void CompareWaveformDisplay::clearAll()
{
    before.setSize(0, 0);
    after.setSize(0, 0);
    hasBefore = false;
    hasAfter = false;
    repaint();
}

void CompareWaveformDisplay::setBeforeInfo(const WaveformCardInfo& info)
{
    beforeInfo = info;
    repaint();
}

void CompareWaveformDisplay::setAfterInfo(const WaveformCardInfo& info)
{
    afterInfo = info;
    repaint();
}

void CompareWaveformDisplay::setShowEmptyDropZone(bool show)
{
    showEmptyDropZone = show;
    repaint();
}

void CompareWaveformDisplay::setDragHighlight(bool active, DropPayloadKind kind)
{
    dragHighlight = active;
    dropKind = kind;
    repaint();
}

void CompareWaveformDisplay::mouseDown(const juce::MouseEvent& e)
{
    if (onPreviewSideClicked != nullptr)
        onPreviewSideClicked(e.x > getWidth() / 2);
}

void CompareWaveformDisplay::drawEmptyDropZone(juce::Graphics& g, juce::Rectangle<int> area) const
{
    ui::Theme::drawPanel(g, area.reduced(2));

    auto panel = area.reduced(12);
    g.setColour(ui::Theme::textPrimary());
    g.setFont(ui::Theme::sectionFont());
    g.drawFittedText("Drop WAV / FLAC / AIFF here", panel.removeFromTop(panel.getHeight() / 2),
                     juce::Justification::centred, 1);

    g.setColour(ui::Theme::textMuted());
    g.setFont(ui::Theme::bodyFont());
    g.drawFittedText("or use Import on the left", panel, juce::Justification::centred, 2);
}

void CompareWaveformDisplay::drawDragOverlay(juce::Graphics& g) const
{
    auto bounds = getLocalBounds();
    g.setColour(ui::Theme::accent().withAlpha(0.10f));
    g.fillRect(bounds.reduced(2));
    g.setColour(ui::Theme::accent());
    g.drawRect(bounds.reduced(2), 2);

    juce::String headline = "Drop audio file to import";
    juce::String detail = "WAV, FLAC, AIFF, OGG";
    if (dropKind == DropPayloadKind::Folder)
    {
        headline = "Drop folder for mixtape";
        detail = "Natural sort, Side A/B split";
    }

    g.setColour(ui::Theme::textPrimary());
    g.setFont(ui::Theme::sectionFont());
    g.drawFittedText(headline, getLocalBounds().reduced(24).removeFromTop(getHeight() / 2),
                     juce::Justification::centred, 1);

    g.setColour(ui::Theme::textSecondary());
    g.setFont(ui::Theme::bodyFont());
    g.drawFittedText(detail, getLocalBounds().reduced(24), juce::Justification::centred, 2);
}

void CompareWaveformDisplay::drawHalfWaveform(juce::Graphics& g,
                                              juce::Rectangle<int> area,
                                              const juce::AudioBuffer<float>& buffer,
                                              juce::Colour colour,
                                              const WaveformCardInfo& info,
                                              bool processed) const
{
    auto panel = area.reduced(2);
    ui::Theme::drawPanel(g, panel);

    auto header = panel.removeFromTop(kHeaderHeight).reduced(14, 10);
    g.setColour(ui::Theme::textPrimary());
    g.setFont(ui::Theme::sectionFont());
    g.drawText(info.title, header.removeFromTop(16), juce::Justification::centredLeft);

    if (info.subtitle.isNotEmpty())
    {
        g.setColour(ui::Theme::textSecondary());
        g.setFont(ui::Theme::metricFont());
        g.drawText(info.subtitle, header, juce::Justification::centredLeft);
    }

    auto waveArea = panel.reduced(12, 10);

    if (buffer.getNumSamples() == 0)
    {
        if (processed)
        {
            g.setColour(ui::Theme::textMuted());
            g.setFont(ui::Theme::captionFont());
            g.drawFittedText("Prepared output appears here",
                             waveArea,
                             juce::Justification::centred,
                             2);
        }
        return;
    }

    auto r = waveArea.toFloat();
    const int n = buffer.getNumSamples();
    juce::Path p;
    p.startNewSubPath(r.getX(), r.getCentreY());

    for (int x = 0; x < static_cast<int>(r.getWidth()); ++x)
    {
        const int i = static_cast<int>((static_cast<double>(x) / r.getWidth()) * n);
        const float s = buffer.getSample(0, juce::jlimit(0, n - 1, i));
        p.lineTo(r.getX() + static_cast<float>(x), r.getCentreY() - s * r.getHeight() * 0.42f);
    }

    g.setColour(colour.withAlpha(0.25f));
    g.strokePath(p, juce::PathStrokeType(2.4f));
    g.setColour(colour);
    g.strokePath(p, juce::PathStrokeType(1.2f));
}

void CompareWaveformDisplay::paint(juce::Graphics& g)
{
    g.fillAll(ui::Theme::background());

    if (!hasBefore && showEmptyDropZone)
    {
        drawEmptyDropZone(g, getLocalBounds());
        if (dragHighlight)
            drawDragOverlay(g);
        return;
    }

    auto bounds = getLocalBounds().reduced(4);
    auto left = bounds.removeFromLeft(bounds.getWidth() / 2);
    bounds.removeFromLeft(6);
    auto right = bounds;

    drawHalfWaveform(g, left, before, ui::Theme::trackBlue(), beforeInfo, false);
    drawHalfWaveform(g,
                     right,
                     hasAfter ? after : juce::AudioBuffer<float>(),
                     hasAfter ? ui::Theme::trackGreen() : ui::Theme::trackGrey().withAlpha(0.55f),
                     hasAfter ? afterInfo : WaveformCardInfo { "After", {}, false },
                     hasAfter);

    if (dragHighlight)
        drawDragOverlay(g);
}

}
