#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

namespace cassette
{

class TruePeakMeter
{
public:
    static float measurePeakLinear(const juce::AudioBuffer<float>& buffer, double sampleRate);

    static float measurePeakDbfs(const juce::AudioBuffer<float>& buffer, double sampleRate);

    static float measureBlockPeakLinear(const float* samples, int numSamples);
};

}
