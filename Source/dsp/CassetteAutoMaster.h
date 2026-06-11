#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include "CassetteProfile.h"
#include "MasteringOptions.h"
#include "../analysis/AudioFeatures.h"

namespace cassette
{

class CassetteAutoMaster
{
public:
    void prepare(double sampleRate, int maxBlockSize);
    void processTrack(juce::AudioBuffer<float>& buffer,
                      const CassetteProfile& profile,
                      const AudioFeatures& features,
                      const MasteringOptions& options = {});

    static float calculateGainForTargetLUFS(float currentLUFS, float targetLUFS);
    static void applyGain(juce::AudioBuffer<float>& buffer, float gainDb);

private:
    double sampleRate = 48000.0;
    int maxBlock = 512;
};

}
