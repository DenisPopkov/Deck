#pragma once

namespace cassette
{

struct MasteringOptions
{
    bool maximumDigital = true;
    float hfTamerIntensity = 1.0f;
    bool perceptualAutoFallback = true;
    bool skipQualityCompare = false;
    bool reducePerceivedPitch = false;
    float perceivedPitchReductionPercent = 0.0f;
    bool enableTruePeakLimiter = true;
    bool enableStereoTightening = true;
};

}
