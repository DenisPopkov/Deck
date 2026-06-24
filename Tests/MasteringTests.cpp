#include "TestHelpers.h"
#include <cstdlib>
#include "../Source/project/MixtapeProject.h"
#include "../Source/project/FolderMixBuilder.h"
#include "../Source/project/MixtapeEditController.h"
#include "../Source/dsp/CassetteMasteringPlanner.h"
#include "../Source/export/PreflightTones.h"
#include "../Source/dsp/filters/BiquadFilter.h"
#include "../Source/dsp/dynamics/TruePeakLimiter.h"
#include "../Source/dsp/stereo/MidSideProcessor.h"
#include "../Source/analysis/TruePeakMeter.h"
#include "../Source/dsp/dynamics/MaskingNoiseGate.h"
#include "../Source/analysis/WowFlutterAnalyzer.h"
#include "../Source/dsp/ml/StnGridModel.h"
#include "../Source/dsp/ml/OnnxStnRunner.h"
#include "../Source/analysis/LoudnessMeter.h"
#include "../Source/analysis/SpectrumAnalyzer.h"
#include "../Source/dsp/ml/TapeAwareSoftClipper.h"
#include "../Source/io/AudioFileLoader.h"
#include "../Source/io/DropPayload.h"
#include "../Source/audio/PreviewEngine.h"
#include "../Source/audio/PerceptualPlaybackProcessor.h"
#include "../Source/io/AudioResampler.h"
#include "../Source/dsp/dynamics/DynamicEQ.h"
#include "../Source/dsp/dynamics/TransientShaper.h"
#include "../Source/dsp/tape/WowFlutterEmulator.h"
#include "../Source/dsp/tape/RubberBandWowProcessor.h"
#include "../Source/dsp/CassetteAutoMaster.h"
#include "../Source/analysis/PerceptualQualityGuard.h"
#include "../Source/dsp/graph/AdaptiveMasteringProcessor.h"
#include "../Source/export/WavExporter.h"
#include "../Source/project/SideRenderer.h"
#include "../Source/dsp/AudioConstants.h"

using namespace cassette;
using namespace cassette::test;

