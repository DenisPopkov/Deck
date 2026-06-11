#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace cassette
{

class AnalogVuMeter : public juce::Component,
                      private juce::Timer
{
public:
    AnalogVuMeter();
    void setLevelDb(float leftDb, float rightDb);

private:
    void paint(juce::Graphics& g) override;
    void timerCallback() override;

    float leftDb = -60.0f;
    float rightDb = -60.0f;
    float displayLeft = -60.0f;
    float displayRight = -60.0f;
};

}
