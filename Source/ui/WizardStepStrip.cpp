#include "WizardStepStrip.h"
#include "UiTheme.h"

namespace cassette
{

void WizardStepStrip::setPhase(WizardPhase phase)
{
    active = phase;
    repaint();
}

void WizardStepStrip::setStepDone(WizardPhase phase, bool done)
{
    switch (phase)
    {
        case WizardPhase::AddMusic: musicDone = done; break;
        case WizardPhase::EditTracks: editDone = done; break;
        case WizardPhase::Preparing: prepareDone = done; break;
        case WizardPhase::ReadyToExport: exportDone = done; break;
    }
    repaint();
}

void WizardStepStrip::paint(juce::Graphics& g)
{
    using namespace ui;

    const juce::String titles[] = { "Add music", "Edit tracks", "Prepare", "Export" };
    const bool done[] = { musicDone, editDone, prepareDone, exportDone };
    const WizardPhase phases[] = { WizardPhase::AddMusic,
                                   WizardPhase::EditTracks,
                                   WizardPhase::Preparing,
                                   WizardPhase::ReadyToExport };

    auto area = getLocalBounds().reduced(12, 4);
    const int lineY = area.getCentreY();

    g.setColour(Theme::borderLight());
    g.drawHorizontalLine(lineY, static_cast<float>(area.getX() + 28), static_cast<float>(area.getRight() - 28));

    const int n = 4;
    const int cellW = area.getWidth() / n;

    for (int i = 0; i < n; ++i)
    {
        const int cx = area.getX() + cellW * i + cellW / 2;
        const bool isActive = phases[i] == active;
        const bool isDone = done[i];

        const int dotR = 11;
        juce::Rectangle<int> dot(cx - dotR, lineY - dotR, dotR * 2, dotR * 2);

        g.setColour(isDone || isActive ? Theme::accent() : Theme::buttonDisabledFill());
        g.fillEllipse(dot.toFloat());
        g.setColour(isActive || isDone ? Theme::border() : Theme::buttonDisabledBorder());
        g.drawEllipse(dot.toFloat(), 1.0f);

        Theme::drawCentredText(g,
                               juce::String(i + 1),
                               dot,
                               Theme::buttonFontSmall().boldened(),
                               isDone || isActive ? juce::Colours::white : Theme::buttonDisabledText());

        juce::Rectangle<int> titleArea(area.getX() + i * cellW, lineY + dotR + 4, cellW, area.getBottom() - lineY - dotR - 4);
        g.setColour(isDone || isActive ? Theme::accent() : Theme::textMuted());
        g.setFont(Theme::captionFont());
        g.drawText(titles[i], titleArea, juce::Justification::centredTop);
    }
}

}
