#include "TestHelpers.h"
#include <cstdlib>
#include "../Source/project/MixtapeProject.h"
#include "../Source/project/FolderMixBuilder.h"
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
#include "../Source/dsp/graph/AdaptiveMasteringProcessor.h"

using namespace cassette;
using namespace cassette::test;

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
    ctx.expectTrue(report.split.sideADurationSec <= report.allowedSec + 1.0,
                   "side A within capacity");
    ctx.expectTrue(report.split.sideBDurationSec <= report.allowedSec + 1.0,
                   "side B within capacity");
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
        ctx.expectTrue(cassette.sideADurationSec <= report.allowedSec + 1.0, "cassette side A within cap");
        if (cassette.hasSideB)
            ctx.expectTrue(cassette.sideBDurationSec <= report.allowedSec + 1.0, "cassette side B within cap");
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
    ctx.expectTrue(after.integratedLUFS >= profile.maxIntegratedLUFS - 0.5f,
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

int main()
{
    TestContext ctx;
    std::cout << "CassetteAutoMaster DSP tests\n";

    testHotMasterTrimmedToProfileCap(ctx);
    testModerateMasterStaysNearSourceLoudness(ctx);
    testOutputStableAndAudible(ctx);
    testLfMonoStageAttenuatesSideBass(ctx);
    testAdaptiveHfTamingDoesNotBoostHighs(ctx);
    testSideLfMonoInFullTapePrep(ctx);
    testPsychoacousticMetricsPresent(ctx);
    testRoughnessHigherOnModulatedHf(ctx);
    testAdaptiveFallbackProducesQualityReport(ctx);
    testPeaqBasicEvaluation(ctx);
    testRoughnessDeEsserReducesRinging(ctx);
    testCheapPortableLouderCapThanHighEndDeck(ctx);
    testMixtapeSideLengthValidation(ctx);
    testFolderMixSplitsAcrossTwoSides(ctx);
    testMultiCassetteSplitWhenAlbumExceedsOneTape(ctx);
    testAutoMasteringPlannerPicksMinimalForQuietSource(ctx);
    testAutoMasteringPlannerPicksTapePrepForHotBrightSource(ctx);
    testHotCleanPopAutoMasterLandsOnKenwoodCap(ctx);
    testAutoMasteringPlannerHotCleanPopUsesLevelOnly(ctx);
#if defined(CASSETTE_CHARLI_FIXTURE)
    testCharliFixtureKenwoodMasteringRegression(ctx);
#endif
    testKenwoodKX1100ProfileExtendsHfHeadroom(ctx);
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

    std::cout << "----------------------------------------\n";
    if (ctx.failures == 0)
    {
        std::cout << "All tests passed.\n";
        return 0;
    }

    std::cerr << ctx.failures << " test(s) failed.\n";
    return 1;
}
