#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

namespace cassette
{

class LoudnessMeter
{
public:
    struct Result
    {
        float integratedLufs = -100.0f;
        float shortTermMaxLufs = -100.0f;
        float shortTermMinLufs = 100.0f;
        float loudnessRangeLu = 0.0f;
    };

    static Result analyze(const juce::AudioBuffer<float>& buffer, double sampleRate);

    static float lufsFromMeanSquare(double meanSquare);
};

}
