#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <functional>
#include "MixtapeProject.h"

namespace cassette
{

struct RenderResult
{
    bool success = false;
    juce::String error;
    juce::AudioBuffer<float> buffer;
    juce::AudioBuffer<float> referenceBuffer;
    double sampleRate = 48000.0;
};

class SideRenderer
{
public:
    using ProgressCallback = std::function<void(int clipIndex, int clipCount, const juce::String& title)>;

    static RenderResult renderSide(const MixtapeProject& project,
                                   bool sideB,
                                   double sampleRate = 48000.0,
                                   bool padToMaxTapeLength = true,
                                   ProgressCallback onProgress = {},
                                   bool captureUnmasteredReference = false);
};

}
