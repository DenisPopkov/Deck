#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <functional>

namespace cassette
{

class MidSideProcessor
{
public:
    static void toMidSide(juce::AudioBuffer<float>& buffer);
    static void fromMidSide(juce::AudioBuffer<float>& buffer);

    void process(juce::AudioBuffer<float>& buffer, const std::function<void(juce::AudioBuffer<float>& midSide)>& fn);
};

}
