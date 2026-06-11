#include "MaskingNoiseGate.h"

namespace cassette
{

void MaskingNoiseGate::process(juce::AudioBuffer<float>& buffer,
                               double sampleRate,
                               const AudioFeatures& features,
                               float intensity)
{
    intensity = juce::jlimit(0.0f, 1.0f, intensity);
    if (intensity < 0.01f || buffer.getNumSamples() == 0 || buffer.getNumChannels() == 0)
        return;

    if (features.noiseFloorDbfs < -50.0f)
        return;

    const float thresholdLin = juce::Decibels::decibelsToGain(features.noiseFloorDbfs + 12.0f);
    const float maxAttenuationDb = juce::jmap(features.noiseFloorDbfs, -50.0f, -38.0f, 6.0f, 14.0f);
    const float minGainLin = juce::Decibels::decibelsToGain(-maxAttenuationDb * intensity);

    const float attack = std::exp(-1.0f / (0.004f * static_cast<float>(sampleRate)));
    const float release = std::exp(-1.0f / (0.100f * static_cast<float>(sampleRate)));

    const int numSamples = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();

    for (int i = 0; i < numSamples; ++i)
    {
        float peak = 0.0f;
        for (int ch = 0; ch < numChannels; ++ch)
            peak = juce::jmax(peak, std::abs(buffer.getSample(ch, i)));

        if (peak > envelope)
            envelope = attack * envelope + (1.0f - attack) * peak;
        else
            envelope = release * envelope + (1.0f - release) * peak;

        float gain = 1.0f;
        if (envelope < thresholdLin && thresholdLin > 1.0e-8f)
        {
            const float t = juce::jlimit(0.0f, 1.0f, envelope / thresholdLin);
            gain = juce::jmap(t, 0.0f, 1.0f, minGainLin, 1.0f);
        }

        for (int ch = 0; ch < numChannels; ++ch)
            buffer.setSample(ch, i, buffer.getSample(ch, i) * gain);
    }
}

}
