#pragma once

#include <juce_gui_extra/juce_gui_extra.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include "AudioDropTarget.h"

namespace cassette
{

class WaveformDisplay : public juce::Component,
                        private AudioDropForwarder
{
public:
    WaveformDisplay();

    void setBuffer(const juce::AudioBuffer<float>& buffer, double sampleRate);

    void paint(juce::Graphics& g) override;

private:
    juce::AudioBuffer<float> audio;
    double sampleRate = 48000.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(WaveformDisplay)
};

}
