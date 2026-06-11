#pragma once

#include <array>
#include <juce_audio_basics/juce_audio_basics.h>

namespace cassette
{

struct PsychoacousticMetrics
{
    static constexpr int kBarkBands = 24;
    static constexpr int kModulationBands = 47;

    std::array<float, kBarkBands> barkSpecificLoudness {};

    float roughnessAsper = 0.0f;

    float fluctuationVacil = 0.0f;

    float streamingRingingIndex = 0.0f;

    std::array<float, kModulationBands> bandModulationIndex {};

    float sharpnessAcum = 0.0f;

    float tonalityIndex = 0.0f;

    float hfAboveMaskingDb = 0.0f;

    float hfTamerStrength = 0.0f;

    static float hzToBark(float hz);
    static float barkToHz(float bark);
    static float erbBandwidthHz(float centerHz);

    static PsychoacousticMetrics analyze(const juce::AudioBuffer<float>& buffer, double sampleRate);
};

}
