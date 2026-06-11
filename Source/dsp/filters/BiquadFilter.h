#pragma once

#include <juce_dsp/juce_dsp.h>

namespace cassette
{

enum class FilterType
{
    HighPass,
    LowPass
};

class BiquadFilter
{
public:
    BiquadFilter() = default;
    BiquadFilter(float cutoffHz, float q, FilterType type, double sampleRate = 48000.0);

    void prepare(double sampleRate, int maxBlockSize, int numChannels = 2);
    void setCutoff(float cutoffHz);
    void process(juce::AudioBuffer<float>& buffer);

private:
    using IIRDuplicator = juce::dsp::ProcessorDuplicator<juce::dsp::IIR::Filter<float>,
                                                         juce::dsp::IIR::Coefficients<float>>;

    FilterType filterType = FilterType::HighPass;
    float cutoff = 1000.0f;
    float qFactor = 0.707f;
    IIRDuplicator iir;
    juce::dsp::ProcessSpec spec {};
    bool prepared = false;

    void updateCoefficients();
};

}
