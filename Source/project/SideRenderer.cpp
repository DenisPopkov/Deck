#include "SideRenderer.h"
#include "../io/AudioFileLoader.h"
#include "../io/AudioResampler.h"
#include "../dsp/graph/AdaptiveMasteringProcessor.h"
#include <algorithm>

namespace cassette
{

namespace
{

void applyRecLevel(juce::AudioBuffer<float>& buffer, float recLevelDb)
{
    if (std::abs(recLevelDb) > 0.01f)
        buffer.applyGain(juce::Decibels::decibelsToGain(recLevelDb));
}

void ensureTimelineLength(juce::AudioBuffer<float>& timeline, int minSamples)
{
    if (minSamples <= timeline.getNumSamples())
        return;

    timeline.setSize(timeline.getNumChannels(), minSamples, true, true, true);
}

} // namespace

RenderResult SideRenderer::renderSide(const MixtapeProject& project,
                                      bool sideB,
                                      double sampleRate,
                                      bool padToMaxTapeLength,
                                      ProgressCallback onProgress,
                                      bool captureUnmasteredReference)
{
    RenderResult result;
    result.sampleRate = sampleRate;

    const auto& side = sideB ? project.sideB : project.sideA;
    if (side.clips.empty())
    {
        result.error = "No clips on this side";
        return result;
    }

    if (!side.fitsOnTape())
    {
        result.error = "Side exceeds maximum tape length";
        return result;
    }

    const double timelineSec = padToMaxTapeLength ? side.maxDurationSec : side.usedDurationSec();
    int totalSamples = static_cast<int>(std::ceil(juce::jmax(1.0, timelineSec) * sampleRate));
    juce::AudioBuffer<float> timeline(2, totalSamples);
    timeline.clear();

    juce::AudioBuffer<float> referenceTimeline;
    if (captureUnmasteredReference)
    {
        referenceTimeline.setSize(2, totalSamples);
        referenceTimeline.clear();
    }

    auto profile = project.profile;
    profile.hfTamerRatio = juce::jmax(1.05f, profile.hfTamerRatio - project.biasDb * 0.05f);

    int maxEndSample = 0;
    const int clipCount = static_cast<int>(side.clips.size());

    for (int clipIndex = 0; clipIndex < clipCount; ++clipIndex)
    {
        const auto& clip = side.clips[static_cast<size_t>(clipIndex)];

        if (!clip.sourceFile.existsAsFile())
        {
            result.error = "Missing file: " + clip.sourceFile.getFileName();
            return result;
        }

        const auto loaded = AudioFileLoader::loadToBufferWithDiagnostics(clip.sourceFile);
        if (!loaded.audio.hasValue())
        {
            result.error = loaded.error;
            return result;
        }

        auto buffer = loaded.audio->buffer;
        if (buffer.getNumChannels() == 1)
        {
            juce::AudioBuffer<float> stereo(2, buffer.getNumSamples());
            stereo.copyFrom(0, 0, buffer, 0, 0, buffer.getNumSamples());
            stereo.copyFrom(1, 0, buffer, 0, 0, buffer.getNumSamples());
            buffer = std::move(stereo);
        }

        resampleBufferLinear(buffer, loaded.audio->sampleRate, sampleRate);

        const int startSample = juce::jmax(0, static_cast<int>(std::llround(clip.startTimeSec * sampleRate)));
        const int endSample = startSample + buffer.getNumSamples();

        if (captureUnmasteredReference)
        {
            ensureTimelineLength(referenceTimeline, endSample);
            for (int ch = 0; ch < juce::jmin(referenceTimeline.getNumChannels(), buffer.getNumChannels()); ++ch)
            {
                const int copyLen = juce::jmin(buffer.getNumSamples(), referenceTimeline.getNumSamples() - startSample);
                if (copyLen > 0)
                    referenceTimeline.addFrom(ch, startSample, buffer, ch, 0, copyLen);
            }
        }

        AdaptiveMasteringProcessor::process(buffer, profile, project.mastering, sampleRate);
        applyRecLevel(buffer, project.recLevelDb);

        ensureTimelineLength(timeline, endSample);

        for (int ch = 0; ch < juce::jmin(timeline.getNumChannels(), buffer.getNumChannels()); ++ch)
        {
            const int copyLen = juce::jmin(buffer.getNumSamples(), timeline.getNumSamples() - startSample);
            if (copyLen > 0)
                timeline.addFrom(ch, startSample, buffer, ch, 0, copyLen);
        }

        maxEndSample = juce::jmax(maxEndSample, endSample);

        if (onProgress)
            onProgress(clipIndex + 1, clipCount, clip.displayTitle);
    }

    if (!padToMaxTapeLength && maxEndSample > 0 && maxEndSample < timeline.getNumSamples())
        timeline.setSize(timeline.getNumChannels(), maxEndSample, true, true, true);

    if (captureUnmasteredReference && maxEndSample > 0 && maxEndSample < referenceTimeline.getNumSamples())
        referenceTimeline.setSize(referenceTimeline.getNumChannels(), maxEndSample, true, true, true);

    result.buffer = std::move(timeline);
    if (captureUnmasteredReference)
        result.referenceBuffer = std::move(referenceTimeline);
    result.success = true;
    return result;
}

} // namespace cassette
