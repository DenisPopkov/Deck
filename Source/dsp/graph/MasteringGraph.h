#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include "../CassetteProfile.h"
#include "../MasteringOptions.h"
#include "../../analysis/AudioFeatures.h"

namespace cassette
{

class MasteringGraph
{
public:
    void prepare(double sampleRate, int maxBlockSize);
    void process(juce::AudioBuffer<float>& buffer,
                 const CassetteProfile& profile,
                 const AudioFeatures& features,
                 const MasteringOptions& options = {});

private:
    double sampleRate = 48000.0;
};

}
