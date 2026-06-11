#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include "../../analysis/AudioFeatures.h"

namespace cassette
{

class MaskingNoiseGate
{
public:
    void process(juce::AudioBuffer<float>& buffer,
                 double sampleRate,
                 const AudioFeatures& features,
                 float intensity = 1.0f);

private:
    float envelope = 0.0f;
};

}
