#pragma once

#include <juce_audio_formats/juce_audio_formats.h>

namespace cassette
{

class WavExporter
{
public:
    static bool writeWav32Float(const juce::File& file, const juce::AudioBuffer<float>& buffer, double sampleRate);
};

}
