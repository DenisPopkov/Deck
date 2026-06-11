#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

namespace cassette
{

class SpectrumAnalyzer
{
public:
    struct BandRatios
    {
        float lf = 0.0f;
        float mf = 0.0f;
        float hf = 0.0f;
    };

    static BandRatios bandEnergyRatios(const juce::AudioBuffer<float>& buffer, double sampleRate);
};

}
