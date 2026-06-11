#include "TransientShaper.h"

namespace cassette
{

void TransientShaper::prepare(double sr) { sampleRate = sr; }

void TransientShaper::process(juce::AudioBuffer<float>& buffer)
{
    const float fast = std::exp(-1.0f / (0.001f * static_cast<float>(sampleRate)));
    const float slow = std::exp(-1.0f / (0.050f * static_cast<float>(sampleRate)));
    const auto attackGain = juce::Decibels::decibelsToGain(attackDb);
    const auto sustainGain = juce::Decibels::decibelsToGain(sustainDb);

    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
    {
        auto* data = buffer.getWritePointer(ch);
        float envFast = 0.0f;
        float envSlow = 0.0f;

        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            const auto x = std::abs(data[i]);
            envFast = fast * envFast + (1.0f - fast) * x;
            envSlow = slow * envSlow + (1.0f - slow) * x;
            const auto transient = juce::jmax(0.0f, envFast - envSlow);
            const auto sustain = envSlow;
            data[i] *= (1.0f + transient * (attackGain - 1.0f)) * (1.0f + sustain * (sustainGain - 1.0f));
        }
    }
}

}
