#include "DynamicEQ.h"

namespace cassette
{

DynamicEQ::DynamicEQ(float lowBandHz, float highBandHz, float thresholdDb, float ratio)
    : lowHz(lowBandHz), highHz(highBandHz), thresholdDb(thresholdDb), ratio(ratio),
      bandFilter((lowBandHz + highBandHz) * 0.5f, 1.0f, FilterType::HighPass)
{
    juce::ignoreUnused(lowHz, highHz);
}

void DynamicEQ::prepare(double sr, int maxBlockSize)
{
    sampleRate = sr;
    bandFilter.prepare(sr, maxBlockSize);
}

void DynamicEQ::setThreshold(float db) { thresholdDb = db; }
void DynamicEQ::setRatio(float r) { ratio = r; }

void DynamicEQ::process(juce::AudioBuffer<float>& buffer)
{
    const float attack = std::exp(-1.0f / (0.003f * static_cast<float>(sampleRate)));
    const float release = std::exp(-1.0f / (0.040f * static_cast<float>(sampleRate)));

    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
    {
        auto* data = buffer.getWritePointer(ch);
        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            const auto x = data[i];
            const auto level = std::abs(x);
            envelope = level > envelope ? attack * envelope + (1.0f - attack) * level
                                        : release * envelope + (1.0f - release) * level;

            const auto levelDb = juce::Decibels::gainToDecibels(envelope + 1.0e-9f, -100.0f);
            if (levelDb > thresholdDb)
            {
                const auto over = levelDb - thresholdDb;
                const auto grDb = over * (1.0f - 1.0f / ratio);
                data[i] *= juce::Decibels::decibelsToGain(-grDb);
            }
        }
    }
}

}
