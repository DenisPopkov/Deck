#include "PreflightTones.h"
#include <cmath>

namespace cassette
{

namespace
{
void appendBlock(juce::AudioBuffer<float>& dest, int numSamples, double sampleRate)
{
    juce::ignoreUnused(sampleRate);
    const int channels = dest.getNumChannels();
    const int oldLen = dest.getNumSamples();
    dest.setSize(channels, oldLen + numSamples, true, true, true);
}
}

void PreflightTones::appendTone(juce::AudioBuffer<float>& dest,
                                double sampleRate,
                                float frequencyHz,
                                float levelDbfs,
                                double durationSeconds)
{
    const int n = static_cast<int>(durationSeconds * sampleRate);
    appendBlock(dest, n, sampleRate);
    const auto gain = juce::Decibels::decibelsToGain(levelDbfs);
    const int start = dest.getNumSamples() - n;

    for (int i = 0; i < n; ++i)
    {
        const auto sample = gain * std::sin(2.0 * juce::MathConstants<double>::pi * frequencyHz
                                            * static_cast<double>(i) / sampleRate);
        for (int ch = 0; ch < dest.getNumChannels(); ++ch)
            dest.setSample(ch, start + i, static_cast<float>(sample));
    }
}

void PreflightTones::appendPinkNoise(juce::AudioBuffer<float>& dest,
                                     double sampleRate,
                                     float levelDbfs,
                                     double durationSeconds)
{
    const int n = static_cast<int>(durationSeconds * sampleRate);
    appendBlock(dest, n, sampleRate);
    const auto gain = juce::Decibels::decibelsToGain(levelDbfs);
    const int start = dest.getNumSamples() - n;
    float b0 = 0, b1 = 0, b2 = 0, b3 = 0, b4 = 0, b5 = 0, b6 = 0;

    for (int i = 0; i < n; ++i)
    {
        const float white = juce::Random::getSystemRandom().nextFloat() * 2.0f - 1.0f;
        b0 = 0.99886f * b0 + white * 0.0555179f;
        b1 = 0.99332f * b1 + white * 0.0750759f;
        b2 = 0.96900f * b2 + white * 0.1538520f;
        b3 = 0.86650f * b3 + white * 0.3104856f;
        b4 = 0.55000f * b4 + white * 0.5329522f;
        b5 = -0.7616f * b5 - white * 0.0168980f;
        const auto pink = (b0 + b1 + b2 + b3 + b4 + b5 + b6 + white * 0.5362f) * 0.11f;
        b6 = white * 0.115926f;
        for (int ch = 0; ch < dest.getNumChannels(); ++ch)
            dest.setSample(ch, start + i, pink * gain);
    }
}

void PreflightTones::appendSilence(juce::AudioBuffer<float>& dest, double sampleRate, double durationSeconds)
{
    const int n = static_cast<int>(durationSeconds * sampleRate);
    appendBlock(dest, n, sampleRate);
}

void PreflightTones::prependToBuffer(juce::AudioBuffer<float>& musicBuffer,
                                     double sampleRate,
                                     const PreflightOptions& options)
{
    if (!options.includeTones && !options.includeSlate)
        return;

    juce::AudioBuffer<float> prefix(musicBuffer.getNumChannels(), 0);

    if (options.includeSlate)
        appendSilence(prefix, sampleRate, 3.0);

    if (options.includeTones)
    {
        if (options.kenwoodKX1100Calibration)
        {
            appendTone(prefix, sampleRate, 400.0f, -18.0f, 30.0);
            appendTone(prefix, sampleRate, 10000.0f, -20.0f, 15.0);
            appendTone(prefix, sampleRate, 1000.0f, -18.0f, 10.0);
            appendSilence(prefix, sampleRate, 1.0);
        }
        else
        {
            appendTone(prefix, sampleRate, 1000.0f, -18.0f, 25.0);
            appendTone(prefix, sampleRate, 10000.0f, -20.0f, 12.0);
            appendTone(prefix, sampleRate, 100.0f, -20.0f, 12.0);
            appendPinkNoise(prefix, sampleRate, -24.0f, 10.0);
            appendSilence(prefix, sampleRate, 1.0);
        }
    }

    const int total = prefix.getNumSamples() + musicBuffer.getNumSamples();
    juce::AudioBuffer<float> combined(musicBuffer.getNumChannels(), total);

    for (int ch = 0; ch < combined.getNumChannels(); ++ch)
    {
        combined.copyFrom(ch, 0, prefix, ch, 0, prefix.getNumSamples());
        combined.copyFrom(ch, prefix.getNumSamples(), musicBuffer, ch, 0, musicBuffer.getNumSamples());
    }

    musicBuffer = std::move(combined);
    juce::ignoreUnused(options.artist, options.title, options.sideLabel, options.dolbyNote);
}

}
