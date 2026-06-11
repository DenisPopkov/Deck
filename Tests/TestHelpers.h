#pragma once

#include <cmath>
#include <iostream>
#include <string>
#include <vector>
#include <juce_audio_basics/juce_audio_basics.h>
#include "../Source/analysis/EssentiaAnalyzer.h"
#include "../Source/analysis/AudioFeatures.h"
#include "../Source/dsp/graph/MasteringGraph.h"
#include "../Source/dsp/graph/AdaptiveMasteringProcessor.h"
#include "../Source/analysis/PeaqEvaluator.h"
#include "../Source/dsp/dynamics/RoughnessDeEsser.h"
#include "../Source/dsp/CassetteProfile.h"
#include "../Source/dsp/MasteringOptions.h"
#include "../Source/dsp/stereo/MidSideProcessor.h"

namespace cassette::test
{

struct TestContext
{
    int failures = 0;

    void expectTrue(bool condition, const char* message)
    {
        if (!condition)
        {
            ++failures;
            std::cerr << "FAIL: " << message << '\n';
        }
        else
        {
            std::cout << "OK:   " << message << '\n';
        }
    }

    void expectNear(float actual, float expected, float tolerance, const char* message)
    {
        if (std::abs(actual - expected) > tolerance)
        {
            ++failures;
            std::cerr << "FAIL: " << message << " (got " << actual << ", expected " << expected
                      << " +/- " << tolerance << ")\n";
        }
        else
        {
            std::cout << "OK:   " << message << '\n';
        }
    }
};

inline juce::AudioBuffer<float> makeSineBuffer(double sampleRate,
                                               double durationSec,
                                               float frequencyHz,
                                               float peakAmplitude,
                                               int numChannels = 2)
{
    const int numSamples = static_cast<int>(sampleRate * durationSec);
    juce::AudioBuffer<float> buffer(numChannels, numSamples);

    for (int ch = 0; ch < numChannels; ++ch)
    {
        auto* data = buffer.getWritePointer(ch);
        for (int i = 0; i < numSamples; ++i)
        {
            const auto t = static_cast<double>(i) / sampleRate;
            data[i] = peakAmplitude * std::sin(2.0 * juce::MathConstants<double>::pi * frequencyHz * t);
        }
    }
    return buffer;
}

inline void applyGain(juce::AudioBuffer<float>& buffer, float gainLinear)
{
    buffer.applyGain(gainLinear);
}

inline AudioFeatures analyze(const juce::AudioBuffer<float>& buffer, double sampleRate)
{
    return EssentiaAnalyzer::extractFeatures(buffer, sampleRate);
}

inline AudioFeatures processAndAnalyze(juce::AudioBuffer<float>& buffer,
                                         double sampleRate,
                                         const CassetteProfile& profile,
                                         const MasteringOptions& options = {})
{
    const auto features = analyze(buffer, sampleRate);
    MasteringGraph graph;
    graph.prepare(sampleRate, buffer.getNumSamples());
    graph.process(buffer, profile, features, options);
    return analyze(buffer, sampleRate);
}

inline bool bufferIsFinite(const juce::AudioBuffer<float>& buffer)
{
    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
    {
        const auto* data = buffer.getReadPointer(ch);
        for (int i = 0; i < buffer.getNumSamples(); ++i)
            if (!std::isfinite(data[i]))
                return false;
    }
    return true;
}

inline float peakAbs(const juce::AudioBuffer<float>& buffer)
{
    return buffer.getMagnitude(0, buffer.getNumSamples());
}

inline float sideChannelRms(juce::AudioBuffer<float> buffer)
{
    MidSideProcessor::toMidSide(buffer);
    if (buffer.getNumChannels() < 2)
        return 0.0f;
    return buffer.getRMSLevel(1, 0, buffer.getNumSamples());
}

} // namespace cassette::test
