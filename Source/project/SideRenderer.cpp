#include "SideRenderer.h"
#include "../io/AudioFileLoader.h"
#include "../io/AudioResampler.h"
#include "../dsp/graph/AdaptiveMasteringProcessor.h"
#include "../dsp/ml/TapeAwareSoftClipper.h"
#include "../util/AppLog.h"
#include <algorithm>
#include <atomic>
#include <thread>
#include <vector>

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

int parallelWorkerCount(int clipCount)
{
    const int hw = static_cast<int>(std::thread::hardware_concurrency());
    const int cores = juce::jmax(2, hw > 0 ? hw : juce::SystemStats::getNumCpus());
    return juce::jlimit(1, clipCount, juce::jmin(6, cores));
}

struct ClipProcessResult
{
    bool success = false;
    juce::String error;
    juce::AudioBuffer<float> mastered;
    juce::AudioBuffer<float> reference;
    int startSample = 0;
    int endSample = 0;
    double elapsedMs = 0.0;
    juce::String title;
};

ClipProcessResult processClip(const TapeClip& clip,
                              CassetteProfile profile,
                              MasteringOptions mastering,
                              float recLevelDb,
                              double sampleRate,
                              bool captureReference)
{
    ClipProcessResult out;
    out.title = clip.displayTitle;
    const double t0 = juce::Time::getMillisecondCounterHiRes();

    if (!clip.sourceFile.existsAsFile())
    {
        out.error = "Missing file: " + clip.sourceFile.getFileName();
        return out;
    }

    const auto loaded = AudioFileLoader::loadToBufferWithDiagnostics(clip.sourceFile);
    if (!loaded.audio.hasValue())
    {
        out.error = loaded.error;
        return out;
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

    out.startSample = juce::jmax(0, static_cast<int>(std::llround(clip.startTimeSec * sampleRate)));
    out.endSample = out.startSample + buffer.getNumSamples();

    if (captureReference)
        out.reference.makeCopyOf(buffer);

    AdaptiveMasteringProcessor::process(buffer, profile, mastering, sampleRate);
    applyRecLevel(buffer, recLevelDb);

    out.mastered = std::move(buffer);
    out.success = true;
    out.elapsedMs = juce::Time::getMillisecondCounterHiRes() - t0;
    return out;
}

struct ClipRenderTask
{
    int clipCount = 0;
    const std::vector<TapeClip>* clipList = nullptr;
    CassetteProfile profile;
    MasteringOptions mastering;
    float recLevelDb = 0.0f;
    double sampleRate = 48000.0;
    bool captureReference = false;
    SideRenderer::ProgressCallback onProgress;
    juce::String sideLabel;

    std::vector<ClipProcessResult> results;
    juce::CriticalSection queueLock;
    int nextClip = 0;
    std::atomic<bool> failed { false };
    std::atomic<int> finishedCount { 0 };
    juce::CriticalSection errorLock;
    juce::String failError;

    int claimNextClip()
    {
        const juce::ScopedLock sl(queueLock);
        if (failed.load() || nextClip >= clipCount)
            return -1;
        return nextClip++;
    }

    void setFailed(const juce::String& error)
    {
        const juce::ScopedLock sl(errorLock);
        if (failError.isEmpty())
            failError = error;
        failed.store(true);
    }

    void workerLoop()
    {
        warmThreadLocalOnnx();

        for (;;)
        {
            if (failed.load())
                return;

            const int clipIndex = claimNextClip();
            if (clipIndex < 0)
                return;

            auto processed = processClip(clipList->at(static_cast<size_t>(clipIndex)),
                                         profile,
                                         mastering,
                                         recLevelDb,
                                         sampleRate,
                                         captureReference);

            results[static_cast<size_t>(clipIndex)] = std::move(processed);

            if (! results[static_cast<size_t>(clipIndex)].success)
            {
                setFailed(results[static_cast<size_t>(clipIndex)].error);
                return;
            }

            const auto& done = results[static_cast<size_t>(clipIndex)];
            logTiming("render-clip",
                      done.elapsedMs,
                      sideLabel + " " + juce::String(clipIndex + 1) + "/" + juce::String(clipCount) + ": "
                          + done.title);

            const int finished = finishedCount.fetch_add(1) + 1;
            if (onProgress)
                onProgress(finished, clipCount, done.title);
        }
    }
};

void mergeClipIntoTimeline(juce::AudioBuffer<float>& timeline, const ClipProcessResult& clip)
{
    ensureTimelineLength(timeline, clip.endSample);
    for (int ch = 0; ch < juce::jmin(timeline.getNumChannels(), clip.mastered.getNumChannels()); ++ch)
    {
        const int copyLen = juce::jmin(clip.mastered.getNumSamples(), timeline.getNumSamples() - clip.startSample);
        if (copyLen > 0)
            timeline.addFrom(ch, clip.startSample, clip.mastered, ch, 0, copyLen);
    }
}

void mergeClipIntoReference(juce::AudioBuffer<float>& referenceTimeline, const ClipProcessResult& clip)
{
    if (clip.reference.getNumSamples() == 0)
        return;

    ensureTimelineLength(referenceTimeline, clip.endSample);
    for (int ch = 0; ch < juce::jmin(referenceTimeline.getNumChannels(), clip.reference.getNumChannels()); ++ch)
    {
        const int copyLen = juce::jmin(clip.reference.getNumSamples(), referenceTimeline.getNumSamples() - clip.startSample);
        if (copyLen > 0)
            referenceTimeline.addFrom(ch, clip.startSample, clip.reference, ch, 0, copyLen);
    }
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

    const int clipCount = static_cast<int>(side.clips.size());
    const juce::String sideLabel = sideB ? "Side B" : "Side A";
    const int workers = parallelWorkerCount(clipCount);
    log("render-side start: " + sideLabel + ", " + juce::String(clipCount) + " clips, workers="
        + juce::String(workers) + ", project=" + project.name);

    const double renderT0 = juce::Time::getMillisecondCounterHiRes();

    ClipRenderTask task;
    task.clipCount = clipCount;
    task.clipList = &side.clips;
    task.profile = profile;
    task.mastering = project.mastering;
    task.recLevelDb = project.recLevelDb;
    task.sampleRate = sampleRate;
    task.captureReference = captureUnmasteredReference;
    task.onProgress = onProgress;
    task.sideLabel = sideLabel;
    task.results.resize(static_cast<size_t>(clipCount));

    std::vector<std::thread> threads;
    threads.reserve(static_cast<size_t>(workers));
    for (int w = 0; w < workers; ++w)
        threads.emplace_back([&task]() { task.workerLoop(); });

    for (auto& t : threads)
        t.join();

    if (task.failed.load())
    {
        result.error = task.failError;
        log("render-side error: " + result.error);
        return result;
    }

    int maxEndSample = 0;
    for (const auto& clip : task.results)
    {
        mergeClipIntoTimeline(timeline, clip);
        if (captureUnmasteredReference)
            mergeClipIntoReference(referenceTimeline, clip);
        maxEndSample = juce::jmax(maxEndSample, clip.endSample);
    }

    if (!padToMaxTapeLength && maxEndSample > 0 && maxEndSample < timeline.getNumSamples())
        timeline.setSize(timeline.getNumChannels(), maxEndSample, true, true, true);

    if (captureUnmasteredReference && maxEndSample > 0 && maxEndSample < referenceTimeline.getNumSamples())
        referenceTimeline.setSize(referenceTimeline.getNumChannels(), maxEndSample, true, true, true);

    result.buffer = std::move(timeline);
    if (captureUnmasteredReference)
        result.referenceBuffer = std::move(referenceTimeline);
    result.success = true;

    const double renderMs = juce::Time::getMillisecondCounterHiRes() - renderT0;
    logTiming("render-side", renderMs, sideLabel + ", samples=" + juce::String(result.buffer.getNumSamples()));
    return result;
}

} // namespace cassette
