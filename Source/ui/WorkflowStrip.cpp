#include "WorkflowStrip.h"
#include "UiTheme.h"

namespace cassette
{

void WorkflowStrip::setActiveStep(WorkflowStep step)
{
    active = step;
    repaint();
}

void WorkflowStrip::setStepComplete(WorkflowStep step, bool complete)
{
    switch (step)
    {
        case WorkflowStep::Import: importDone = complete; break;
        case WorkflowStep::TapeSetup: setupDone = complete; break;
        case WorkflowStep::Process: processDone = complete; break;
        case WorkflowStep::Review: reviewDone = complete; break;
        case WorkflowStep::Export: exportDone = complete; break;
    }
    repaint();
}

void WorkflowStrip::paint(juce::Graphics& g)
{
    const juce::String names[] = { "Import", "Tape Setup", "Process", "Review", "Export" };
    const bool done[] = { importDone, setupDone, processDone, reviewDone, exportDone };
    const WorkflowStep steps[] = { WorkflowStep::Import, WorkflowStep::TapeSetup, WorkflowStep::Process,
                                 WorkflowStep::Review, WorkflowStep::Export };

    auto area = getLocalBounds().reduced(4);
    g.setColour(ui::Theme::card());
    g.fillRoundedRectangle(area.toFloat(), 8.0f);

    const int n = 5;
    const float w = static_cast<float>(area.getWidth()) / static_cast<float>(n);

    for (int i = 0; i < n; ++i)
    {
        auto cell = juce::Rectangle<float>(area.getX() + i * w, static_cast<float>(area.getY()), w,
                                           static_cast<float>(area.getHeight()));
        const bool isActive = steps[i] == active;
        const bool isDone = done[i];

        g.setColour(isActive ? ui::Theme::accent().withAlpha(0.18f) : juce::Colours::transparentBlack);
        g.fillRoundedRectangle(cell.reduced(2.0f, 4.0f), 6.0f);

        g.setColour(isDone ? ui::Theme::okGreen() : (isActive ? ui::Theme::accent() : ui::Theme::textMuted()));
        g.setFont(ui::Theme::captionFont().boldened());
        g.drawText(names[i], cell.reduced(6.0f, 0.0f), juce::Justification::centred);

        if (i + 1 < n)
        {
            g.setColour(ui::Theme::cardBorder());
            g.drawLine(cell.getRight(), cell.getCentreY(), cell.getRight() + 4.0f, cell.getCentreY(), 1.0f);
        }
    }
}

}
