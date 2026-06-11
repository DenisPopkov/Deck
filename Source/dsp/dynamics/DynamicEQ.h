#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include "../filters/BiquadFilter.h"

namespace cassette
{

class DynamicEQ
{
public:
    DynamicEQ(float lowBandHz, float highBandHz, float thresholdDb, float ratio);

    void prepare(double sampleRate, int maxBlockSize);
    void setThreshold(float thresholdDb);
    void setRatio(float ratio);
    void process(juce::AudioBuffer<float>& buffer);

private:
    float lowHz;
    float highHz;
    float thresholdDb;
    float ratio;
    BiquadFilter bandFilter;
    float envelope = 0.0f;
    double sampleRate = 48000.0;
};

}
