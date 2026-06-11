#include "PerceptualPlaybackProcessor.h"
#include "../dsp/stereo/MidSideProcessor.h"

namespace cassette
{

namespace
{

template<typename FilterType>
void prepareFilter(FilterType& filter, double sr, int blockSize, int channels = 2)
{
    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sr;
    spec.maximumBlockSize = static_cast<juce::uint32>(juce::jmax(1, blockSize));
    spec.numChannels = static_cast<juce::uint32>(juce::jmax(1, channels));
    filter.prepare(spec);
}

template<typename FilterType>
void processFilter(FilterType& filter, juce::AudioBuffer<float>& buffer)
{
    juce::dsp::AudioBlock<float> block(buffer);
    juce::dsp::ProcessContextReplacing<float> ctx(block);
    filter.process(ctx);
}

}

void PerceptualPlaybackProcessor::prepare(double sr, int maxBlockSize)
{
    sampleRate = sr;
    maxBlock = juce::jmax(64, maxBlockSize);
    crossDelaySamples = juce::jmax(1, static_cast<int>(std::llround(220.0e-6 * sampleRate)));

    const int delayLen = juce::jmax(256, crossDelaySamples + maxBlock + 8);
    delayBuffer.setSize(2, delayLen);
    delayBuffer.clear();
    delayWritePos = 0;

    prepareFilter(lowSumLp, sampleRate, maxBlock, 1);
    prepareFilter(subHpfMain, sampleRate, maxBlock, 2);
    prepareFilter(harmBandBp, sampleRate, maxBlock, 1);
    prepareFilter(directHighShelf, sampleRate, maxBlock, 1);
    prepareFilter(directPeak, sampleRate, maxBlock, 1);
    prepareFilter(crossHighShelf, sampleRate, maxBlock, 1);
    prepareFilter(crossPeak, sampleRate, maxBlock, 1);
    prepareFilter(crossLowPass, sampleRate, maxBlock, 1);
    prepareFilter(sideAllPassA, sampleRate, maxBlock, 1);
    prepareFilter(sideAllPassB, sampleRate, maxBlock, 1);
    prepareFilter(sideLfHpf, sampleRate, maxBlock, 1);

    *lowSumLp.state = *Coefs::makeLowPass(sampleRate, 100.0, 0.707f);
    *subHpfMain.state = *Coefs::makeHighPass(sampleRate, 80.0, 0.707f);
    *harmBandBp.state = *Coefs::makeBandPass(sampleRate, 180.0, 0.85f);

    *sideAllPassA.state = *Coefs::makeAllPass(sampleRate, 700.0, 0.65f);
    *sideAllPassB.state = *Coefs::makeAllPass(sampleRate, 2400.0, 0.55f);
    *sideLfHpf.state = *Coefs::makeHighPass(sampleRate, 120.0, 0.707f);

    updateCrossfeedCoeffs();

    subMix.reset(sampleRate, 0.04);
    crossMix.reset(sampleRate, 0.04);
    widenMix.reset(sampleRate, 0.04);
    subMix.setCurrentAndTargetValue(playbackSettings.virtualSubAmount);
    crossMix.setCurrentAndTargetValue(playbackSettings.crossfeedAmount);
    widenMix.setCurrentAndTargetValue(playbackSettings.stereoWidenAmount);

    scratchMonoLow.setSize(1, maxBlock);
    scratchHarmonics.setSize(1, maxBlock);
    scratchDry.setSize(2, maxBlock);
    scratchDirectL.setSize(1, maxBlock);
    scratchDirectR.setSize(1, maxBlock);
    scratchCrossL.setSize(1, maxBlock);
    scratchCrossR.setSize(1, maxBlock);
    scratchMidSide.setSize(2, maxBlock);
}

void PerceptualPlaybackProcessor::reset()
{
    delayBuffer.clear();
    delayWritePos = 0;
    lowSumLp.reset();
    subHpfMain.reset();
    harmBandBp.reset();
    directHighShelf.reset();
    directPeak.reset();
    crossHighShelf.reset();
    crossPeak.reset();
    crossLowPass.reset();
    sideAllPassA.reset();
    sideAllPassB.reset();
    sideLfHpf.reset();
}

void PerceptualPlaybackProcessor::setSettings(const PerceptualPlaybackSettings& settings)
{
    playbackSettings = settings;
    subMix.setTargetValue(settings.virtualSubEnabled ? settings.virtualSubAmount : 0.0f);
    crossMix.setTargetValue(settings.crossfeedEnabled ? settings.crossfeedAmount : 0.0f);
    widenMix.setTargetValue(settings.stereoWidenEnabled ? settings.stereoWidenAmount : 0.0f);
}

void PerceptualPlaybackProcessor::updateCrossfeedCoeffs()
{
    *directHighShelf.state = *Coefs::makeHighShelf(sampleRate, 1500.0, 0.5f, juce::Decibels::decibelsToGain(-1.2f));
    *directPeak.state = *Coefs::makePeakFilter(sampleRate, 180.0, 0.55f, juce::Decibels::decibelsToGain(-0.3f));
    *crossHighShelf.state = *Coefs::makeHighShelf(sampleRate, 750.0, 0.5f, juce::Decibels::decibelsToGain(-0.3f));
    *crossPeak.state = *Coefs::makePeakFilter(sampleRate, 180.0, 0.55f, juce::Decibels::decibelsToGain(0.5f));
    *crossLowPass.state = *Coefs::makeLowPass(sampleRate, 10000.0, 0.707f);
}

void PerceptualPlaybackProcessor::processVirtualSub(juce::AudioBuffer<float>& buffer)
{
    if (buffer.getNumChannels() < 2 || buffer.getNumSamples() == 0)
        return;

    const int n = buffer.getNumSamples();
    auto& monoLow = scratchMonoLow;
    auto& harmonics = scratchHarmonics;
    auto& dry = scratchDry;
    monoLow.setSize(1, n, false, false, true);
    harmonics.setSize(1, n, false, false, true);
    dry.setSize(2, n, false, false, true);
    dry.makeCopyOf(buffer);

    monoLow.clear();
    for (int i = 0; i < n; ++i)
        monoLow.setSample(0, i, 0.5f * (buffer.getSample(0, i) + buffer.getSample(1, i)));

    processFilter(lowSumLp, monoLow);

    auto* low = monoLow.getWritePointer(0);
    auto* har = harmonics.getWritePointer(0);
    for (int i = 0; i < n; ++i)
    {
        const auto x = low[i];
        har[i] = 0.65f * x * x + 0.35f * x * x * x;
    }

    processFilter(harmBandBp, harmonics);
    processFilter(subHpfMain, dry);

    for (int i = 0; i < n; ++i)
    {
        const auto mix = subMix.getNextValue();
        const auto h = harmonics.getSample(0, i) * mix;
        buffer.setSample(0, i, dry.getSample(0, i) + h);
        buffer.setSample(1, i, dry.getSample(1, i) + h);
    }
}

void PerceptualPlaybackProcessor::processCrossfeed(juce::AudioBuffer<float>& buffer)
{
    if (buffer.getNumChannels() < 2 || buffer.getNumSamples() == 0)
        return;

    const int n = buffer.getNumSamples();
    const int delayLen = delayBuffer.getNumSamples();
    auto& directL = scratchDirectL;
    auto& directR = scratchDirectR;
    auto& crossL = scratchCrossL;
    auto& crossR = scratchCrossR;
    directL.setSize(1, n, false, false, true);
    directR.setSize(1, n, false, false, true);
    crossL.setSize(1, n, false, false, true);
    crossR.setSize(1, n, false, false, true);

    directL.copyFrom(0, 0, buffer, 0, 0, n);
    directR.copyFrom(0, 0, buffer, 1, 0, n);

    processFilter(directHighShelf, directL);
    processFilter(directPeak, directL);
    processFilter(directHighShelf, directR);
    processFilter(directPeak, directR);

    crossL.clear();
    crossR.clear();

    for (int i = 0; i < n; ++i)
    {
        const int readPos = (delayWritePos - crossDelaySamples + delayLen) % delayLen;
        crossL.setSample(0, i, delayBuffer.getSample(1, readPos));
        crossR.setSample(0, i, delayBuffer.getSample(0, readPos));

        delayBuffer.setSample(0, delayWritePos, buffer.getSample(0, i));
        delayBuffer.setSample(1, delayWritePos, buffer.getSample(1, i));
        delayWritePos = (delayWritePos + 1) % delayLen;
    }

    processFilter(crossHighShelf, crossL);
    processFilter(crossPeak, crossL);
    processFilter(crossLowPass, crossL);
    processFilter(crossHighShelf, crossR);
    processFilter(crossPeak, crossR);
    processFilter(crossLowPass, crossR);

    for (int i = 0; i < n; ++i)
    {
        const auto wet = crossMix.getNextValue();
        buffer.setSample(0, i, (1.0f - wet) * buffer.getSample(0, i)
                                 + wet * (directL.getSample(0, i) + crossL.getSample(0, i)));
        buffer.setSample(1, i, (1.0f - wet) * buffer.getSample(1, i)
                                 + wet * (directR.getSample(0, i) + crossR.getSample(0, i)));
    }
}

void PerceptualPlaybackProcessor::processStereoWiden(juce::AudioBuffer<float>& buffer)
{
    if (buffer.getNumChannels() < 2 || buffer.getNumSamples() == 0)
        return;

    const int n = buffer.getNumSamples();
    auto& ms = scratchMidSide;
    ms.setSize(2, n, false, false, true);
    ms.makeCopyOf(buffer);

    MidSideProcessor::toMidSide(ms);

    juce::AudioBuffer<float> side(1, n);
    side.copyFrom(0, 0, ms, 1, 0, n);

    processFilter(sideAllPassA, side);
    processFilter(sideAllPassB, side);

    const float widen = widenMix.getNextValue();
    const float sideGain = 1.0f + widen * 0.55f;

    auto* sideData = side.getWritePointer(0);
    for (int i = 0; i < n; ++i)
    {
        const auto s = sideData[i] * sideGain;
        sideData[i] = std::tanh(s * (1.0f + widen * 0.35f));
    }

    processFilter(sideLfHpf, side);
    ms.copyFrom(1, 0, side, 0, 0, n);

    MidSideProcessor::fromMidSide(ms);
    buffer.makeCopyOf(ms);
}

void PerceptualPlaybackProcessor::process(juce::AudioBuffer<float>& buffer)
{
    if (buffer.getNumChannels() < 2 || buffer.getNumSamples() == 0)
        return;

    if (playbackSettings.virtualSubEnabled)
        processVirtualSub(buffer);

    if (playbackSettings.stereoWidenEnabled)
        processStereoWiden(buffer);

    if (playbackSettings.crossfeedEnabled)
        processCrossfeed(buffer);
}

}
