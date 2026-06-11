#include "AudioResampler.h"

namespace cassette
{

void resampleBufferLinear(juce::AudioBuffer<float>& buffer, double srcRate, double dstRate)
{
    if (srcRate <= 0.0 || dstRate <= 0.0 || std::abs(srcRate - dstRate) < 0.5)
        return;

    const int numChannels = buffer.getNumChannels();
    const int srcLen = buffer.getNumSamples();
    if (srcLen <= 0 || numChannels <= 0)
        return;

    const double ratio = dstRate / srcRate;
    const int dstLen = juce::jmax(1, static_cast<int>(std::llround(static_cast<double>(srcLen) * ratio)));

    juce::AudioBuffer<float> out(numChannels, dstLen);
    out.clear();

    for (int ch = 0; ch < numChannels; ++ch)
    {
        const auto* src = buffer.getReadPointer(ch);
        auto* dst = out.getWritePointer(ch);

        for (int i = 0; i < dstLen; ++i)
        {
            const double srcPos = static_cast<double>(i) / ratio;
            const int i0 = juce::jlimit(0, srcLen - 1, static_cast<int>(srcPos));
            const int i1 = juce::jmin(i0 + 1, srcLen - 1);
            const float frac = static_cast<float>(srcPos - static_cast<double>(i0));
            dst[i] = src[i0] + frac * (src[i1] - src[i0]);
        }
    }

    buffer = std::move(out);
}

}
