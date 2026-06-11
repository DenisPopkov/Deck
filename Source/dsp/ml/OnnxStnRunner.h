#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include "../CassetteProfile.h"

namespace cassette
{

class OnnxStnRunner
{
public:
    OnnxStnRunner();
    ~OnnxStnRunner();

    bool tryLoadDefaultModel();
    bool isLoaded() const { return loaded; }

    void reset();
    void process(juce::AudioBuffer<float>& buffer, const CassetteProfile& profile, double sampleRate);

private:
    bool loaded = false;
    bool profileConditioned = false;
    float stateL = 0.0f;
    float stateR = 0.0f;
    struct Impl;
    Impl* impl = nullptr;

    juce::File resolveModelPath() const;
    static float driveNorm(const CassetteProfile& profile);
    static float biasNorm(const CassetteProfile& profile);
};

}
