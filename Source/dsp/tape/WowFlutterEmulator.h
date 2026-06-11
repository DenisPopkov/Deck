#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include "../CassetteProfile.h"

namespace cassette
{

class WowFlutterEmulator
{
public:
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

    juce::AudioBuffer<float> delayBuffer;
    int writePos = 0;
    int delayLen = 0;

    float readInterpolated(int channel, float delaySamples) const;
};

}