namespace
{

juce::File fixtureAlbumFromEnv()
{
    if (const char* path = std::getenv("CASSETTE_FIXTURE_ALBUM"))
        return juce::File(juce::String::fromUTF8(path));
    return {};
}

struct AlbumFixtureContext
{
    juce::File folder;
    FolderScanResult scan;
    TapeLengthSpec tape;
    FolderFitReport fit;
    MixtapeEditController editor;
    MixtapeEditController::LayoutSnapshot layout;
    bool ready = false;
};

bool loadAlbumFixture(AlbumFixtureContext& fx)
{
    fx.ready = false;
    fx.folder = fixtureAlbumFromEnv();
    if (!fx.folder.isDirectory())
        return false;

    fx.scan = FolderMixBuilder::scanFolder(fx.folder);
    if (!fx.scan.success)
        return false;

    fx.tape = tapeLengthSpecForPreset(TapeLengthPreset::C90, 45.0);
    fx.fit = FolderMixBuilder::analyzeFit(fx.scan, fx.tape);
    fx.editor.loadFromScan(fx.scan, fx.fit);
    fx.editor.syncCassettePlan(fx.tape);
    fx.layout = fx.editor.layoutForCassette(0);
    fx.ready = true;
    return true;
}

static bool isFullAlbumFixture(const AlbumFixtureContext& fx)
{
    return fx.scan.tracks.size() >= 10 && fx.scan.totalDurationSec > 45.0 * 60.0;
}

static void runFixtureAlbumDropAndImportTests(TestContext& ctx, const AlbumFixtureContext& fx)
{
    if (!fx.ready)
        return;

    juce::StringArray folderDrop;
    folderDrop.add(fx.folder.getFullPathName());
    ctx.expectTrue(classifyDropPayload(folderDrop) == DropPayloadKind::Folder,
                   "folder drag payload should classify as folder");
    ctx.expectTrue(isDropPayloadInterested(folderDrop), "folder drag should be accepted");

    juce::StringArray fileDrop;
    fileDrop.add(fx.scan.tracks.front().file.getFullPathName());
    ctx.expectTrue(classifyDropPayload(fileDrop) == DropPayloadKind::AudioFile,
                   "audio file drag payload should classify as audio");
    ctx.expectTrue(isDropPayloadInterested(fileDrop), "audio file drag should be accepted");

    juce::StringArray fileUrlDrop;
    fileUrlDrop.add("file://" + fx.scan.tracks.front().file.getFullPathName());
    const auto normalised = AudioFileLoader::normaliseDroppedPath(fileUrlDrop[0]);
    ctx.expectTrue(normalised.existsAsFile(), "file:// drop path should normalise to a file");
    ctx.expectTrue(AudioFileLoader::pickFirstAudioFile(fileUrlDrop) == normalised,
                   "pickFirstAudioFile should resolve file:// drops");

    juce::StringArray junkDrop;
    junkDrop.add(fx.folder.getChildFile("not-a-track.txt").getFullPathName());
    ctx.expectTrue(classifyDropPayload(junkDrop) == DropPayloadKind::None,
                   "unsupported file drag should be ignored");
    ctx.expectTrue(!isDropPayloadInterested(junkDrop), "unsupported drag should not be accepted");
}

static void runFixtureAlbumPreviewTests(TestContext& ctx, const AlbumFixtureContext& fx)
{
    if (!fx.ready)
        return;

    const auto loaded = AudioFileLoader::loadToBuffer(fx.scan.tracks.front().file);
    ctx.expectTrue(loaded.hasValue(), "preview fixture track should load into buffer");

    PreviewEngine engine;
    engine.setBuffer(loaded->buffer, loaded->sampleRate);
    engine.prepareToPlay(512, 48000.0);
    ctx.expectTrue(peakAbs(loaded->buffer) > 0.001f, "fixture track should contain audible audio");

    engine.setPlayheadSec(engine.getDurationSec() * 0.25);
    engine.setPlaying(true);

    juce::AudioBuffer<float> block(2, 512);
    float peak = 0.0f;
    for (int i = 0; i < 24; ++i)
    {
        juce::AudioSourceChannelInfo info(block);
        engine.getNextAudioBlock(info);
        peak = juce::jmax(peak, peakAbs(block));
    }

    ctx.expectTrue(bufferIsFinite(block), "preview engine output must stay finite");
    ctx.expectTrue(peak > 0.001f, "preview engine should output audible audio while playing");
    ctx.expectTrue(engine.getPlayheadSec() > 0.05, "preview playhead should advance during playback");

    const double seekTarget = engine.getDurationSec() * 0.5;
    engine.setPlaying(false);
    engine.setPlayheadSec(seekTarget);
    ctx.expectNear(static_cast<float>(engine.getPlayheadSec()),
                   static_cast<float>(seekTarget),
                   0.15f,
                   "preview seek should land near requested position");
}

static void runFixtureAlbumSingleTrackSmoke(TestContext& ctx, const AlbumFixtureContext& fx)
{
    if (!fx.ready)
        return;

    ctx.expectTrue(fx.editor.canPrepare(fx.tape), "mixtape editor should be prepare-ready");

    const auto& firstTrack = fx.scan.tracks.front().file;
    const auto loaded = AudioFileLoader::loadToBufferWithDiagnostics(firstTrack);
    ctx.expectTrue(loaded.audio.hasValue(),
                   ("album track should load: " + loaded.error).toRawUTF8());
    if (!loaded.audio.hasValue())
        return;

    auto buffer = loaded.audio->buffer;
    const double sr = loaded.audio->sampleRate;
    ctx.expectTrue(buffer.getNumChannels() >= 1, "loaded track should have channels");
    ctx.expectTrue(sr >= 44100.0, "fixture track should load at CD quality or higher");
    ctx.expectTrue(bufferIsFinite(buffer), "loaded album audio must be finite");

    const auto before = analyze(buffer, sr);
    const auto profile = CassetteProfile::fromFormulation(TapeFormulation::TypeIV);
    MasteringOptions opts;
    opts.skipQualityCompare = true;

    AdaptiveMasteringProcessor processor;
    const auto mastered = processor.process(buffer, profile, opts, sr);
    const auto after = analyze(buffer, sr);

    ctx.expectTrue(mastered.iterations >= 1, "album track mastering should run");
    ctx.expectTrue(bufferIsFinite(buffer), "mastered album track must stay finite");
    ctx.expectTrue(peakAbs(buffer) > 0.01f, "mastered album track should remain audible");
    ctx.expectTrue(after.truePeakDbfs <= profile.truePeakCeilingDbfs + 0.35f,
                   "mastered album track should respect true-peak ceiling");
    ctx.expectTrue(std::abs(after.integratedLUFS - before.integratedLUFS) < 8.0f,
                   "mastering should not wildly change loudness");

    const auto exportFile = juce::File::getSpecialLocation(juce::File::tempDirectory)
                                .getChildFile("deck_audit_mastered.wav");
    exportFile.deleteFile();
    ctx.expectTrue(WavExporter::writeWav32Float(exportFile, buffer, sr),
                   "mastered album track should export to WAV");
    ctx.expectTrue(exportFile.existsAsFile() && exportFile.getSize() > 10000,
                   "exported WAV should be non-trivial size");
    exportFile.deleteFile();
}

static void runFixtureAlbumSingleClipRenderTests(TestContext& ctx, const AlbumFixtureContext& fx)
{
    if (!fx.ready)
        return;

    std::vector<FolderTrackInfo> singleSideA = { fx.layout.sideA.front() };
    const std::vector<FolderTrackInfo> emptySideB;
    const auto profile = CassetteProfile::fromFormulation(TapeFormulation::TypeIV);
    MasteringOptions opts;
    opts.skipQualityCompare = true;

    auto project = FolderMixBuilder::buildProjectFromSides(singleSideA,
                                                           emptySideB,
                                                           fx.folder,
                                                           fx.editor.gapBetweenTracksSec(),
                                                           "single-clip",
                                                           profile,
                                                           opts,
                                                           fx.tape.minutesPerSide * 60.0);

    ctx.expectTrue(project.sideA.clips.size() == 1, "single-clip project should contain one clip");
    ctx.expectTrue(project.sideB.clips.empty(), "single-clip project should not use side B");

    const double sr = kProjectSampleRate;
    auto rendered = SideRenderer::renderSide(project, false, sr, false, {}, true);
    ctx.expectTrue(rendered.success, ("single-clip render: " + rendered.error).toRawUTF8());
    if (!rendered.success)
        return;

    ctx.expectTrue(bufferIsFinite(rendered.buffer), "single-clip mastered render must be finite");
    ctx.expectTrue(peakAbs(rendered.buffer) > 0.01f, "single-clip render should be audible");
    ctx.expectTrue(rendered.referenceBuffer.getNumSamples() > 0, "single-clip render should capture reference");
    ctx.expectTrue(bufferIsFinite(rendered.referenceBuffer), "single-clip reference must be finite");
    ctx.expectTrue(peakAbs(rendered.referenceBuffer) > 0.01f, "single-clip reference should be audible");
    ctx.expectTrue(rendered.buffer.getNumSamples() == rendered.referenceBuffer.getNumSamples(),
                   "reference timeline should match mastered timeline length");
    ctx.expectTrue(rendered.buffer.getNumSamples() < static_cast<int>(sr * fx.layout.sideA.front().durationSec * 1.05),
                   "trimmed render should not pad to full tape length");

    auto padded = SideRenderer::renderSide(project, false, sr, true, {}, false);
    ctx.expectTrue(padded.success, ("single-clip padded render: " + padded.error).toRawUTF8());
    if (padded.success)
    {
        const int expectedPad = static_cast<int>(fx.tape.minutesPerSide * 60.0 * sr);
        ctx.expectTrue(std::abs(padded.buffer.getNumSamples() - expectedPad) <= static_cast<int>(sr * 2.0),
                       "padded single-clip render should fill the tape side length");
    }
}

static void runFixtureAlbumFullPrepareTests(TestContext& ctx, const AlbumFixtureContext& fx)
{
    if (!fx.ready)
        return;

    if (std::getenv("CASSETTE_SKIP_FULL_PREPARE"))
    {
        std::cout << "SKIP: full album prepare (CASSETTE_SKIP_FULL_PREPARE is set)\n";
        return;
    }

    if (!isFullAlbumFixture(fx))
    {
        std::cout << "Running mini-album full prepare (" << fx.scan.tracks.size() << " tracks)\n";
    }
    else
    {
        std::cout << "Full album prepare: rendering Side A and Side B for "
                  << fx.scan.tracks.size() << " tracks...\n";
    }

    const juce::String projectName = isFullAlbumFixture(fx) ? "evermore" : "mini-album";

    const auto profile = CassetteProfile::fromFormulation(TapeFormulation::TypeIV);
    MasteringOptions opts;
    opts.skipQualityCompare = true;

    auto project = FolderMixBuilder::buildProjectFromSides(fx.layout.sideA,
                                                           fx.layout.sideB,
                                                           fx.folder,
                                                           fx.editor.gapBetweenTracksSec(),
                                                           projectName,
                                                           profile,
                                                           opts,
                                                           fx.tape.minutesPerSide * 60.0);

    ctx.expectTrue(!project.sideA.clips.empty(), "full prepare project should have side A clips");
    ctx.expectTrue(project.sideA.clips.size() + project.sideB.clips.size() == fx.scan.tracks.size(),
                   "full prepare should assign every scanned track");
    if (fx.fit.split.needsSideB)
        ctx.expectTrue(!project.sideB.clips.empty(), "split fixture should have side B clips");

    const double sr = kProjectSampleRate;
    const double gapSec = fx.editor.gapBetweenTracksSec();
    const int expectedSideASamples = static_cast<int>(FolderMixBuilder::sideDurationSec(fx.layout.sideA, gapSec) * sr);
    const int expectedSideBSamples = static_cast<int>(FolderMixBuilder::sideDurationSec(fx.layout.sideB, gapSec) * sr);

    int sideAClipsRendered = 0;
    const auto sideAProgress = [&](int done, int total, const juce::String& title) {
        sideAClipsRendered = done;
        std::cout << "  Side A " << done << "/" << total << ": " << title << "\n";
    };

    auto renderedA = SideRenderer::renderSide(project, false, sr, false, sideAProgress, false);
    ctx.expectTrue(renderedA.success, ("Side A full prepare: " + renderedA.error).toRawUTF8());
    if (!renderedA.success)
        return;

    ctx.expectTrue(sideAClipsRendered == static_cast<int>(project.sideA.clips.size()),
                   "Side A should render every clip");
    ctx.expectTrue(bufferIsFinite(renderedA.buffer), "Side A output must be finite");
    ctx.expectTrue(peakAbs(renderedA.buffer) > 0.01f, "Side A output should be audible");
    ctx.expectTrue(std::abs(renderedA.buffer.getNumSamples() - expectedSideASamples)
                       <= static_cast<int>(sr * 2.0),
                   "Side A duration should match planned side length");

    RenderResult renderedB;
    int sideBClipsRendered = 0;
    if (!project.sideB.clips.empty())
    {
        const auto sideBProgress = [&](int done, int total, const juce::String& title) {
            sideBClipsRendered = done;
            std::cout << "  Side B " << done << "/" << total << ": " << title << "\n";
        };

        renderedB = SideRenderer::renderSide(project, true, sr, false, sideBProgress, false);
        ctx.expectTrue(renderedB.success, ("Side B full prepare: " + renderedB.error).toRawUTF8());
        if (!renderedB.success)
            return;

        ctx.expectTrue(sideBClipsRendered == static_cast<int>(project.sideB.clips.size()),
                       "Side B should render every clip");
        ctx.expectTrue(bufferIsFinite(renderedB.buffer), "Side B output must be finite");
        ctx.expectTrue(peakAbs(renderedB.buffer) > 0.01f, "Side B output should be audible");
        ctx.expectTrue(std::abs(renderedB.buffer.getNumSamples() - expectedSideBSamples)
                           <= static_cast<int>(sr * 2.0),
                       "Side B duration should match planned side length");
    }
    else
    {
        std::cout << "SKIP: side B full prepare (single-side fixture)\n";
    }

    const auto tempDir = juce::File::getSpecialLocation(juce::File::tempDirectory)
                             .getChildFile("deck_audit_prepare");
    tempDir.createDirectory();
    const auto sideAFile = tempDir.getChildFile(projectName + " Side A.wav");
    const auto sideBFile = tempDir.getChildFile(projectName + " Side B.wav");
    sideAFile.deleteFile();
    sideBFile.deleteFile();

    ctx.expectTrue(WavExporter::writeWav32Float(sideAFile, renderedA.buffer, sr),
                   "Side A full prepare should export to WAV");
    const int minSideABytes = isFullAlbumFixture(fx) ? 100000 : 1000;
    ctx.expectTrue(sideAFile.existsAsFile() && sideAFile.getSize() > minSideABytes,
                   "Side A WAV should be non-trivial");

    if (!project.sideB.clips.empty())
    {
        ctx.expectTrue(WavExporter::writeWav32Float(sideBFile, renderedB.buffer, sr),
                       "Side B full prepare should export to WAV");
        ctx.expectTrue(sideBFile.existsAsFile() && sideBFile.getSize() > minSideABytes,
                       "Side B WAV should be non-trivial");
        sideBFile.deleteFile();
    }

    sideAFile.deleteFile();
}

static void runFixtureAlbumEditorTests(TestContext& ctx, AlbumFixtureContext& fx)
{
    if (!fx.ready || fx.layout.sideA.size() < 2)
        return;

    const auto first = fx.layout.sideA.front().displayName;
    const auto second = fx.layout.sideA[1].displayName;

    ctx.expectTrue(fx.editor.reorderWithinSide(0, 0, 1), "album editor should reorder within side A");
    ctx.expectTrue(fx.editor.sideA().front().displayName == second,
                   "reorder should move second track to the top of side A");

    fx.editor.getUndoManager().undo();
    ctx.expectTrue(fx.editor.sideA().front().displayName == first,
                   "undo should restore original side A order");

    const int sideBBefore = static_cast<int>(fx.editor.sideB().size());
    ctx.expectTrue(fx.editor.moveToSide(0, 0, 1, sideBBefore),
                   "album editor should move a track from side A to side B");
    ctx.expectTrue(fx.editor.sideB().size() == static_cast<size_t>(sideBBefore + 1),
                   "side B should gain one track after cross-side move");

    const auto preview = fx.editor.buildPreviewProject(fx.tape.minutesPerSide * 60.0);
    ctx.expectTrue(preview.sideA.clips.size() + preview.sideB.clips.size() == fx.scan.tracks.size(),
                   "preview project should still cover every track after edits");
    ctx.expectTrue(fx.editor.hasManualEdits(), "cross-side move should mark manual edits");
}

static void runFixtureAlbumNaturalSortAndRebalanceTests(TestContext& ctx)
{
    const auto folder = fixtureAlbumFromEnv();
    if (!folder.isDirectory())
        return;

    const auto scan = FolderMixBuilder::scanFolder(folder);
    ctx.expectTrue(scan.success, "album folder should scan successfully");
    if (!scan.success)
        return;

    for (size_t i = 1; i < scan.tracks.size(); ++i)
    {
        ctx.expectTrue(scan.tracks[i - 1].file.getFileName().compareNatural(scan.tracks[i].file.getFileName()) <= 0,
                       "folder scan should return tracks in natural filename order");
    }

    AlbumFixtureContext fx;
    if (!loadAlbumFixture(fx))
        return;

    while (!fx.editor.sideB().empty())
        ctx.expectTrue(fx.editor.moveToSide(1, 0, 0, static_cast<int>(fx.editor.sideA().size())),
                       "album rebalance setup should stack side B onto side A");

    if (!fx.editor.hasSideOverflow(fx.tape))
    {
        std::cout << "SKIP: album rebalance (fixture fits on one side)\n";
        return;
    }

    ctx.expectTrue(fx.editor.hasSideOverflow(fx.tape),
                   "stacked album should overflow one C90 side before rebalance");
    fx.editor.rebalance(fx.tape);
    ctx.expectTrue(!fx.editor.hasSideOverflow(fx.tape), "album rebalance should fit both sides on C90");
    ctx.expectTrue(fx.editor.canPrepare(fx.tape), "rebalanced album should be prepare-ready");
    ctx.expectTrue(fx.editor.mergedFullScan().tracks.size() == fx.scan.tracks.size(),
                   "album rebalance should keep all scanned tracks");
}

static void runFixtureAlbumIntegrationTests(TestContext& ctx)
{
    const auto folder = fixtureAlbumFromEnv();
    if (!folder.isDirectory())
    {
        std::cout << "SKIP: album integration (set CASSETTE_FIXTURE_ALBUM)\n";
        return;
    }

    std::cout << "Album fixture: " << folder.getFullPathName() << "\n";

    AlbumFixtureContext fx;
    ctx.expectTrue(loadAlbumFixture(fx), "album fixture should scan and build editor layout");
    if (!fx.ready)
        return;

    ctx.expectTrue(fx.scan.tracks.size() >= 2, "fixture should have at least 2 tracks");
    if (isFullAlbumFixture(fx))
    {
        std::cout << "Full album fixture detected (" << fx.scan.tracks.size() << " tracks)\n";
        ctx.expectTrue(fx.scan.tracks.size() >= 10, "full album fixture should have at least 10 tracks");
        ctx.expectTrue(fx.scan.totalDurationSec > 45.0 * 60.0, "full album should be longer than one tape side");
        ctx.expectTrue(fx.fit.fitsOnCassette, "full-length album should fit on one C90");
        ctx.expectTrue(fx.fit.split.needsSideB, "full-length album should use both sides on C90");
    }
    else
    {
        std::cout << "Mini album fixture smoke (" << fx.scan.tracks.size() << " tracks)\n";
        ctx.expectTrue(fx.fit.fits, "mini fixture tracks should be buildable");
        ctx.expectTrue(fx.fit.fitsOnCassette || fx.fit.fitsOneSide, "mini fixture should fit on cassette");
    }

    for (const auto& track : fx.scan.tracks)
    {
        ctx.expectTrue(AudioFileLoader::isSupportedAudioFile(track.file),
                       "scanned track should be a supported audio file");
        ctx.expectTrue(track.durationSec > 0.2, "scanned track should have positive duration");
    }

    runFixtureAlbumDropAndImportTests(ctx, fx);
    runFixtureAlbumPreviewTests(ctx, fx);
    runFixtureAlbumSingleTrackSmoke(ctx, fx);
    runFixtureAlbumSingleClipRenderTests(ctx, fx);
    runFixtureAlbumFullPrepareTests(ctx, fx);
    runFixtureAlbumEditorTests(ctx, fx);
    runFixtureAlbumNaturalSortAndRebalanceTests(ctx);
}

}

static void testHotMasterTrimmedToProfileCap(TestContext& ctx)
{
    constexpr double sr = 48000.0;
    auto buffer = makeSineBuffer(sr, 8.0, 440.0f, 0.92f);
    applyGain(buffer, 2.5f);

    const auto before = analyze(buffer, sr);
    const auto profile = CassetteProfile::fromFormulation(TapeFormulation::TypeII);
    const auto after = processAndAnalyze(buffer, sr, profile);

    ctx.expectTrue(before.integratedLUFS > -11.0f, "fixture should start loud (>-11 LUFS)");
    ctx.expectTrue(after.integratedLUFS <= profile.maxIntegratedLUFS + 0.5f,
                   "LUFS should not exceed profile cap");
    ctx.expectTrue(after.integratedLUFS >= profile.maxIntegratedLUFS - 0.5f,
                   "LUFS should not sit far below profile cap");
    ctx.expectTrue(after.truePeakDbfs <= profile.truePeakCeilingDbfs + 0.25f,
                   "true peak should respect Type II ceiling");
}

