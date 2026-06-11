#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace cassette
{

enum class WorkflowStep
{
    Import,
    TapeSetup,
    Process,
    Review,
    Export
};

class WorkflowStrip : public juce::Component
{
public:
    void setActiveStep(WorkflowStep step);
    void setStepComplete(WorkflowStep step, bool complete);

    void paint(juce::Graphics& g) override;

private:
    WorkflowStep active = WorkflowStep::Import;
    bool importDone = false;
    bool setupDone = false;
    bool processDone = false;
    bool reviewDone = false;
    bool exportDone = false;
};

}
