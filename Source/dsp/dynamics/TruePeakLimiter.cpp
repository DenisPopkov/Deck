#include "TruePeakLimiter.h"
#include "../../analysis/TruePeakMeter.h"

namespace cassette
{

void TruePeakLimiter::prepare(double sr) { sampleRate = sr; }

void TruePeakLimiter::process(juce::AudioBuffer<float>& buffer)
{
    if (buffer.getNumSamples() == 0 || buffer.getNumChannels() == 0)
        return;

    const float ceiling = juce::Decibels::decibelsToGain(thresholdDbfs);
    constexpr int kMaxPasses = 6;

    for (int pass = 0; pass < kMaxPasses; ++pass)
    {
        const float peak = TruePeakMeter::measurePeakLinear(buffer, sampleRate);
        if (peak <= ceiling * 1.0001f || peak <= 0.0f)
            break;

        buffer.applyGain(ceiling / peak);
    }
}

}
