#pragma once

#include <juce_core/juce_core.h>
#include "PsychoacousticMetrics.h"
#include "WowFlutterAnalyzer.h"

namespace cassette
{

struct AudioFeatures
{
    float integratedLUFS = -14.0f;
    float shortTermMaxLUFS = -14.0f;
    float shortTermMinLUFS = -14.0f;
    float loudnessRangeDb = 0.0f;

    float truePeakDbfs = 0.0f;
    float samplePeakDbfs = 0.0f;
    float rmsLevelDb = -18.0f;
    float crestFactorDb = 12.0f;
    float dynamicRangeDb = 0.0f;

    float lfEnergyRatio = 0.0f;
    float mfEnergyRatio = 0.0f;
    float hfEnergyRatio = 0.0f;

    float stereoCorrelation = 1.0f;
    float lfStereoCorrelation = 1.0f;
    float stereoWidthPercent = 0.0f;

    float noiseFloorDbfs = -90.0f;
    float clippingPercent = 0.0f;
    float dcOffsetDbfs = -100.0f;

    PsychoacousticMetrics psycho;

    WowFlutterMetrics wowFlutter;

    juce::String toDisplayString() const;
};

}
