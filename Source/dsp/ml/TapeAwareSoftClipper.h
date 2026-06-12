#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include "../CassetteProfile.h"

namespace cassette
{

class TapeAwareSoftClipper
{
public:
    void prepare(double sampleRate, int oversamplingFactor = 4);
    void setProfile(const CassetteProfile& profile);
    void process(juce::AudioBuffer<float>& buffer);

private:
    CassetteProfile profile;
    double sampleRate = 48000.0;
    int osFactor = 4;
    float state = 0.0f;

    float processSampleStn(float x, float drive) const;
    float tanhSaturation(float x, float drive) const;
};

/** Load thread-local ONNX session on the calling thread (for parallel workers). */
void warmThreadLocalOnnx();

}
