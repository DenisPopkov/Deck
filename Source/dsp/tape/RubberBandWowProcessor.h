#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <memory>
#include "../CassetteProfile.h"
#include "WowFlutterEmulator.h"

namespace cassette
{

class RubberBandWowProcessor
{
public:
    RubberBandWowProcessor();
    ~RubberBandWowProcessor();

    void prepare(double sampleRate, int maxBlockSize, int numChannels);
    void setProfile(const CassetteProfile& profile);
    void process(juce::AudioBuffer<float>& buffer);

private:
    double sampleRate = 48000.0;
    int maxBlock = 512;
    int channels = 2;
    float wowDepth = 0.0035f;
    float flutterDepth = 0.0012f;
    float wowRateHz = 0.75f;
    float flutterRateHz = 11.0f;
    float wowPhase = 0.0f;
    float flutterPhase = 0.0f;

    WowFlutterEmulator fallback;

#if defined(CASSETTE_HAS_RUBBERBAND)
    struct RubberBandState;
    std::unique_ptr<RubberBandState> rb;
    void processRubberBand(juce::AudioBuffer<float>& buffer);
#endif
};

}