static void testModerateMasterStaysNearSourceLoudness(TestContext& ctx)
{
    constexpr double sr = 48000.0;
    auto buffer = makeSineBuffer(sr, 6.0, 440.0f, 0.18f);
    const auto before = analyze(buffer, sr);
    const auto profile = CassetteProfile::fromFormulation(TapeFormulation::TypeIV);
    const auto after = processAndAnalyze(buffer, sr, profile);

    ctx.expectTrue(std::abs(after.integratedLUFS - before.integratedLUFS) < 2.0f,
                   "moderate-level material should stay within 2 LU of source");
}

static void testOutputStableAndAudible(TestContext& ctx)
{
    constexpr double sr = 44100.0;
    auto buffer = makeSineBuffer(sr, 4.0, 220.0f, 0.35f);
    const auto profile = CassetteProfile::fromFormulation(TapeFormulation::TypeII);
    processAndAnalyze(buffer, sr, profile);

    ctx.expectTrue(bufferIsFinite(buffer), "samples must be finite (no NaN/Inf)");
    ctx.expectTrue(peakAbs(buffer) > 0.01f, "output must not be near silence");
}

static void testLfMonoStageAttenuatesSideBass(TestContext& ctx)
{
    constexpr double sr = 48000.0;
    const int n = static_cast<int>(sr * 5.0);
    juce::AudioBuffer<float> buffer(2, n);

    for (int i = 0; i < n; ++i)
    {
        const auto t = static_cast<double>(i) / sr;
        const auto bass = 0.6f * std::sin(2.0 * juce::MathConstants<double>::pi * 60.0 * t);
        buffer.setSample(0, i, bass);
        buffer.setSample(1, i, -bass);
    }

    const auto sideBefore = sideChannelRms(buffer);

    MidSideProcessor ms;
    BiquadFilter sideHpf(120.0f, 0.707f, FilterType::HighPass);
    sideHpf.prepare(sr, n, 1);

    ms.process(buffer, [&](juce::AudioBuffer<float>& midSide) {
        if (midSide.getNumChannels() < 2)
            return;
        juce::AudioBuffer<float> side(midSide.getArrayOfWritePointers() + 1, 1, midSide.getNumSamples());
        sideHpf.process(side);
    });

    const auto sideAfter = sideChannelRms(buffer);

    ctx.expectTrue(sideBefore > 0.05f, "fixture should have side bass energy");
    ctx.expectTrue(sideAfter < sideBefore * 0.55f,
                   "side HPF @120 Hz should cut 60 Hz out-of-phase bass");
}

static void testAdaptiveHfTamingDoesNotBoostHighs(TestContext& ctx)
{
    constexpr double sr = 48000.0;
    const int n = static_cast<int>(sr * 6.0);
    juce::AudioBuffer<float> buffer(2, n);

    for (int i = 0; i < n; ++i)
    {
        const auto t = static_cast<double>(i) / sr;
        const auto hf = 0.45f * std::sin(2.0 * juce::MathConstants<double>::pi * 9000.0 * t);
        const auto body = 0.25f * std::sin(2.0 * juce::MathConstants<double>::pi * 300.0 * t);
        const auto s = hf + body;
        buffer.setSample(0, i, s);
        buffer.setSample(1, i, s);
    }

    const auto before = analyze(buffer, sr);
    const auto profile = CassetteProfile::fromFormulation(TapeFormulation::TypeII);
    MasteringOptions fullTapePrep;
    fullTapePrep.maximumDigital = false;
    const auto after = processAndAnalyze(buffer, sr, profile, fullTapePrep);

    ctx.expectTrue(after.psycho.hfAboveMaskingDb <= before.psycho.hfAboveMaskingDb + 1.0f,
                   "masking-aware HF tamer should not increase HF audibility");
    ctx.expectTrue(after.hfEnergyRatio <= before.hfEnergyRatio + 0.05f,
                   "HF-heavy fixture should not increase HF energy ratio");
}

static void testSideLfMonoInFullTapePrep(TestContext& ctx)
{
    constexpr double sr = 48000.0;
    const int n = static_cast<int>(sr * 4.0);
    juce::AudioBuffer<float> buffer(2, n);

    for (int i = 0; i < n; ++i)
    {
        const auto t = static_cast<double>(i) / sr;
        const auto bass = 0.55f * std::sin(2.0 * juce::MathConstants<double>::pi * 55.0 * t);
        buffer.setSample(0, i, bass);
        buffer.setSample(1, i, -bass);
    }

    const auto sideBefore = sideChannelRms(buffer);
    MasteringOptions fullTapePrep;
    fullTapePrep.maximumDigital = false;
    processAndAnalyze(buffer, sr, CassetteProfile::fromFormulation(TapeFormulation::TypeIV), fullTapePrep);
    const auto sideAfter = sideChannelRms(buffer);

    ctx.expectTrue(sideBefore > 0.04f, "fixture should have side bass");
    ctx.expectTrue(sideAfter < sideBefore * 0.55f,
                   "full tape prep should mono-maker side LF");
}

static void testDisabledStereoTighteningPreservesWideBass(TestContext& ctx)
{
    constexpr double sr = 48000.0;
    const int n = static_cast<int>(sr * 4.0);
    juce::AudioBuffer<float> tightBuffer(2, n);
    juce::AudioBuffer<float> looseBuffer(2, n);

    for (int i = 0; i < n; ++i)
    {
        const auto t = static_cast<double>(i) / sr;
        const auto bass = 0.55f * std::sin(2.0 * juce::MathConstants<double>::pi * 55.0 * t);
        tightBuffer.setSample(0, i, bass);
        tightBuffer.setSample(1, i, -bass);
        looseBuffer.setSample(0, i, bass);
        looseBuffer.setSample(1, i, -bass);
    }

    MasteringOptions tightened;
    tightened.maximumDigital = false;
    tightened.enableStereoTightening = true;
    processAndAnalyze(tightBuffer, sr, CassetteProfile::fromFormulation(TapeFormulation::TypeIV), tightened);

    MasteringOptions loose;
    loose.maximumDigital = false;
    loose.enableStereoTightening = false;
    processAndAnalyze(looseBuffer, sr, CassetteProfile::fromFormulation(TapeFormulation::TypeIV), loose);

    const auto sideTight = sideChannelRms(tightBuffer);
    const auto sideLoose = sideChannelRms(looseBuffer);

    ctx.expectTrue(sideLoose > sideTight * 1.35f,
                   "disabled stereo tightening should keep more wide bass than enabled path");
}

static void testDisabledTruePeakLimiterAllowsHotPeaks(TestContext& ctx)
{
    constexpr double sr = 48000.0;
    const int n = static_cast<int>(sr * 8.0);
    juce::AudioBuffer<float> limitedBuffer(2, n);
    juce::AudioBuffer<float> unlimitedBuffer(2, n);
    limitedBuffer.clear();
    unlimitedBuffer.clear();

    const int hop = static_cast<int>(sr * 0.02);
    for (int i = 0; i < n; i += hop)
    {
        constexpr float amp = 0.95f;
        limitedBuffer.setSample(0, i, amp);
        limitedBuffer.setSample(1, i, amp);
        unlimitedBuffer.setSample(0, i, amp);
        unlimitedBuffer.setSample(1, i, amp);
    }

    const auto profile = CassetteProfile::fromFormulation(TapeFormulation::TypeII);

    MasteringOptions limited;
    limited.maximumDigital = true;
    limited.enableTruePeakLimiter = true;
    const auto limitedAfter = processAndAnalyze(limitedBuffer, sr, profile, limited);

    MasteringOptions unlimited;
    unlimited.maximumDigital = true;
    unlimited.enableTruePeakLimiter = false;
    const auto unlimitedAfter = processAndAnalyze(unlimitedBuffer, sr, profile, unlimited);

    ctx.expectTrue(limitedAfter.truePeakDbfs <= profile.truePeakCeilingDbfs + 0.35f,
                   "enabled limiter should respect ceiling");
    ctx.expectTrue(unlimitedAfter.truePeakDbfs > limitedAfter.truePeakDbfs + 0.1f,
                   "disabled limiter should leave hotter peaks than enabled path");
}

static void testPsychoacousticMetricsPresent(TestContext& ctx)
{
    constexpr double sr = 48000.0;
    auto buffer = makeSineBuffer(sr, 2.0, 1000.0f, 0.2f);
    const auto features = analyze(buffer, sr);

    ctx.expectTrue(features.psycho.sharpnessAcum > 0.5f && features.psycho.sharpnessAcum < 3.0f,
                   "sharpness should be in a plausible range for 1 kHz tone");
    ctx.expectTrue(features.psycho.hfTamerStrength >= 0.0f && features.psycho.hfTamerStrength <= 1.0f,
                   "HF tamer strength should be normalized");
    ctx.expectTrue(features.psycho.roughnessAsper >= 0.0f && features.psycho.roughnessAsper < 2.0f,
                   "roughness should be non-negative and bounded");
}

static void testRoughnessHigherOnModulatedHf(TestContext& ctx)
{
    constexpr double sr = 48000.0;
    const int n = static_cast<int>(sr * 3.0);
    juce::AudioBuffer<float> tone(2, n);
    juce::AudioBuffer<float> modulated(2, n);

    for (int i = 0; i < n; ++i)
    {
        const auto t = static_cast<double>(i) / sr;
        const auto carrier = 0.35f * std::sin(2.0 * juce::MathConstants<double>::pi * 7000.0 * t);
        const auto am = 0.5f + 0.5f * std::sin(2.0 * juce::MathConstants<double>::pi * 70.0 * t);
        tone.setSample(0, i, carrier);
        tone.setSample(1, i, carrier);
        modulated.setSample(0, i, carrier * am);
        modulated.setSample(1, i, carrier * am);
    }

    const auto toneMetrics = analyze(tone, sr).psycho;
    const auto modMetrics = analyze(modulated, sr).psycho;

    ctx.expectTrue(modMetrics.roughnessAsper > toneMetrics.roughnessAsper,
                   "AM-modulated HF should increase roughness vs steady HF tone");
}

static void testAdaptiveFallbackProducesQualityReport(TestContext& ctx)
{
    constexpr double sr = 48000.0;
    const int n = static_cast<int>(sr * 4.0);
    juce::AudioBuffer<float> buffer(2, n);

    for (int i = 0; i < n; ++i)
    {
        const auto t = static_cast<double>(i) / sr;
        const auto s = 0.55f * std::sin(2.0 * juce::MathConstants<double>::pi * 440.0 * t)
                       + 0.35f * std::sin(2.0 * juce::MathConstants<double>::pi * 9500.0 * t);
        buffer.setSample(0, i, s);
        buffer.setSample(1, i, s);
    }

    MasteringOptions options;
    options.maximumDigital = false;
    options.perceptualAutoFallback = true;
    options.hfTamerIntensity = 1.0f;

    const auto result = AdaptiveMasteringProcessor::process(
        buffer, CassetteProfile::fromFormulation(TapeFormulation::TypeII), options, sr);

    ctx.expectTrue(result.iterations >= 1, "adaptive processor should run at least once");
    ctx.expectTrue(result.quality.objectiveDifferenceGrade <= 0.0f,
                   "ODG should be on the valid ITU-like scale");
    ctx.expectTrue(result.featuresAfter.integratedLUFS > -30.0f, "output should remain audible");
}

static void testPeaqBasicEvaluation(TestContext& ctx)
{
    constexpr double sr = 48000.0;
    auto ref = makeSineBuffer(sr, 2.0, 440.0f, 0.25f);
    auto test = ref;
    test.applyGain(0.98f);

    const auto peaq = PeaqEvaluator::evaluate(ref, test, sr);
    ctx.expectTrue(peaq.success, "PEAQ Basic should evaluate");
    ctx.expectTrue(peaq.backend == PeaqBackend::Basic, "should use Basic backend without gstreamer");
    ctx.expectTrue(peaq.objectiveDifferenceGrade <= 0.0f && peaq.objectiveDifferenceGrade >= -4.0f,
                   "ODG in valid range");
}

