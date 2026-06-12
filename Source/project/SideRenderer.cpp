#include "SideRenderer.h"
#include "../io/AudioFileLoader.h"
#include "../io/AudioResampler.h"
#include "../dsp/graph/AdaptiveMasteringProcessor.h"
#include "../util/AppLog.h"
#include <algorithm>
#include <atomic>
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
    const int cores = juce::jmax(1, juce::SystemStats::getNumCpus());
    return juce::jlimit(1, clipCount, juce::jmin(6, cores - 1));
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

struct ClipRenderPool
{
    ClipRenderPool(int workers,
                   int totalClips,
                   const std::vector<TapeClip>& clips,
                   CassetteProfile profileIn,
                   MasteringOptions masteringIn,
                   float recLevelDbIn,
                   double sampleRateIn,
                   bool captureReferenceIn,
                   SideRenderer::ProgressCallback progressIn,
                   const juce::String& sideLabelIn)
        : pool(workers),
          clipCount(totalClips),
          clipList(clips),
          profile(std::move(profileIn)),
          mastering(std::move(masteringIn)),
          recLevelDb(recLevelDbIn),
          sampleRate(sampleRateIn),
          captureReference(captureReferenceIn),
          onProgress(std::move(progressIn)),
          sideLabel(sideLabelIn)
    {
        results.resize(static_cast<size_t>(clipCount));
    }

    bool run()
    {
        for (int w = 0; w < pool.getNumThreads(); ++w)
            pool.addJob(new WorkerJob(*this), true);

        pool.removeAllJobs(false, -1);
        return ! failed.load();
    }

    juce::String firstError() const
    {
        const juce::ScopedLock sl(errorLock);
        return failError;
    }

    std::vector<ClipProcessResult> results;

private:
    class WorkerJob : public juce::ThreadPoolJob
    {
    public:
        explicit WorkerJob(ClipRenderPool& ownerIn)
            : juce::ThreadPoolJob("clip-render"), owner(ownerIn)
        {
        }

        JobStatus runJob() override
        {
            for (;;)
            {
                if (owner.failed.load())
                    return jobHasFinished;

                const int clipIndex = owner.claimNextClip();
                if (clipIndex < 0)
                    return jobHasFinished;

                auto processed = processClip(owner.clipList[static_cast<size_t>(clipIndex)],
                                             owner.profile,
                                             owner.mastering,
                                             owner.recLevelDb,
                                             owner.sampleRate,
                                             owner.captureReference);

                owner.results[static_cast<size_t>(clipIndex)] = std::move(processed);

                if (! owner.results[static_cast<size_t>(clipIndex)].success)
                {
                    owner.setFailed(owner.results[static_cast<size_t>(clipIndex)].error);
                    return jobHasFinished;
                }

                const auto& done = owner.results[static_cast<size_t>(clipIndex)];
                logTiming("render-clip",
                          done.elapsedMs,
                          owner.sideLabel + " " + juce::String(clipIndex + 1) + "/" + juce::String(owner.clipCount)
                              + ": " + done.title);

                const int finished = owner.finishedCount.fetch_add(1) + 1;
                if (owner.onProgress)
                    owner.onProgress(finished, owner.clipCount, done.title);
            }
        }

    private:
        ClipRenderPool& owner;
    };

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

    juce::ThreadPool pool;
    const int clipCount;
    const std::vector<TapeClip>& clipList;
    CassetteProfile profile;
    MasteringOptions mastering;
    float recLevelDb;
    double sampleRate;
    bool captureReference;
    SideRenderer::ProgressCallback onProgress;
    juce::String sideLabel;

    juce::CriticalSection queueLock;
    int nextClip = 0;
    std::atomic<bool> failed { false };
    std::atomic<int> finishedCount { 0 };
    juce::CriticalSection errorLock;
    juce::String failError;
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

    ClipRenderPool pool(workers,
                        clipCount,
                        side.clips,
                        profile,
                        project.mastering,
                        project.recLevelDb,
                        sampleRate,
                        captureUnmasteredReference,
                        onProgress,
                        sideLabel);

    if (! pool.run())
    {
        result.error = pool.firstError();
        log("render-side error: " + result.error);
        return result;
    }

    int maxEndSample = 0;
    for (const auto& clip : pool.results)
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
