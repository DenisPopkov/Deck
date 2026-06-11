#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <optional>

namespace cassette
{

class EssentiaBridge
{
public:
    static bool isLinked();

    static std::optional<float> integratedLufs(const juce::AudioBuffer<float>& buffer, double sampleRate);
};

}