static void testRoughnessDeEsserReducesRinging(TestContext& ctx)
{
    constexpr double sr = 48000.0;
    const int n = static_cast<int>(sr * 2.0);
    juce::AudioBuffer<float> buffer(2, n);
    for (int i = 0; i < n; ++i)
    {
        const auto t = static_cast<double>(i) / sr;
        const auto carrier = 0.4f * std::sin(2.0 * juce::MathConstants<double>::pi * 8000.0 * t);
        const auto am = 0.5f + 0.5f * std::sin(2.0 * juce::MathConstants<double>::pi * 90.0 * t);
        const auto s = carrier * am;
        buffer.setSample(0, i, s);
        buffer.setSample(1, i, s);
    }

    const auto before = analyze(buffer, sr).psycho.streamingRingingIndex;
    RoughnessDeEsser deEsser;
    deEsser.process(buffer, analyze(buffer, sr).psycho, sr, 1.0f);
    const auto after = analyze(buffer, sr).psycho.streamingRingingIndex;

    ctx.expectTrue(before > 0.05f, "fixture should have ringing index");
    ctx.expectTrue(after <= before + 0.05f, "roughness de-esser should not increase ringing index");
}

static void testCheapPortableLouderCapThanHighEndDeck(TestContext& ctx)
{
    constexpr double sr = 48000.0;
    auto bufferA = makeSineBuffer(sr, 6.0, 330.0f, 0.25f);
    auto bufferB = makeSineBuffer(sr, 6.0, 330.0f, 0.25f);

    const auto inFeatures = analyze(bufferA, sr);
    juce::ignoreUnused(inFeatures);

    MasteringGraph graphA;
    graphA.prepare(sr, bufferA.getNumSamples());
    graphA.process(bufferA, CassetteProfile::fromFormulation(TapeFormulation::CheapPortable),
                   analyze(bufferA, sr));

    MasteringGraph graphB;
    graphB.prepare(sr, bufferB.getNumSamples());
    graphB.process(bufferB, CassetteProfile::fromFormulation(TapeFormulation::HighEndDeck),
                   analyze(bufferB, sr));

    const auto cheapLufs = analyze(bufferA, sr).integratedLUFS;
    const auto hiEndLufs = analyze(bufferB, sr).integratedLUFS;

    ctx.expectTrue(cheapLufs >= hiEndLufs - 0.5f,
                   "Cheap Portable cap should allow equal or higher LUFS than High-end Deck");
    ctx.expectTrue(CassetteProfile::fromFormulation(TapeFormulation::CheapPortable).maxIntegratedLUFS
                       > CassetteProfile::fromFormulation(TapeFormulation::HighEndDeck).maxIntegratedLUFS,
                   "Cheap Portable profile cap should be higher than High-end Deck");
}

static void testMixtapeSideLengthValidation(TestContext& ctx)
{
    auto project = MixtapeProject::demoProject();
    ctx.expectTrue(project.sideA.fitsOnTape(), "demo side A should fit within 45 minutes");

    TapeClip tooLong;
    tooLong.startTimeSec = 44.0 * 60.0;
    tooLong.durationSec = 2.0 * 60.0;
    ctx.expectTrue(!project.sideA.validateClipPlacement(tooLong),
                   "clip extending past 45:00 should be rejected");
}

static void testFolderMixSplitsAcrossTwoSides(TestContext& ctx)
{
    FolderScanResult scan;
    scan.success = true;
    scan.gapBetweenTracksSec = 2.0;

    auto addTrack = [&](double minutes) {
        FolderTrackInfo t;
        t.durationSec = minutes * 60.0;
        scan.tracks.push_back(t);
    };

    for (int i = 0; i < 5; ++i)
        addTrack(10.0);

    scan.totalDurationSec = 5.0 * 10.0 * 60.0 + 4 * scan.gapBetweenTracksSec;

    const auto tape = tapeLengthSpecForPreset(TapeLengthPreset::C90, 45.0);
    const auto report = FolderMixBuilder::analyzeFit(scan, tape);

    ctx.expectTrue(!report.fitsOneSide, "84+ min should not fit one 45 min side");
    ctx.expectTrue(report.fitsOnCassette, "should fit on C90 cassette (two sides)");
    ctx.expectTrue(report.split.needsSideB, "should split to side B");
    ctx.expectTrue(report.sideATrackCount >= 1 && report.sideBTrackCount >= 1,
                   "both sides should have tracks");
    const double sideCap = report.allowedSec + 2.0 * scan.gapBetweenTracksSec + 1.0;
    ctx.expectTrue(report.split.sideADurationSec <= sideCap, "side A within capacity");
    ctx.expectTrue(report.split.sideBDurationSec <= sideCap, "side B within capacity");

    const double imbalance = std::abs(report.split.sideADurationSec - report.split.sideBDurationSec);
    const double greedyImbalance = (4.0 * 10.0 * 60.0 + 3.0 * scan.gapBetweenTracksSec)
                                   - (1.0 * 10.0 * 60.0);
    ctx.expectTrue(imbalance < greedyImbalance - 60.0,
                   "split should balance sides instead of maxing side A");
}

static void testAnalyzeLayoutRespectsManualSplit(TestContext& ctx)
{
    FolderScanResult scan;
    scan.success = true;
    scan.gapBetweenTracksSec = 2.0;

    for (int i = 0; i < 4; ++i)
    {
        FolderTrackInfo t;
        t.durationSec = 12.0 * 60.0;
        scan.tracks.push_back(t);
    }

    scan.totalDurationSec = 4.0 * 12.0 * 60.0 + 3.0 * scan.gapBetweenTracksSec;

    const auto tape = tapeLengthSpecForPreset(TapeLengthPreset::C90, 45.0);
    const auto autoReport = FolderMixBuilder::analyzeFit(scan, tape);
    const int manualSplit = autoReport.split.sideAEndIndex == 1 ? 2 : 1;
    const auto manualReport = FolderMixBuilder::analyzeLayout(scan, manualSplit, tape);

    ctx.expectTrue(manualReport.split.sideAEndIndex == manualSplit, "manual split index preserved");
    ctx.expectTrue(manualReport.sideATrackCount == manualSplit, "side A track count follows manual split");
    ctx.expectTrue(manualReport.sideBTrackCount == scan.tracks.size() - static_cast<size_t>(manualSplit),
                   "side B track count follows manual split");
    ctx.expectTrue(manualReport.split.sideADurationSec != autoReport.split.sideADurationSec
                       || manualReport.split.sideBDurationSec != autoReport.split.sideBDurationSec,
                   "manual layout should differ from auto split when index differs");
}

static void testMixtapeEditorDeleteRecalculatesFit(TestContext& ctx)
{
    FolderScanResult scan;
    scan.success = true;
    scan.gapBetweenTracksSec = 2.0;

    for (int i = 0; i < 4; ++i)
    {
        FolderTrackInfo t;
        t.durationSec = 10.0 * 60.0;
        scan.tracks.push_back(t);
    }

    scan.totalDurationSec = 4.0 * 10.0 * 60.0 + 3.0 * scan.gapBetweenTracksSec;
    const auto tape = tapeLengthSpecForPreset(TapeLengthPreset::C90, 45.0);
    const auto fit = FolderMixBuilder::analyzeFit(scan, tape);

    MixtapeEditController editor;
    editor.loadFromScan(scan, fit);
    ctx.expectTrue(editor.canPrepare(tape), "initial layout should be prepare-ready");

    editor.deleteTrack(0, 0);
    const auto after = editor.computeFit(tape);
    ctx.expectTrue(after.trackCount == 3, "delete should reduce track count");
    ctx.expectTrue(after.requiredSec < fit.requiredSec, "delete should reduce required duration");
}

static void testMixtapeEditorSyncConsolidatesToOneCassette(TestContext& ctx)
{
    FolderScanResult scan;
    scan.success = true;
    scan.gapBetweenTracksSec = 2.0;

    for (int i = 0; i < 7; ++i)
    {
        FolderTrackInfo t;
        t.displayName = "Track " + juce::String(i + 1);
        t.durationSec = 20.0 * 60.0;
        scan.tracks.push_back(t);
    }

    scan.totalDurationSec = 7.0 * 20.0 * 60.0 + 6.0 * scan.gapBetweenTracksSec;
    const auto tape = tapeLengthSpecForPreset(TapeLengthPreset::C120, 60.0);
    const auto fit = FolderMixBuilder::analyzeFit(scan, tape);
    ctx.expectTrue(fit.cassetteCount == 2, "140+ min should require two cassettes");

    MixtapeEditController editor;
    editor.loadFromScan(scan, fit);
    ctx.expectTrue(editor.getCassetteCount() == 2, "editor should start with two cassettes");

    editor.setActiveCassetteIndex(0);
    ctx.expectTrue(editor.deleteTrack(0, 0), "delete from first cassette");
    ctx.expectTrue(editor.deleteTrack(0, 0), "delete second track from first cassette");
    editor.syncCassettePlan(tape);

    ctx.expectTrue(editor.getCassetteCount() == 1, "removed tracks should consolidate to one cassette");
    ctx.expectTrue(editor.mergedFullScan().tracks.size() == 5, "deleted tracks should stay removed");
    ctx.expectTrue(editor.canPrepare(tape), "consolidated layout should be prepare-ready");
}

static void testMultiCassetteSplitWhenAlbumExceedsOneTape(TestContext& ctx)
{
    FolderScanResult scan;
    scan.success = true;
    scan.gapBetweenTracksSec = 2.0;

    for (int i = 0; i < 7; ++i)
    {
        FolderTrackInfo t;
        t.durationSec = 20.0 * 60.0;
        scan.tracks.push_back(t);
    }

    scan.totalDurationSec = 7.0 * 20.0 * 60.0 + 6.0 * scan.gapBetweenTracksSec;

    const auto tape = tapeLengthSpecForPreset(TapeLengthPreset::C120, 60.0);
    const auto report = FolderMixBuilder::analyzeFit(scan, tape);

    ctx.expectTrue(report.fits, "tracks shorter than side cap should be buildable");
    ctx.expectTrue(!report.fitsOnCassette, "140+ min should exceed one C120 cassette");
    ctx.expectTrue(report.cassetteCount == 2, "should split across two cassettes");

    for (const auto& cassette : report.cassettes)
    {
        const double sideCap = report.allowedSec + 2.0 * scan.gapBetweenTracksSec + 1.0;
        ctx.expectTrue(cassette.sideADurationSec <= sideCap, "cassette side A within cap");
        if (cassette.hasSideB)
            ctx.expectTrue(cassette.sideBDurationSec <= sideCap, "cassette side B within cap");
    }

    const auto& first = report.cassettes.front();
    ctx.expectTrue(first.sideATrackCount() >= 2, "first cassette side A should be well filled");
    ctx.expectTrue(first.hasSideB, "first cassette should use side B before spilling to cassette 2");
}

static void testAutoMasteringPlannerPicksMinimalForQuietSource(TestContext& ctx)
{
    AudioFeatures quiet;
    quiet.integratedLUFS = -14.0f;
    quiet.truePeakDbfs = -1.5f;
    quiet.lfStereoCorrelation = 0.85f;
    quiet.hfEnergyRatio = 1.2f;
    quiet.clippingPercent = 0.0f;
    quiet.psycho.hfTamerStrength = 0.05f;
    quiet.psycho.roughnessAsper = 0.08f;
    quiet.psycho.sharpnessAcum = 1.1f;
    quiet.psycho.hfAboveMaskingDb = 1.0f;

    const auto profile = CassetteProfile::fromFormulation(TapeFormulation::TypeIV);
    const auto plan = CassetteMasteringPlanner::plan(quiet, profile);

    ctx.expectTrue(plan.options.maximumDigital, "quiet material should use deck-friendly transfer");
    ctx.expectTrue(plan.options.hfTamerIntensity < 0.05f, "HF prep should be off for quiet material");
    ctx.expectTrue(plan.tapeThreatScore < 0.20f, "threat score should stay low");
}

