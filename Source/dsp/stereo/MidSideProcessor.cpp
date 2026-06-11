#include "MidSideProcessor.h"

namespace cassette
{

void MidSideProcessor::toMidSide(juce::AudioBuffer<float>& buffer)
{
    if (buffer.getNumChannels() < 2)
        return;

    auto* l = buffer.getWritePointer(0);
    auto* r = buffer.getWritePointer(1);
    const int n = buffer.getNumSamples();

    for (int i = 0; i < n; ++i)
    {
        const auto mid = 0.5f * (l[i] + r[i]);
        const auto side = 0.5f * (l[i] - r[i]);
        l[i] = mid;
        r[i] = side;
    }
}

void MidSideProcessor::fromMidSide(juce::AudioBuffer<float>& buffer)
{
    if (buffer.getNumChannels() < 2)
        return;

    auto* mid = buffer.getWritePointer(0);
    auto* side = buffer.getWritePointer(1);
    const int n = buffer.getNumSamples();

    for (int i = 0; i < n; ++i)
    {
        const auto m = mid[i];
        const auto s = side[i];
        mid[i] = m + s;
        side[i] = m - s;
    }
}

void MidSideProcessor::process(
    juce::AudioBuffer<float>& buffer,
    const std::function<void(juce::AudioBuffer<float>& midSide)>& fn)
{
    toMidSide(buffer);
    fn(buffer);
    fromMidSide(buffer);
}

}
