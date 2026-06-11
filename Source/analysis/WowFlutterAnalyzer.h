#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

namespace cassette
{

struct WowFlutterMetrics
{
    float wowPercent = 0.0f;

    float flutterPercent = 0.0f;

    float wowFlutterIndex = 0.0f;

    bool hasTestTone3150 = false;

    float testToneWowPercent = 0.0f;
};

class WowFlutterAnalyzer
{
public:
    static WowFlutterMetrics analyze(const juce::AudioBuffer<float>& buffer, double sampleRate);
};

}