static void testAutoMasteringPlannerPicksTapePrepForHotBrightSource(TestContext& ctx)
{
    AudioFeatures hot;
    hot.integratedLUFS = -8.0f;
    hot.truePeakDbfs = -0.1f;
    hot.lfStereoCorrelation = 0.15f;
    hot.hfEnergyRatio = 2.6f;
    hot.clippingPercent = 0.02f;
    hot.psycho.hfTamerStrength = 0.72f;
    hot.psycho.roughnessAsper = 0.34f;
    hot.psycho.sharpnessAcum = 2.0f;
    hot.psycho.hfAboveMaskingDb = 8.0f;
    hot.psycho.streamingRingingIndex = 0.42f;

    const auto profile = CassetteProfile::fromFormulation(TapeFormulation::TypeII);
    const auto plan = CassetteMasteringPlanner::plan(hot, profile);

    ctx.expectTrue(!plan.options.maximumDigital, "hot bright material should use tape prep");
    ctx.expectTrue(plan.options.hfTamerIntensity >= 0.45f, "HF prep should be substantial");
    ctx.expectTrue(plan.tapeThreatScore > 0.35f, "threat score should reflect problematic source");
}

static void testHotCleanPopAutoMasterLandsOnKenwoodCap(TestContext& ctx)
{
    constexpr double sr = 48000.0;
    auto buffer = makeSineBuffer(sr, 10.0, 440.0f, 0.88f);
    applyGain(buffer, 2.6f);

    const auto profile = CassetteProfile::forRecording(RecordingDeck::KenwoodKX1100G,
                                                       TapeFormulation::TypeI);
    MasteringOptions opts;
    opts.skipQualityCompare = true;

    AdaptiveMasteringProcessor processor;
    processor.process(buffer, profile, opts, sr);
    const auto after = analyze(buffer, sr);

    ctx.expectTrue(after.integratedLUFS >= profile.maxIntegratedLUFS - 0.6f,
                   "LUFS should not sit far below Kenwood cap");
    ctx.expectTrue(after.integratedLUFS <= profile.maxIntegratedLUFS + 0.5f,
                   "LUFS should not exceed Kenwood cap");
    ctx.expectTrue(after.truePeakDbfs <= profile.truePeakCeilingDbfs + 0.25f,
                   "true peak should respect Kenwood ceiling");
}

static void testAutoMasteringPlannerHotCleanPopUsesLevelOnly(TestContext& ctx)
{
    AudioFeatures pop;
    pop.integratedLUFS = -9.76f;
    pop.truePeakDbfs = 0.15f;
    pop.lfStereoCorrelation = 0.95f;
    pop.hfEnergyRatio = 0.67f;
    pop.clippingPercent = 0.0f;
    pop.psycho.hfTamerStrength = 0.19f;
    pop.psycho.roughnessAsper = 0.227f;
    pop.psycho.sharpnessAcum = 1.19f;
    pop.psycho.hfAboveMaskingDb = 0.0f;
    pop.psycho.streamingRingingIndex = 0.32f;

    const auto profile = CassetteProfile::forRecording(RecordingDeck::KenwoodKX1100G,
                                                       TapeFormulation::TypeI);
    const auto plan = CassetteMasteringPlanner::plan(pop, profile);

    ctx.expectTrue(plan.options.maximumDigital, "hot clean pop should stay deck-friendly");
    ctx.expectTrue(plan.options.hfTamerIntensity < 0.05f, "HF tamer should stay off");
}

#if defined(CASSETTE_CHARLI_FIXTURE)
static void testCharliFixtureKenwoodMasteringRegression(TestContext& ctx)
{
    const juce::File fixture(CASSETTE_CHARLI_FIXTURE);
    const auto loaded = AudioFileLoader::loadToBufferWithDiagnostics(fixture);
    ctx.expectTrue(loaded.audio.hasValue(), "Charli fixture should load");
    if (!loaded.audio.hasValue())
        return;

    auto buffer = loaded.audio->buffer;
    const double sr = loaded.audio->sampleRate;
    const auto before = analyze(buffer, sr);

    const auto profile = CassetteProfile::forRecording(RecordingDeck::KenwoodKX1100G,
                                                       TapeFormulation::TypeI);
    MasteringOptions opts;
    opts.skipQualityCompare = true;

    AdaptiveMasteringProcessor processor;
    const auto result = processor.process(buffer, profile, opts, sr);
    const auto after = analyze(buffer, sr);

    ctx.expectTrue(before.integratedLUFS > profile.maxIntegratedLUFS + 0.5f,
                   "Charli fixture should start hotter than Kenwood cap");
    ctx.expectTrue(result.optionsUsed.maximumDigital,
                   "Charli fixture should use hot/clean level-only path");
    ctx.expectTrue(result.optionsUsed.hfTamerIntensity < 0.05f,
                   "Charli fixture should not engage HF tamer");
    // Record-side HF pre-emphasis lifts the treble before the deck; under a fixed
    // true-peak ceiling that trades a little integrated loudness (~1 LU here) for
    // HF fidelity, so the lower bound is wider than the legacy level-only contract.
    ctx.expectTrue(after.integratedLUFS >= profile.maxIntegratedLUFS - 1.3f,
                   "Charli mastered LUFS should not sit far below Kenwood cap");
    ctx.expectTrue(after.integratedLUFS <= profile.maxIntegratedLUFS + 0.35f,
                   "Charli mastered LUFS should land near Kenwood cap");
    ctx.expectTrue(after.truePeakDbfs <= profile.truePeakCeilingDbfs + 0.35f,
                   "Charli mastered true peak should respect Kenwood ceiling");
    ctx.expectTrue(after.integratedLUFS - before.integratedLUFS > -2.5f,
                   "Charli should not be over-attenuated (>2.5 LU cut vs source)");
}
#endif

static void testKenwoodKX1100ProfileExtendsHfHeadroom(TestContext& ctx)
{
    const auto generic = CassetteProfile::fromFormulation(TapeFormulation::TypeII);
    const auto kenwood = CassetteProfile::forRecording(RecordingDeck::KenwoodKX1100G, TapeFormulation::TypeII);

    ctx.expectTrue(kenwood.hfHeadroomKhz > generic.hfHeadroomKhz,
                   "KX-1100G Type II should allow more HF headroom");
    ctx.expectTrue(kenwood.sideHighCutHz > generic.sideHighCutHz,
                   "KX-1100G should use higher side HF cut frequency");
    ctx.expectTrue(kenwood.autoPrepRelief > 0.0f, "KX-1100G should reduce auto prep aggressiveness");
    ctx.expectTrue(!kenwood.emulateHxPro, "KX-1100G has no Dolby HX-Pro");
    ctx.expectTrue(kenwood.biasReductionOnHf > generic.biasReductionOnHf,
                   "KX-1100G prep should compensate missing HX-Pro in software");
}

static void testKenwoodTypeICalibratedTransferProfile(TestContext& ctx)
{
    const auto kenwoodTypeI = CassetteProfile::forRecording(RecordingDeck::KenwoodKX1100G,
                                                            TapeFormulation::TypeI);

    ctx.expectTrue(kenwoodTypeI.recordHfPreEmphasis, "Kenwood Type I should enable record HF pre-emphasis");
    ctx.expectTrue(kenwoodTypeI.softwareHxPro, "Kenwood Type I should use software HX-Pro headroom");
    ctx.expectTrue(kenwoodTypeI.recordHfPreEmphasisDb >= 2.5f,
                   "Kenwood Type I shelf should match empirically tuned v2 level");
    ctx.expectTrue(kenwoodTypeI.recordHfPreEmphasisDb2 < 0.05f,
                   "Kenwood Type I should use single shelf (dual shelf saturates ferric tape)");
    ctx.expectTrue(kenwoodTypeI.biasReductionOnHf > 0.0f,
                   "Kenwood Type I should bias HF saturation model toward tape headroom");
}

static void testKenwoodPlannerIsLessAggressiveOnBorderlineSource(TestContext& ctx)
{
    AudioFeatures borderline;
    borderline.integratedLUFS = -12.5f;
    borderline.truePeakDbfs = -1.2f;
    borderline.lfStereoCorrelation = 0.62f;
    borderline.hfEnergyRatio = 1.6f;
    borderline.clippingPercent = 0.0f;
    borderline.psycho.hfTamerStrength = 0.13f;
    borderline.psycho.roughnessAsper = 0.13f;
    borderline.psycho.sharpnessAcum = 1.35f;
    borderline.psycho.hfAboveMaskingDb = 3.5f;

    const auto genericProfile = CassetteProfile::fromFormulation(TapeFormulation::TypeIV);
    const auto kenwoodProfile = CassetteProfile::forRecording(RecordingDeck::KenwoodKX1100G, TapeFormulation::TypeIV);

    const auto genericPlan = CassetteMasteringPlanner::plan(borderline, genericProfile);
    const auto kenwoodPlan = CassetteMasteringPlanner::plan(borderline, kenwoodProfile);

    ctx.expectTrue(kenwoodPlan.tapeThreatScore <= genericPlan.tapeThreatScore,
                   "KX-1100 planner should score borderline material lower");
    ctx.expectTrue(kenwoodPlan.options.hfTamerIntensity <= genericPlan.options.hfTamerIntensity + 0.01f,
                   "KX-1100 should not exceed generic HF prep on borderline material");
}

static void testPreflightAddsCalibrationBlock(TestContext& ctx)
{
    constexpr double sr = 48000.0;
    juce::AudioBuffer<float> music(2, static_cast<int>(sr * 2.0));
    music.clear();

    const auto beforeSamples = music.getNumSamples();
    PreflightOptions opts;
    opts.artist = "Test";
    opts.title = "Track";
    PreflightTones::prependToBuffer(music, sr, opts);

    ctx.expectTrue(music.getNumSamples() > beforeSamples + static_cast<int>(sr * 50),
                   "pre-flight block should add >50 seconds of calibration audio");
}

static void testTruePeakDetectsInterSamplePeaks(TestContext& ctx)
{
    constexpr double sr = 48000.0;
    const int n = static_cast<int>(sr);
    juce::AudioBuffer<float> buffer(1, n);

    for (int i = 0; i < n; ++i)
    {
        const auto t = static_cast<double>(i) / sr;
        buffer.setSample(0, i, 0.85f * std::sin(2.0 * juce::MathConstants<double>::pi * 14000.0 * t));
    }

    const auto samplePeak = juce::Decibels::gainToDecibels(buffer.getMagnitude(0, 0, n), -100.0f);
    const auto truePeak = TruePeakMeter::measurePeakDbfs(buffer, sr);

    ctx.expectTrue(truePeak >= samplePeak - 0.05f, "true peak should be >= sample peak");
    ctx.expectTrue(truePeak <= 0.0f + 0.5f, "true peak should stay near full scale for loud sine");
}

static void testTruePeakLimiterRespectsCeiling(TestContext& ctx)
{
    constexpr double sr = 48000.0;
    auto buffer = makeSineBuffer(sr, 2.0, 440.0f, 0.99f);

    const auto before = TruePeakMeter::measurePeakDbfs(buffer, sr);

    TruePeakLimiter limiter;
    limiter.setThresholdDbfs(-1.0f);
    limiter.prepare(sr);
    limiter.process(buffer);

    const auto after = TruePeakMeter::measurePeakDbfs(buffer, sr);
    ctx.expectTrue(after < before - 0.5f, "limiter should reduce true peak on hot material");
    ctx.expectTrue(after <= -0.98f, "limiter should keep true peak at ceiling");
}

