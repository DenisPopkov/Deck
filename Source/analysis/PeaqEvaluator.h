#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

namespace cassette
{

enum class PeaqBackend
{
    Heuristic,
    Basic,
    GStreamer
};

struct PeaqEvaluation
{
    float objectiveDifferenceGrade = 0.0f;
    PeaqBackend backend = PeaqBackend::Heuristic;
    bool success = false;
    juce::String detail;
};

class PeaqEvaluator
{
public:
    static PeaqEvaluation evaluate(const juce::AudioBuffer<float>& reference,
                                   const juce::AudioBuffer<float>& processed,
                                   double sampleRate);

    static bool isGStreamerPeaqAvailable();
};

}
