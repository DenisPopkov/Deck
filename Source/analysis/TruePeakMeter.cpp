#include "TruePeakMeter.h"
#include <juce_dsp/juce_dsp.h>
#include <cmath>

namespace cassette
{

namespace
{

float cubicHermite(float y0, float y1, float y2, float y3, float t)
{
    const float a = -0.5f * y0 + 1.5f * y1 - 1.5f * y2 + 0.5f * y3;
    const float b = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3;
    const float c = -0.5f * y0 + 0.5f * y2;
    const float d = y1;
    return ((a * t + b) * t + c) * t + d;
}

float channelCubicPeak(const float* data, int numSamples)
{
    if (numSamples <= 0)
        return 0.0f;

    float peak = 0.0f;
    for (int i = 0; i < numSamples; ++i)
        peak = juce::jmax(peak, std::abs(data[i]));

    if (numSamples < 2)
        return peak;

    for (int i = 0; i < numSamples - 1; ++i)
    {
        const float y0 = i > 0 ? data[i - 1] : data[i];
        const float y1 = data[i];
        const float y2 = data[i + 1];
        const float y3 = i + 2 < numSamples ? data[i + 2] : data[i + 1];

        for (int k = 1; k < 4; ++k)
        {
            const float t = static_cast<float>(k) * 0.25f;
            peak = juce::jmax(peak, std::abs(cubicHermite(y0, y1, y2, y3, t)));
        }
    }

    return peak;
}

float oversampledPeak(const juce::AudioBuffer<float>& buffer, double sampleRate)
{
    if (buffer.getNumSamples() == 0 || buffer.getNumChannels() == 0)
        return 0.0f;

    const int numChannels = buffer.getNumChannels();
    const int numSamples = buffer.getNumSamples();

    juce::dsp::Oversampling<float> oversampler(
        static_cast<size_t>(numChannels),
        2,
        juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR,
        true,
        false);

    oversampler.initProcessing(static_cast<size_t>(numSamples));
    oversampler.reset();

    juce::dsp::AudioBlock<const float> inBlock(buffer);
    auto upBlock = oversampler.processSamplesUp(inBlock);

    float peak = 0.0f;
    for (size_t ch = 0; ch < upBlock.getNumChannels(); ++ch)
    {
        const auto* data = upBlock.getChannelPointer(ch);
        for (size_t i = 0; i < upBlock.getNumSamples(); ++i)
            peak = juce::jmax(peak, std::abs(data[i]));
    }

    juce::ignoreUnused(sampleRate);
    return peak;
}

}

float TruePeakMeter::measureBlockPeakLinear(const float* samples, int numSamples)
{
    return channelCubicPeak(samples, numSamples);
}

float TruePeakMeter::measurePeakLinear(const juce::AudioBuffer<float>& buffer, double sampleRate)
{
    const auto oversampled = oversampledPeak(buffer, sampleRate);

    float cubic = 0.0f;
    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        cubic = juce::jmax(cubic, channelCubicPeak(buffer.getReadPointer(ch), buffer.getNumSamples()));

    return juce::jmax(oversampled, cubic);
}

float TruePeakMeter::measurePeakDbfs(const juce::AudioBuffer<float>& buffer, double sampleRate)
{
    return juce::Decibels::gainToDecibels(measurePeakLinear(buffer, sampleRate), -100.0f);
}

}