static float empiricalAmplitudeForTargetLufs(float targetLufs, double sr)
{
    const int n = static_cast<int>(sr * 5.0);
    juce::AudioBuffer<float> probe(2, n);
    constexpr float probeAmp = 0.5f;

    for (int i = 0; i < n; ++i)
    {
        const auto t = static_cast<double>(i) / sr;
        const float s = probeAmp * std::sin(2.0 * juce::MathConstants<double>::pi * 1000.0 * t);
        probe.setSample(0, i, s);
        probe.setSample(1, i, s);
    }

    const auto measured = LoudnessMeter::analyze(probe, sr).integratedLufs;
    const float scale = std::pow(10.0f, (targetLufs - measured) / 20.0f);
    return probeAmp * scale;
}

static void testBs1770ReferenceTone(TestContext& ctx)
{
    constexpr double sr = 48000.0;
    const float amp = empiricalAmplitudeForTargetLufs(-23.0f, static_cast<float>(sr));
    const int n = static_cast<int>(sr * 20.0);
    juce::AudioBuffer<float> buffer(2, n);

    for (int i = 0; i < n; ++i)
    {
        const auto t = static_cast<double>(i) / sr;
        const float s = amp * std::sin(2.0 * juce::MathConstants<double>::pi * 1000.0 * t);
        buffer.setSample(0, i, s);
        buffer.setSample(1, i, s);
    }

    const auto r = LoudnessMeter::analyze(buffer, sr);
    ctx.expectTrue(std::abs(r.integratedLufs + 23.0f) < 0.15f, "1 kHz EBU ref should read ~ -23 LUFS");
    ctx.expectTrue(r.loudnessRangeLu < 0.5f, "steady sine should have near-zero LRA");

    const float amp33 = empiricalAmplitudeForTargetLufs(-33.0f, static_cast<float>(sr));
    juce::AudioBuffer<float> quiet(2, n);
    for (int i = 0; i < n; ++i)
    {
        const auto t = static_cast<double>(i) / sr;
        const float s = amp33 * std::sin(2.0 * juce::MathConstants<double>::pi * 1000.0 * t);
        quiet.setSample(0, i, s);
        quiet.setSample(1, i, s);
    }
    const auto q = LoudnessMeter::analyze(quiet, sr);
    ctx.expectTrue(std::abs((q.integratedLufs - r.integratedLufs) + 10.0f) < 0.2f,
                   "10 LU step between -23 and -33 targets");
}

static void testMaskingNoiseGateReducesQuietNoise(TestContext& ctx)
{
    constexpr double sr = 48000.0;
    const int n = static_cast<int>(sr * 4.0);
    juce::AudioBuffer<float> buffer(2, n);

    for (int i = 0; i < n; ++i)
    {
        const auto t = static_cast<double>(i) / sr;
        const float tone = (t > 2.0 && t < 2.5f) ? 0.35f * std::sin(2.0 * juce::MathConstants<double>::pi * 440.0 * t)
                                                 : 0.0f;
        const float noise = 0.02f * (static_cast<float>(i % 17) / 17.0f - 0.5f);
        buffer.setSample(0, i, tone + noise);
        buffer.setSample(1, i, tone + noise);
    }

    AudioFeatures features;
    features.noiseFloorDbfs = -42.0f;

    const auto quietBefore = buffer.getRMSLevel(0, 0, static_cast<int>(0.5 * sr));
    MaskingNoiseGate gate;
    gate.process(buffer, sr, features, 1.0f);
    const auto quietAfter = buffer.getRMSLevel(0, 0, static_cast<int>(0.5 * sr));

    ctx.expectTrue(quietAfter < quietBefore * 0.85f, "noise gate should attenuate quiet noisy passages");
}

static void testWowFlutterDetectsModulatedTone(TestContext& ctx)
{
    constexpr double sr = 48000.0;
    const int n = static_cast<int>(sr * 10.0);
    juce::AudioBuffer<float> buffer(1, n);

    for (int i = 0; i < n; ++i)
    {
        const auto t = static_cast<double>(i) / sr;
        const float am = 1.0f + 0.10f * std::sin(2.0 * juce::MathConstants<double>::pi * 1.5 * t);
        buffer.setSample(0, i, am * 0.55f * std::sin(2.0 * juce::MathConstants<double>::pi * 440.0 * t));
    }

    const auto wf = WowFlutterAnalyzer::analyze(buffer, sr);
    ctx.expectTrue(wf.wowPercent > 0.5f || wf.wowFlutterIndex > 0.015f,
                   "AM-modulated tone should show wow/flutter energy");
    ctx.expectTrue(wf.wowFlutterIndex > 0.01f, "composite wow/flutter index should rise");
}

static void testSpectrumBandRatios(TestContext& ctx)
{
    constexpr double sr = 48000.0;
    auto lowTone = makeSineBuffer(sr, 2.0, 80.0f, 0.5f);
    auto highTone = makeSineBuffer(sr, 2.0, 8000.0f, 0.5f);

    const auto lowBands = SpectrumAnalyzer::bandEnergyRatios(lowTone, sr);
    const auto highBands = SpectrumAnalyzer::bandEnergyRatios(highTone, sr);

    const float lowSum = lowBands.lf + lowBands.mf + lowBands.hf;
    const float highSum = highBands.lf + highBands.mf + highBands.hf;

    ctx.expectTrue(lowSum > 0.9f && lowSum < 1.1f, "LF tone band ratios should sum to ~1");
    ctx.expectTrue(highSum > 0.9f && highSum < 1.1f, "HF tone band ratios should sum to ~1");
    ctx.expectTrue(lowBands.lf > highBands.lf, "80 Hz tone should have more LF energy than 8 kHz");
    ctx.expectTrue(highBands.hf > lowBands.hf, "8 kHz tone should have more HF energy than 80 Hz");
}

static void testStnGridModelBounded(TestContext& ctx)
{
    constexpr double sr = 48000.0;
    auto buffer = makeSineBuffer(sr, 2.0, 660.0f, 0.9f);
    CassetteProfile profile = CassetteProfile::fromFormulation(TapeFormulation::TypeII);
    profile.saturationDrive = 1.15f;
    profile.biasReductionOnHf = 0.08f;

    StnGridModel grid;
    grid.process(buffer, profile, sr);

    ctx.expectTrue(bufferIsFinite(buffer), "STN grid must stay finite");
    ctx.expectTrue(peakAbs(buffer) < 1.1f, "STN grid output bounded");
}

static void testStnSaturationStable(TestContext& ctx)
{
    constexpr double sr = 48000.0;
    auto buffer = makeSineBuffer(sr, 3.0, 880.0f, 0.95f);
    CassetteProfile profile = CassetteProfile::fromFormulation(TapeFormulation::VintageWalkman);

    TapeAwareSoftClipper clipper;
    clipper.prepare(sr, 4);
    clipper.setProfile(profile);
    clipper.process(buffer);

    ctx.expectTrue(bufferIsFinite(buffer), "STN saturation must stay finite");
    ctx.expectTrue(peakAbs(buffer) < 1.05f, "STN output should remain bounded");
}

#if defined(CASSETTE_HAS_ONNX) && defined(CASSETTE_STN_MODEL)
static void testOnnxStnRunnerMatchesGrid(TestContext& ctx)
{
    const juce::File modelPath(CASSETTE_STN_MODEL);
    if (!modelPath.existsAsFile())
    {
        std::cout << "SKIP: ONNX model not found at " << modelPath.getFullPathName() << "\n";
        return;
    }

    setenv("CASSETTE_STN_MODEL", modelPath.getFullPathName().toRawUTF8(), 1);

    constexpr double sr = 48000.0;
    auto gridBuf = makeSineBuffer(sr, 0.5, 660.0f, 0.85f);
    auto onnxBuf = makeSineBuffer(sr, 0.5, 660.0f, 0.85f);

    CassetteProfile profile = CassetteProfile::fromFormulation(TapeFormulation::VintageWalkman);
    profile.saturationDrive = 1.1f;
    profile.biasReductionOnHf = 0.06f;

    StnGridModel grid;
    grid.reset();
    grid.process(gridBuf, profile, sr);

    OnnxStnRunner onnx;
    ctx.expectTrue(onnx.tryLoadDefaultModel(), "ONNX STN model should load");
    onnx.reset();
    onnx.process(onnxBuf, profile, sr);

    ctx.expectTrue(bufferIsFinite(onnxBuf), "ONNX STN output must stay finite");
    ctx.expectTrue(peakAbs(onnxBuf) < 1.1f, "ONNX STN output bounded");

    const float gridPeak = peakAbs(gridBuf);
    const float onnxPeak = peakAbs(onnxBuf);
    ctx.expectTrue(std::abs(gridPeak - onnxPeak) < 0.15f,
                   "ONNX STN peak should track grid model closely");
}
#endif

static void testAudioResamplerChangesSampleCount(TestContext& ctx)
{
    constexpr double srcRate = 44100.0;
    constexpr double dstRate = 48000.0;
    auto buffer = makeSineBuffer(srcRate, 1.0, 440.0f, 0.5f);
    const int srcSamples = buffer.getNumSamples();

    resampleBufferLinear(buffer, srcRate, dstRate);

    const int expected = static_cast<int>(std::llround(srcSamples * dstRate / srcRate));
    ctx.expectTrue(std::abs(buffer.getNumSamples() - expected) <= 2,
                   "linear resampler should scale sample count by rate ratio");
    ctx.expectTrue(bufferIsFinite(buffer), "resampled buffer must stay finite");
    ctx.expectTrue(peakAbs(buffer) > 0.1f, "resampled sine should remain audible");
}

static void testPerceptualPlaybackProcessorStable(TestContext& ctx)
{
    constexpr double sr = 48000.0;
    auto buffer = makeSineBuffer(sr, 0.5, 120.0f, 0.35f, 2);
    const auto dryPeak = peakAbs(buffer);

    PerceptualPlaybackProcessor playback;
    playback.prepare(sr, buffer.getNumSamples());
    PerceptualPlaybackSettings settings;
    settings.virtualSubEnabled = true;
    settings.crossfeedEnabled = true;
    settings.stereoWidenEnabled = true;
    playback.setSettings(settings);
    playback.process(buffer);

    ctx.expectTrue(bufferIsFinite(buffer), "perceptual playback output must stay finite");
    ctx.expectTrue(peakAbs(buffer) > dryPeak * 0.2f, "perceptual playback should not silence material");
}

static void testPreviewEngineMonitoringPath(TestContext& ctx)
{
    constexpr double sr = 48000.0;
    auto source = makeSineBuffer(sr, 0.4, 180.0f, 0.4f, 2);

    PreviewEngine engine;
    engine.setBuffer(source, sr);
    engine.setMonitoringEnabled(true);
    engine.prepareToPlay(512, sr);
    engine.setPlayheadSec(0.0);
    engine.setPlaying(true);

    juce::AudioBuffer<float> block(2, 512);
    for (int i = 0; i < 16; ++i)
    {
        juce::AudioSourceChannelInfo info(block);
        engine.getNextAudioBlock(info);
    }

    ctx.expectTrue(bufferIsFinite(block), "preview monitoring path must stay finite");
    ctx.expectTrue(peakAbs(block) > 0.01f, "preview monitoring path should output audio");
}

static void testDynamicEQAttenuatesHotSignal(TestContext& ctx)
{
    constexpr double sr = 48000.0;
    auto buffer = makeSineBuffer(sr, 0.5, 1000.0f, 0.95f);
    const auto before = peakAbs(buffer);

    DynamicEQ eq(200.0f, 8000.0f, -30.0f, 4.0f);
    eq.prepare(sr, buffer.getNumSamples());
    eq.process(buffer);

    ctx.expectTrue(peakAbs(buffer) < before, "dynamic EQ should attenuate hot material");
    ctx.expectTrue(bufferIsFinite(buffer), "dynamic EQ output must stay finite");
}

static void testTransientShaperBoostsAttack(TestContext& ctx)
{
    constexpr double sr = 48000.0;
    juce::AudioBuffer<float> buffer(1, static_cast<int>(sr * 0.05));
    buffer.clear();
    buffer.setSample(0, 200, 1.0f);
    const auto before = peakAbs(buffer);

    TransientShaper shaper;
    shaper.setAttackDb(8.0f);
    shaper.setSustainDb(0.0f);
    shaper.prepare(sr);
    shaper.process(buffer);

    ctx.expectTrue(peakAbs(buffer) >= before, "transient shaper should boost attack transients");
    ctx.expectTrue(bufferIsFinite(buffer), "transient shaper output must stay finite");
}

