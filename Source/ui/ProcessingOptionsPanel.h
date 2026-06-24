#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>
#include "../dsp/MasteringOptions.h"

namespace cassette
{

class ProcessingOptionsPanel : public juce::Component,
                               private juce::Button::Listener
{
public:
    std::function<void()> onChanged;

    ProcessingOptionsPanel();

    void setOptions(const MasteringOptions& options);
    MasteringOptions getOptions() const;

    void setInteractionEnabled(bool enabled);

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    void buttonClicked(juce::Button* b) override;
    void notifyChanged();

    MasteringOptions options;
    juce::Label sectionLabel { {}, "Processing" };
    juce::ToggleButton truePeakLimiterToggle { "True peak limiter" };
    juce::ToggleButton stereoTighteningToggle { "Stereo tightening" };
};

}
