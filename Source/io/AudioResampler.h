#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

namespace cassette
{

void resampleBufferLinear(juce::AudioBuffer<float>& buffer, double srcRate, double dstRate);

}