static void testWowFlutterEmulatorStable(TestContext& ctx)
{
    constexpr double sr = 48000.0;
    auto buffer = makeSineBuffer(sr, 1.5, 440.0f, 0.45f, 2);

    WowFlutterEmulator wow;
    wow.prepare(sr, buffer.getNumSamples(), buffer.getNumChannels());
    wow.setProfile(CassetteProfile::fromFormulation(TapeFormulation::TypeIV));
    wow.process(buffer);

    ctx.expectTrue(bufferIsFinite(buffer), "wow/flutter output must stay finite");
    ctx.expectTrue(peakAbs(buffer) > 0.05f, "wow/flutter should keep material audible");
}

static void testTapeLengthPresets(TestContext& ctx)
{
    const auto c60 = tapeLengthSpecForPreset(TapeLengthPreset::C60, 45.0);
    const auto c90 = tapeLengthSpecForPreset(TapeLengthPreset::C90, 45.0);
    const auto c120 = tapeLengthSpecForPreset(TapeLengthPreset::C120, 45.0);
    const auto custom = tapeLengthSpecForPreset(TapeLengthPreset::Custom, 52.5);

    ctx.expectNear(static_cast<float>(c60.minutesPerSide), 30.0f, 0.01f, "C60 should be 30 min per side");
    ctx.expectNear(static_cast<float>(c90.minutesPerSide), 45.0f, 0.01f, "C90 should be 45 min per side");
    ctx.expectNear(static_cast<float>(c120.minutesPerSide), 60.0f, 0.01f, "C120 should be 60 min per side");
    ctx.expectNear(static_cast<float>(custom.minutesPerSide), 52.5f, 0.01f, "custom preset should preserve minutes");
    ctx.expectTrue(custom.label.contains("52"), "custom preset label should mention minutes");
}

static void testFolderMixFormatDuration(TestContext& ctx)
{
    ctx.expectTrue(FolderMixBuilder::formatDuration(90.0) == "1:30",
                   "formatDuration should render mm:ss");
    ctx.expectTrue(FolderMixBuilder::formatDuration(45.0 * 60.0 + 7.0) == "45:07",
                   "formatDuration should zero-pad seconds");
}

static void testDropPayloadFolderWinsOverAudioFile(TestContext& ctx)
{
    const auto dir = juce::File::getSpecialLocation(juce::File::tempDirectory)
                         .getChildFile("deck_drop_payload_test");
    dir.createDirectory();

    juce::StringArray mixed;
    mixed.add(dir.getFullPathName());
    mixed.add(dir.getChildFile("01.flac").getFullPathName());
    ctx.expectTrue(classifyDropPayload(mixed) == DropPayloadKind::Folder,
                   "folder path in mixed drag payload should classify as folder");

    dir.deleteRecursively();
}

static void testKenwoodPreflightUsesAlternateToneStack(TestContext& ctx)
{
    constexpr double sr = 48000.0;
    juce::AudioBuffer<float> standard(2, static_cast<int>(sr));
    juce::AudioBuffer<float> kenwood(2, static_cast<int>(sr));
    standard.clear();
    kenwood.clear();

    PreflightOptions stdOpts;
    stdOpts.title = "Test";
    PreflightTones::prependToBuffer(standard, sr, stdOpts);

    PreflightOptions kenOpts;
    kenOpts.title = "Test";
    kenOpts.kenwoodKX1100Calibration = true;
    PreflightTones::prependToBuffer(kenwood, sr, kenOpts);

    ctx.expectTrue(kenwood.getNumSamples() != standard.getNumSamples(),
                   "Kenwood preflight should use a different tone stack than the standard block");
    ctx.expectTrue(kenwood.getNumSamples() > static_cast<int>(sr * 50),
                   "Kenwood preflight block should still exceed 50 seconds");
    ctx.expectTrue(standard.getNumSamples() > static_cast<int>(sr * 50),
                   "standard preflight block should exceed 50 seconds");
}

static void testMixtapeEditorReorderUndoAndCrossSideMove(TestContext& ctx)
{
    FolderScanResult scan;
    scan.success = true;
    scan.gapBetweenTracksSec = 2.0;

    for (int i = 0; i < 4; ++i)
    {
        FolderTrackInfo t;
        t.displayName = "Track " + juce::String(i + 1);
        t.durationSec = 8.0 * 60.0;
        scan.tracks.push_back(t);
    }

    scan.totalDurationSec = 4.0 * 8.0 * 60.0 + 3.0 * scan.gapBetweenTracksSec;
    const auto tape = tapeLengthSpecForPreset(TapeLengthPreset::C90, 45.0);
    const auto fit = FolderMixBuilder::analyzeFit(scan, tape);

    MixtapeEditController editor;
    editor.loadFromScan(scan, fit);
    const auto originalFirst = editor.sideA().front().displayName;

    ctx.expectTrue(editor.reorderWithinSide(0, 0, 1), "editor should reorder within side A");
    ctx.expectTrue(editor.sideA().front().displayName != originalFirst,
                   "reorder should change the first row");

    editor.getUndoManager().undo();
    ctx.expectTrue(editor.sideA().front().displayName == originalFirst,
                   "undo should restore the original first row");

    const int sideBCount = static_cast<int>(editor.sideB().size());
    ctx.expectTrue(editor.moveToSide(0, 0, 1, sideBCount), "editor should move a track to side B");
    ctx.expectTrue(editor.sideB().size() == static_cast<size_t>(sideBCount + 1),
                   "side B should grow after cross-side move");
    ctx.expectTrue(editor.hasManualEdits(), "manual edits flag should be set after move");
}

static void testMixtapeEditorRebalanceClearsOverflow(TestContext& ctx)
{
    FolderScanResult scan;
    scan.success = true;
    scan.gapBetweenTracksSec = 2.0;

    for (int i = 0; i < 4; ++i)
    {
        FolderTrackInfo t;
        t.displayName = "Track " + juce::String(i + 1);
        t.durationSec = 8.0 * 60.0;
        scan.tracks.push_back(t);
    }

    scan.totalDurationSec = 4.0 * 8.0 * 60.0 + 3.0 * scan.gapBetweenTracksSec;
    const auto tape = tapeLengthSpecForPreset(TapeLengthPreset::C60, 30.0);
    const auto fit = FolderMixBuilder::analyzeFit(scan, tape);

    MixtapeEditController editor;
    editor.loadFromScan(scan, fit);

    while (!editor.sideB().empty())
        ctx.expectTrue(editor.moveToSide(1, 0, 0, static_cast<int>(editor.sideA().size())),
                       "editor should move side B tracks onto side A");

    ctx.expectTrue(editor.hasSideOverflow(tape), "stacking all tracks on side A should overflow C60");
    editor.rebalance(tape);
    ctx.expectTrue(!editor.hasSideOverflow(tape), "rebalance should clear side overflow");
    ctx.expectTrue(editor.canPrepare(tape), "rebalanced layout should be prepare-ready");
    ctx.expectTrue(!editor.hasManualEdits(), "rebalance should clear manual edits");
    ctx.expectTrue(editor.sideA().size() + editor.sideB().size() == 4,
                   "rebalance should keep every track");
}

static void testMixtapeEditorMoveRowAndBatchDelete(TestContext& ctx)
{
    FolderScanResult scan;
    scan.success = true;
    scan.gapBetweenTracksSec = 2.0;

    for (int i = 0; i < 5; ++i)
    {
        FolderTrackInfo t;
        t.displayName = "Track " + juce::String(i + 1);
        t.durationSec = 6.0 * 60.0;
        scan.tracks.push_back(t);
    }

    scan.totalDurationSec = 5.0 * 6.0 * 60.0 + 4.0 * scan.gapBetweenTracksSec;
    const auto tape = tapeLengthSpecForPreset(TapeLengthPreset::C90, 45.0);
    const auto fit = FolderMixBuilder::analyzeFit(scan, tape);

    MixtapeEditController editor;
    editor.loadFromScan(scan, fit);
    ctx.expectTrue(editor.sideA().size() > 1, "fixture should start with multiple side A rows");

    const auto originalFirst = editor.sideA().front().displayName;
    const auto originalSecond = editor.sideA()[1].displayName;

    ctx.expectTrue(editor.moveRowDown(0, 0), "moveRowDown should move first row down");
    ctx.expectTrue(editor.sideA()[0].displayName == originalSecond,
                   "first row should receive the former second track");
    ctx.expectTrue(editor.sideA()[1].displayName == originalFirst,
                   "second row should receive the former first track");
    ctx.expectTrue(editor.moveRowUp(0, 1), "moveRowUp should restore row order");

    const auto beforeDelete = editor.mergedFullScan().tracks.size();
    ctx.expectTrue(editor.deleteTracks({ { 0, 0 }, { 0, 1 } }), "batch delete should remove two tracks");
    ctx.expectTrue(editor.mergedFullScan().tracks.size() == beforeDelete - 2,
                   "batch delete should reduce merged track count");
    ctx.expectTrue(editor.computeFit(tape).requiredSec < fit.requiredSec,
                   "batch delete should reduce required duration");
}

static void testMixtapeEditorMultiCassetteLayouts(TestContext& ctx)
{
    FolderScanResult scan;
    scan.success = true;
    scan.gapBetweenTracksSec = 2.0;

    for (int i = 0; i < 7; ++i)
    {
        FolderTrackInfo t;
        t.displayName = "Track " + juce::String(i + 1);
        t.durationSec = 20.0 * 60.0;
        scan.tracks.push_back(t);
    }

    scan.totalDurationSec = 7.0 * 20.0 * 60.0 + 6.0 * scan.gapBetweenTracksSec;
    const auto tape = tapeLengthSpecForPreset(TapeLengthPreset::C120, 60.0);
    const auto fit = FolderMixBuilder::analyzeFit(scan, tape);

    MixtapeEditController editor;
    editor.loadFromScan(scan, fit);
    ctx.expectTrue(editor.getCassetteCount() == 2, "long album should load as two cassettes");

    const auto layout0 = editor.layoutForCassette(0);
    const auto layout1 = editor.layoutForCassette(1);
    const size_t totalTracks = layout0.sideA.size() + layout0.sideB.size() + layout1.sideA.size()
                               + layout1.sideB.size();
    ctx.expectTrue(totalTracks == 7, "cassette layouts should cover every track");
    ctx.expectTrue(!layout0.sideA.empty(), "first cassette should have side A tracks");
    ctx.expectTrue(!layout1.sideA.empty(), "second cassette should have side A tracks");

    const auto project = editor.buildProjectForCassette(1, "tape-2", CassetteProfile::fromFormulation(TapeFormulation::TypeIV), {}, tape.minutesPerSide * 60.0);
    ctx.expectTrue(!project.sideA.clips.empty(), "cassette-specific project should include clips");
}

static void testBuildSequentialProjectCreatesTimeline(TestContext& ctx)
{
    FolderScanResult scan;
    scan.success = true;
    scan.gapBetweenTracksSec = 2.0;

    for (int i = 0; i < 3; ++i)
    {
        FolderTrackInfo t;
        t.displayName = "Track " + juce::String(i + 1);
        t.durationSec = 4.0 * 60.0;
        t.file = juce::File("/tmp/deck-seq-" + juce::String(i + 1) + ".wav");
        scan.tracks.push_back(t);
    }

    scan.totalDurationSec = 3.0 * 4.0 * 60.0 + 2.0 * scan.gapBetweenTracksSec;
    const auto profile = CassetteProfile::fromFormulation(TapeFormulation::TypeIV);
    const auto project = FolderMixBuilder::buildSequentialProject(scan, "sequential", profile, {});

    ctx.expectTrue(project.sideA.clips.size() + project.sideB.clips.size() == 3,
                   "sequential project should place every track on a timeline");
    ctx.expectTrue(project.sideA.usedDurationSec() > 0.0 || project.sideB.usedDurationSec() > 0.0,
                   "sequential project should allocate clip durations");
}

