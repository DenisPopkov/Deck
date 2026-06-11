#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include "../CassetteProfile.h"

namespace cassette
{

class StnGridModel
{
public:
    void reset();
    void process(juce::AudioBuffer<float>& buffer, const CassetteProfile& profile, double sampleRate);

private:
    float stateL = 0.0f;
    float stateR = 0.0f;

    float processSample(float x, float& state, float alpha, float beta, float mix, float drive) const;
};

}
