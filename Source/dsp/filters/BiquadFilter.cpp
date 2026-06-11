#include "BiquadFilter.h"

namespace cassette
{

BiquadFilter::BiquadFilter(float cutoffHz, float q, FilterType type, double sampleRate)
    : filterType(type), cutoff(cutoffHz), qFactor(q)
{
    prepare(sampleRate, 512, 2);
}

void BiquadFilter::prepare(double sampleRate, int maxBlockSize, int numChannels)
{
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = static_cast<juce::uint32>(juce::jmax(1, maxBlockSize));
    spec.numChannels = static_cast<juce::uint32>(juce::jmax(1, numChannels));
    iir.prepare(spec);
    prepared = true;
    updateCoefficients();
}

void BiquadFilter::setCutoff(float cutoffHz)
{
    cutoff = cutoffHz;
    updateCoefficients();
}

void BiquadFilter::updateCoefficients()
{
    if (!prepared)
        return;

    juce::dsp::IIR::Coefficients<float>::Ptr coeffs;
    switch (filterType)
    {
        case FilterType::HighPass:
            coeffs = juce::dsp::IIR::Coefficients<float>::makeHighPass(spec.sampleRate, cutoff, qFactor);
            break;
        case FilterType::LowPass:
            coeffs = juce::dsp::IIR::Coefficients<float>::makeLowPass(spec.sampleRate, cutoff, qFactor);
            break;
    }
    *iir.state = *coeffs;
}

void BiquadFilter::process(juce::AudioBuffer<float>& buffer)
{
    if (!prepared || buffer.getNumSamples() == 0 || buffer.getNumChannels() == 0)
        return;

    const auto channels = static_cast<juce::uint32>(buffer.getNumChannels());
    const auto blockSize = static_cast<juce::uint32>(buffer.getNumSamples());

    if (spec.numChannels != channels || spec.maximumBlockSize < blockSize)
    {
        spec.numChannels = channels;
        spec.maximumBlockSize = juce::jmax(spec.maximumBlockSize, blockSize);
        iir.prepare(spec);
        updateCoefficients();
    }

    juce::dsp::AudioBlock<float> block(buffer);
    juce::dsp::ProcessContextReplacing<float> ctx(block);
    iir.process(ctx);
}

}