static void testMidSideRoundTripPreservesLevel(TestContext& ctx)
{
    constexpr double sr = 48000.0;
    auto buffer = makeSineBuffer(sr, 0.25, 440.0f, 0.42f, 2);
    const auto before = peakAbs(buffer);

    MidSideProcessor::toMidSide(buffer);
    MidSideProcessor::fromMidSide(buffer);

    ctx.expectTrue(bufferIsFinite(buffer), "mid/side round trip must stay finite");
    ctx.expectNear(peakAbs(buffer), before, 0.05f, "mid/side round trip should preserve level");
}

static void testRubberBandWowProcessorStable(TestContext& ctx)
{
    constexpr double sr = 48000.0;
    auto buffer = makeSineBuffer(sr, 1.0, 330.0f, 0.4f, 2);

    RubberBandWowProcessor wow;
    wow.prepare(sr, buffer.getNumSamples(), buffer.getNumChannels());
    wow.setProfile(CassetteProfile::fromFormulation(TapeFormulation::CheapPortable));
    wow.process(buffer);

    ctx.expectTrue(bufferIsFinite(buffer), "rubberband wow/flutter output must stay finite");
    ctx.expectTrue(peakAbs(buffer) > 0.03f, "rubberband wow/flutter should keep material audible");
}

static void testPerceptualQualityIdenticalBuffersPass(TestContext& ctx)
{
    constexpr double sr = 48000.0;
    auto buffer = makeSineBuffer(sr, 0.5, 500.0f, 0.35f, 2);
    const auto report = PerceptualQualityReport::compare(buffer, buffer, sr);

    ctx.expectTrue(report.passesQualityGate, "identical reference/processed pair should pass quality gate");
    ctx.expectTrue(report.objectiveDifferenceGrade >= -0.2f,
                   "identical buffers should score near transparent");
}

static void testWavExporterRoundTrip(TestContext& ctx)
{
    constexpr double sr = 48000.0;
    auto buffer = makeSineBuffer(sr, 0.2, 880.0f, 0.33f, 2);
    const auto out = juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("deck_wav_roundtrip.wav");
    out.deleteFile();

    ctx.expectTrue(WavExporter::writeWav32Float(out, buffer, sr), "wav exporter should write a file");
    const auto loaded = AudioFileLoader::loadToBuffer(out);
    ctx.expectTrue(loaded.hasValue(), "exported wav should reload");
    if (loaded.hasValue())
    {
        ctx.expectTrue(loaded->buffer.getNumChannels() >= 1, "reloaded wav should have channels");
        ctx.expectTrue(std::abs(loaded->buffer.getNumSamples() - buffer.getNumSamples()) <= 2,
                       "reloaded wav should preserve sample length");
        ctx.expectNear(peakAbs(loaded->buffer), peakAbs(buffer), 0.08f,
                       "reloaded wav should preserve level approximately");
    }

    out.deleteFile();
}

static void testPreflightExportIncreasesPayload(TestContext& ctx)
{
    constexpr double sr = 48000.0;
    juce::AudioBuffer<float> plain = makeSineBuffer(sr, 4.0, 440.0f, 0.25f, 2);
    juce::AudioBuffer<float> withPreflight = plain;

    PreflightOptions opts;
    opts.title = "Export";
    opts.sideLabel = "Side A";
    PreflightTones::prependToBuffer(withPreflight, sr, opts);

    const auto plainFile = juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("deck_plain.wav");
    const auto preflightFile = juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("deck_preflight.wav");
    plainFile.deleteFile();
    preflightFile.deleteFile();

    ctx.expectTrue(WavExporter::writeWav32Float(plainFile, plain, sr), "plain export should succeed");
    ctx.expectTrue(WavExporter::writeWav32Float(preflightFile, withPreflight, sr), "preflight export should succeed");
    ctx.expectTrue(preflightFile.getSize() > plainFile.getSize(),
                   "preflight export should produce a larger wav than music-only export");
    ctx.expectTrue(withPreflight.getNumSamples() > plain.getNumSamples(),
                   "preflight buffer should be longer than source music");

    plainFile.deleteFile();
    preflightFile.deleteFile();
}

static void testAudioFileLoaderRejectsJunkExtension(TestContext& ctx)
{
    const auto junk = juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("deck_not_audio.txt");
    junk.replaceWithText("not audio");
    ctx.expectTrue(!AudioFileLoader::isSupportedAudioFile(junk), "non-audio extension should be rejected");
    junk.deleteFile();
}

static void testCassetteAutoMasterGainHelper(TestContext& ctx)
{
    ctx.expectNear(CassetteAutoMaster::calculateGainForTargetLUFS(-16.0f, -13.0f), 3.0f, 0.05f,
                   "gain helper should move toward target LUFS");
    ctx.expectNear(CassetteAutoMaster::calculateGainForTargetLUFS(-12.0f, -12.0f), 0.0f, 0.01f,
                   "gain helper should be zero at target");
}

static void testMixtapeEditorActiveCassetteSwitch(TestContext& ctx)
{
    FolderScanResult scan;
    scan.success = true;
    scan.gapBetweenTracksSec = 2.0;

    for (int i = 0; i < 7; ++i)
    {
        FolderTrackInfo t;
        t.displayName = "Track " + juce::String(i + 1);
        t.durationSec = 20.0 * 60.0;
        scan.tracks.push_back(t);
    }

    scan.totalDurationSec = 7.0 * 20.0 * 60.0 + 6.0 * scan.gapBetweenTracksSec;
    const auto tape = tapeLengthSpecForPreset(TapeLengthPreset::C120, 60.0);
    const auto fit = FolderMixBuilder::analyzeFit(scan, tape);

    MixtapeEditController editor;
    editor.loadFromScan(scan, fit);
    const auto cassette0First = editor.sideA().front().displayName;

    editor.setActiveCassetteIndex(1);
    ctx.expectTrue(editor.getActiveCassetteIndex() == 1, "active cassette index should switch");
    const auto cassette1First = editor.sideA().front().displayName;
    ctx.expectTrue(cassette1First != cassette0First, "second cassette should expose a different side A");

    if (editor.sideA().size() > 1)
    {
        editor.reorderWithinSide(0, 0, 1);
        const auto editedFirst = editor.sideA().front().displayName;
        editor.setActiveCassetteIndex(0);
        ctx.expectTrue(editor.sideA().front().displayName == cassette0First,
                       "switching away should show the first cassette layout");
        editor.setActiveCassetteIndex(1);
        ctx.expectTrue(editor.sideA().front().displayName == editedFirst,
                       "switching back should restore the edited second cassette layout");
    }
}

static void testFolderFitReportSummary(TestContext& ctx)
{
    FolderScanResult scan;
    scan.success = true;
    scan.gapBetweenTracksSec = 2.0;
    FolderTrackInfo t;
    t.durationSec = 20.0 * 60.0;
    scan.tracks.push_back(t);
    scan.totalDurationSec = t.durationSec;

    const auto tape = tapeLengthSpecForPreset(TapeLengthPreset::C60, 30.0);
    const auto report = FolderMixBuilder::analyzeFit(scan, tape);
    const auto summary = report.summary();

    ctx.expectTrue(report.fitsOneSide, "20 minute track should fit on C60 side A");
    ctx.expectTrue(summary.contains("C60"), "fit summary should mention tape preset");
    ctx.expectTrue(summary.contains("fits"), "fit summary should state that material fits");
}

int main()
{
    TestContext ctx;
    std::cout << "Deck DSP tests\n";

    testHotMasterTrimmedToProfileCap(ctx);
    testModerateMasterStaysNearSourceLoudness(ctx);
    testOutputStableAndAudible(ctx);
    testLfMonoStageAttenuatesSideBass(ctx);
    testAdaptiveHfTamingDoesNotBoostHighs(ctx);
    testSideLfMonoInFullTapePrep(ctx);
    testDisabledStereoTighteningPreservesWideBass(ctx);
    testDisabledTruePeakLimiterAllowsHotPeaks(ctx);
    testPsychoacousticMetricsPresent(ctx);
    testRoughnessHigherOnModulatedHf(ctx);
    testAdaptiveFallbackProducesQualityReport(ctx);
    testPeaqBasicEvaluation(ctx);
    testRoughnessDeEsserReducesRinging(ctx);
    testCheapPortableLouderCapThanHighEndDeck(ctx);
    testMixtapeSideLengthValidation(ctx);
    testFolderMixSplitsAcrossTwoSides(ctx);
    testAnalyzeLayoutRespectsManualSplit(ctx);
    testMixtapeEditorDeleteRecalculatesFit(ctx);
    testMixtapeEditorSyncConsolidatesToOneCassette(ctx);
    testMultiCassetteSplitWhenAlbumExceedsOneTape(ctx);
    testAutoMasteringPlannerPicksMinimalForQuietSource(ctx);
    testAutoMasteringPlannerPicksTapePrepForHotBrightSource(ctx);
    testHotCleanPopAutoMasterLandsOnKenwoodCap(ctx);
    testAutoMasteringPlannerHotCleanPopUsesLevelOnly(ctx);
#if defined(CASSETTE_CHARLI_FIXTURE)
    testCharliFixtureKenwoodMasteringRegression(ctx);
#endif
    testKenwoodKX1100ProfileExtendsHfHeadroom(ctx);
    testKenwoodTypeICalibratedTransferProfile(ctx);
    testKenwoodPlannerIsLessAggressiveOnBorderlineSource(ctx);
    testPreflightAddsCalibrationBlock(ctx);
    testTruePeakDetectsInterSamplePeaks(ctx);
    testTruePeakLimiterRespectsCeiling(ctx);
    testBs1770ReferenceTone(ctx);
    testMaskingNoiseGateReducesQuietNoise(ctx);
    testWowFlutterDetectsModulatedTone(ctx);
    testSpectrumBandRatios(ctx);
    testStnGridModelBounded(ctx);
    testStnSaturationStable(ctx);
#if defined(CASSETTE_HAS_ONNX) && defined(CASSETTE_STN_MODEL)
    testOnnxStnRunnerMatchesGrid(ctx);
#endif

    testAudioResamplerChangesSampleCount(ctx);
    testPerceptualPlaybackProcessorStable(ctx);
    testPreviewEngineMonitoringPath(ctx);
    testDynamicEQAttenuatesHotSignal(ctx);
    testTransientShaperBoostsAttack(ctx);
    testWowFlutterEmulatorStable(ctx);
    testTapeLengthPresets(ctx);
    testFolderMixFormatDuration(ctx);
    testDropPayloadFolderWinsOverAudioFile(ctx);
    testKenwoodPreflightUsesAlternateToneStack(ctx);
    testMixtapeEditorReorderUndoAndCrossSideMove(ctx);
    testMixtapeEditorRebalanceClearsOverflow(ctx);
    testMixtapeEditorMoveRowAndBatchDelete(ctx);
    testMixtapeEditorMultiCassetteLayouts(ctx);
    testBuildSequentialProjectCreatesTimeline(ctx);
    testMidSideRoundTripPreservesLevel(ctx);
    testRubberBandWowProcessorStable(ctx);
    testPerceptualQualityIdenticalBuffersPass(ctx);
    testWavExporterRoundTrip(ctx);
    testPreflightExportIncreasesPayload(ctx);
    testAudioFileLoaderRejectsJunkExtension(ctx);
    testCassetteAutoMasterGainHelper(ctx);
    testMixtapeEditorActiveCassetteSwitch(ctx);
    testFolderFitReportSummary(ctx);

    runFixtureAlbumIntegrationTests(ctx);

    std::cout << "----------------------------------------\n";
    if (ctx.failures == 0)
    {
        std::cout << "All tests passed.\n";
        return 0;
    }

    std::cerr << ctx.failures << " test(s) failed.\n";
    return 1;
}
