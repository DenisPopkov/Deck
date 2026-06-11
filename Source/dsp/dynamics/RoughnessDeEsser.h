#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include "../../analysis/PsychoacousticMetrics.h"

namespace cassette
{

class RoughnessDeEsser
{
public:
    void process(juce::AudioBuffer<float>& buffer,
                 const PsychoacousticMetrics& metrics,
                 double sampleRate,
                 float intensity);

private:
    void processBand(juce::AudioBuffer<float>& buffer,
                     float centerHz,
                     float modulationIndex,
                     double sampleRate,
                     float intensity);
};

}
