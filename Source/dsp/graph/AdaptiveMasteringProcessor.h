#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include "../../analysis/AudioFeatures.h"
#include "../../analysis/PerceptualQualityGuard.h"
#include "../CassetteProfile.h"
#include "../MasteringOptions.h"

namespace cassette
{

struct AdaptiveMasteringResult
{
    AudioFeatures featuresAfter;
    PerceptualQualityReport quality;
    MasteringOptions optionsUsed;
    juce::String autoPlanSummary;
    float tapeThreatScore = 0.0f;
    int iterations = 0;
};

class AdaptiveMasteringProcessor
{
public:
    static AdaptiveMasteringResult process(juce::AudioBuffer<float>& buffer,
                                           const CassetteProfile& profile,
                                           MasteringOptions options,
                                           double sampleRate);
};

}
