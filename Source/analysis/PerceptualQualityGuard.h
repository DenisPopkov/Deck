#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include "PsychoacousticMetrics.h"
#include "PeaqEvaluator.h"

namespace cassette
{

struct PerceptualQualityReport
{
    float noiseToMaskRatioDb = 0.0f;
    float sharpnessDeltaAcum = 0.0f;
    float roughnessDeltaAsper = 0.0f;
    float fluctuationDeltaVacil = 0.0f;
    float bandwidthLossPercent = 0.0f;
    float modDiffDb = 0.0f;
    float objectiveDifferenceGrade = 0.0f;
    bool passesQualityGate = true;
    int fallbackIterations = 0;
    float hfTamerIntensityUsed = 1.0f;
    bool usedMinimalFallback = false;
    PeaqBackend peaqBackend = PeaqBackend::Heuristic;
    juce::String peaqDetail;

    static PerceptualQualityReport compare(const juce::AudioBuffer<float>& reference,
                                           const juce::AudioBuffer<float>& processed,
                                           double sampleRate);
};

}
