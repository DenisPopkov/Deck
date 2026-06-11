#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace cassette
{

class StatusBar : public juce::Component
{
public:
    StatusBar();
    void paint(juce::Graphics& g) override;
    void resized() override;
    void setMessage(const juce::String& text);

    juce::ToggleButton tapeHissToggle { "Tape Hiss" };

private:
    juce::Label messageLabel;
    juce::Label cpuLabel { {}, "CPU --" };
    juce::Label srLabel { {}, "SR 44.1 kHz" };
};

}
