#pragma once

#include <juce_gui_extra/juce_gui_extra.h>
#include "../analysis/AudioFeatures.h"
#include "../dsp/CassetteProfile.h"
#include "AudioDropTarget.h"

namespace cassette
{

class MeterPanel : public juce::Component,
                   public AudioDropForwarder
{
public:
    MeterPanel();

    void setFeatures(const AudioFeatures& f,
                     const juce::String& title,
                     TapeFormulation tape = TapeFormulation::TypeIV);
    void setBaselineFeatures(const AudioFeatures& baseline);
    void clearBaselineFeatures();
    void clearFeatures();

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    bool hasFeatures = false;
    bool hasBaseline = false;
    AudioFeatures features;
    AudioFeatures baselineFeatures;
    juce::String panelTitle { "Analysis" };
    TapeFormulation tapeType = TapeFormulation::TypeIV;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MeterPanel)
};

}
