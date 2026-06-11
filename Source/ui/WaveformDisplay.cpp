#include "WaveformDisplay.h"

namespace cassette
{

WaveformDisplay::WaveformDisplay()
{
    initAudioDropForwarding(this);
}

void WaveformDisplay::setBuffer(const juce::AudioBuffer<float>& buffer, double sr)
{
    audio = buffer;
    sampleRate = sr;
    juce::ignoreUnused(sampleRate);
    repaint();
}

void WaveformDisplay::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colours::black);

    auto r = getLocalBounds().toFloat().reduced(10.0f);
    g.setColour(juce::Colours::white.withAlpha(0.08f));
    g.drawRect(r);

    if (audio.getNumSamples() == 0)
    {
        g.setColour(juce::Colours::white.withAlpha(0.6f));
        g.drawFittedText("Drop audio here", getLocalBounds(), juce::Justification::centred, 1);
        return;
    }

    const int n = audio.getNumSamples();
    const int ch = 0;

    juce::Path p;
    p.startNewSubPath(r.getX(), r.getCentreY());

    for (int x = 0; x < static_cast<int>(r.getWidth()); ++x)
    {
        const int i = static_cast<int>((static_cast<double>(x) / r.getWidth()) * n);
        const float s = audio.getSample(ch, juce::jlimit(0, n - 1, i));
        const float y = r.getCentreY() - (s * r.getHeight() * 0.45f);
        p.lineTo(r.getX() + static_cast<float>(x), y);
    }

    g.setColour(juce::Colours::cyan.withAlpha(0.9f));
    g.strokePath(p, juce::PathStrokeType(1.2f));
}

}
