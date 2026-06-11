#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace cassette
{

enum class WizardPhase
{
    AddMusic,
    Preparing,
    ReadyToExport
};

class WizardStepStrip : public juce::Component
{
public:
    void setPhase(WizardPhase phase);
    void setStepDone(WizardPhase phase, bool done);

    void paint(juce::Graphics& g) override;

private:
    WizardPhase active = WizardPhase::AddMusic;
    bool musicDone = false;
    bool prepareDone = false;
    bool exportDone = false;
};

}
