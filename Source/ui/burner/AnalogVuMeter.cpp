#include "AnalogVuMeter.h"
#include "../look/CassetteBurnerLook.h"

namespace cassette
{

AnalogVuMeter::AnalogVuMeter() { startTimerHz(30); }

void AnalogVuMeter::setLevelDb(float left, float right)
{
    leftDb = left;
    rightDb = right;
}

void AnalogVuMeter::timerCallback()
{
    const float smooth = 0.35f;
    displayLeft += smooth * (leftDb - displayLeft);
    displayRight += smooth * (rightDb - displayRight);
    repaint();
}

void AnalogVuMeter::paint(juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced(4.0f);
    g.setColour(CassetteBurnerLook::panel().darker(0.3f));
    g.fillRoundedRectangle(r, 6.0f);

    const auto drawNeedle = [&](float cx, float levelDb) {
        const auto angle = juce::jmap(juce::jlimit(-40.0f, 6.0f, levelDb), -40.0f, 6.0f, -2.4f, 2.4f);
        juce::Path p;
        p.startNewSubPath(cx, r.getBottom() - 8.0f);
        p.lineTo(cx + std::sin(angle) * (r.getHeight() - 20.0f),
                 r.getBottom() - 8.0f - std::cos(angle) * (r.getHeight() - 20.0f));
        g.setColour(CassetteBurnerLook::accent());
        g.strokePath(p, juce::PathStrokeType(2.0f));
    };

    drawNeedle(r.getCentreX() - 18.0f, displayLeft);
    drawNeedle(r.getCentreX() + 18.0f, displayRight);
    g.setColour(CassetteBurnerLook::textPrimary().withAlpha(0.5f));
    g.setFont(10.0f);
    g.drawText("VU", r.getCentreX() - 10.0f, r.getBottom() - 18.0f, 20, 12, juce::Justification::centred);
}

}
