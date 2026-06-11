#include "WowFlutterEmulator.h"
#include <cmath>

namespace cassette
{

void WowFlutterEmulator::prepare(double sr, int maxBlockSize, int numChannels)
{
    sampleRate = sr;
    maxBlock = juce::jmax(64, maxBlockSize);
    channels = juce::jmax(1, numChannels);

    delayLen = juce::jmax(512, static_cast<int>(std::ceil(0.025 * sampleRate)));
    delayBuffer.setSize(channels, delayLen);
    delayBuffer.clear();
    writePos = 0;
}

void WowFlutterEmulator::setProfile(const CassetteProfile& profile)
{
    const bool heavy = profile.formulation == TapeFormulation::CheapPortable;
    wowDepth = heavy ? 0.0055f : 0.0035f;
    flutterDepth = heavy ? 0.0020f : 0.0012f;
    wowRateHz = heavy ? 0.55f : 0.75f;
    flutterRateHz = heavy ? 9.5f : 11.0f;
}

float WowFlutterEmulator::readInterpolated(int channel, float delaySamples) const
{
    delaySamples = juce::jlimit(0.0f, static_cast<float>(delayLen - 2), delaySamples);
    const float readPos = static_cast<float>(writePos) - delaySamples;
    const int i0 = (static_cast<int>(std::floor(readPos)) + delayLen * 4) % delayLen;
    const int i1 = (i0 + 1) % delayLen;
    const float frac = readPos - std::floor(readPos);

    const auto* data = delayBuffer.getReadPointer(channel);
    return data[i0] * (1.0f - frac) + data[i1] * frac;
}

void WowFlutterEmulator::process(juce::AudioBuffer<float>& buffer)
{
    if (delayBuffer.getNumChannels() < buffer.getNumChannels())
        prepare(sampleRate, maxBlock, buffer.getNumChannels());

    const int n = buffer.getNumSamples();
    const float baseDelay = static_cast<float>(delayLen) * 0.45f;

    for (int i = 0; i < n; ++i)
    {
        const float wow = std::sin(wowPhase);
        const float flutter = std::sin(flutterPhase);
        const float modSamples = baseDelay + (wow * wowDepth + flutter * flutterDepth) * static_cast<float>(sampleRate);

        wowPhase += juce::MathConstants<float>::twoPi * wowRateHz / static_cast<float>(sampleRate);
        flutterPhase += juce::MathConstants<float>::twoPi * flutterRateHz / static_cast<float>(sampleRate);

        if (wowPhase > juce::MathConstants<float>::twoPi)
            wowPhase -= juce::MathConstants<float>::twoPi;
        if (flutterPhase > juce::MathConstants<float>::twoPi)
            flutterPhase -= juce::MathConstants<float>::twoPi;

        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        {
            const float in = buffer.getSample(ch, i);
            delayBuffer.setSample(ch, writePos, in);
            buffer.setSample(ch, i, readInterpolated(ch, modSamples));
        }

        writePos = (writePos + 1) % delayLen;
    }
}

}
