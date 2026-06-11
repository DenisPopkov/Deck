#include "PreviewEngine.h"
#include "../io/AudioResampler.h"

namespace cassette
{

void PreviewEngine::prepareToPlay(int samplesPerBlockExpected, double sr)
{
    const juce::ScopedLock lock(bufferLock);
    deviceSampleRate = sr;
    rebuildPlaybackBuffer();
    playbackFx.prepare(sr, samplesPerBlockExpected);
}

void PreviewEngine::releaseResources()
{
    const juce::ScopedLock lock(bufferLock);
    playbackFx.reset();
}

void PreviewEngine::rebuildPlaybackBuffer()
{
    if (sourceBuffer.getNumSamples() == 0 || sourceSampleRate <= 0.0 || deviceSampleRate <= 0.0)
    {
        mixBuffer.setSize(0, 0);
        playheadSec = 0.0;
        return;
    }

    mixBuffer.makeCopyOf(sourceBuffer);
    resampleBufferLinear(mixBuffer, sourceSampleRate, deviceSampleRate);
    playheadSec = juce::jlimit(0.0, getDurationSec(), playheadSec);
}

void PreviewEngine::getNextAudioBlock(const juce::AudioSourceChannelInfo& info)
{
    info.clearActiveBufferRegion();

    const juce::ScopedLock lock(bufferLock);

    if (!playing || mixBuffer.getNumSamples() == 0 || deviceSampleRate <= 0.0)
        return;

    const int start = static_cast<int>(playheadSec * deviceSampleRate);
    const int available = mixBuffer.getNumSamples() - start;
    const int toCopy = juce::jmin(info.numSamples, available);

    for (int ch = 0; ch < info.buffer->getNumChannels(); ++ch)
    {
        const int srcCh = juce::jmin(ch, mixBuffer.getNumChannels() - 1);
        if (toCopy > 0)
            info.buffer->copyFrom(ch, info.startSample, mixBuffer, srcCh, start, toCopy);
    }

    if (monitoringEnabled && info.buffer->getNumChannels() >= 2)
    {
        juce::AudioBuffer<float> block(info.buffer->getArrayOfWritePointers(),
                                       info.buffer->getNumChannels(),
                                       info.startSample,
                                       info.numSamples);
        playbackFx.process(block);
    }

    playheadSec += static_cast<double>(info.numSamples) / deviceSampleRate;
    if (playheadSec >= getDurationSec())
    {
        playing = false;
        playheadSec = getDurationSec();
    }
}

void PreviewEngine::setBuffer(juce::AudioBuffer<float> buffer, double sr)
{
    const juce::ScopedLock lock(bufferLock);
    playing = false;
    sourceBuffer = std::move(buffer);
    sourceSampleRate = sr;
    rebuildPlaybackBuffer();
    playbackFx.reset();
    if (deviceSampleRate > 0.0)
        playbackFx.prepare(deviceSampleRate, 512);
}

void PreviewEngine::setPlayheadSec(double sec)
{
    const juce::ScopedLock lock(bufferLock);
    playheadSec = juce::jlimit(0.0, getDurationSec(), sec);
}

double PreviewEngine::getDurationSec() const
{
    if (deviceSampleRate <= 0.0)
        return 0.0;
    return mixBuffer.getNumSamples() / deviceSampleRate;
}

void PreviewEngine::setPlaying(bool shouldPlay)
{
    const juce::ScopedLock lock(bufferLock);
    playing = shouldPlay;
}

void PreviewEngine::setMonitoringEnabled(bool enabled) { monitoringEnabled = enabled; }

void PreviewEngine::setPlaybackSettings(const PerceptualPlaybackSettings& settings)
{
    playbackFx.setSettings(settings);
}

}
