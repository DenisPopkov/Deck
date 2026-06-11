#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

namespace cassette
{

class TruePeakLimiter
{
public:
    void setThresholdDbfs(float dbfs) { thresholdDbfs = dbfs; }
    void prepare(double sampleRate);
    void process(juce::AudioBuffer<float>& buffer);

private:
    float thresholdDbfs = -1.0f;
    float gain = 1.0f;
    double sampleRate = 48000.0;
};

}
