#include "CassetteAutoMaster.h"
#include "filters/BiquadFilter.h"
#include "stereo/MidSideProcessor.h"
#include "dynamics/DynamicEQ.h"
#include "dynamics/TransientShaper.h"
#include "dynamics/TruePeakLimiter.h"
#include "dynamics/RoughnessDeEsser.h"
#include "dynamics/MaskingNoiseGate.h"
#include "tape/WowFlutterEmulator.h"
#include "tape/RubberBandWowProcessor.h"
#include "ml/TapeAwareSoftClipper.h"
#include "../analysis/EssentiaAnalyzer.h"
#include "../analysis/TruePeakMeter.h"
#include <random>

namespace cassette
{

namespace
{

using ShelfDuplicator = juce::dsp::ProcessorDuplicator<juce::dsp::IIR::Filter<float>,
                                                       juce::dsp::IIR::Coefficients<float>>;

constexpr float kInfraHpfBesselQ = 0.577f;
constexpr float kLoudnessToleranceLu = 0.2f;
constexpr float kMaxPostMakeupDb = 4.5f;
constexpr float kMaxPostCutDb = 2.0f;

void applyShelfGain(juce::AudioBuffer<float>& buffer,
                    double sampleRate,
                    bool highShelf,
                    float cornerHz,
                    float gainDb,
                    float q = 0.707f)
{
    if (std::abs(gainDb) < 0.05f || buffer.getNumSamples() == 0)
        return;

    ShelfDuplicator shelf;
    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = static_cast<juce::uint32>(buffer.getNumSamples());
    spec.numChannels = static_cast<juce::uint32>(buffer.getNumChannels());

    auto coeffs = highShelf ? juce::dsp::IIR::Coefficients<float>::makeHighShelf(sampleRate, cornerHz, q,
                                                                                 juce::Decibels::decibelsToGain(gainDb))
                            : juce::dsp::IIR::Coefficients<float>::makeLowShelf(sampleRate, cornerHz, q,
                                                                                 juce::Decibels::decibelsToGain(gainDb));
    *shelf.state = *coeffs;
    shelf.prepare(spec);

    juce::dsp::AudioBlock<float> block(buffer);
    juce::dsp::ProcessContextReplacing<float> ctx(block);
    shelf.process(ctx);
}

void applySideChannelFilter(juce::AudioBuffer<float>& buffer,
                            float cutoffHz,
                            FilterType type,
                            double sampleRate)
{
    if (buffer.getNumChannels() < 2)
        return;

    juce::AudioBuffer<float> sideOnly(1, buffer.getNumSamples());
    sideOnly.copyFrom(0, 0, buffer, 1, 0, buffer.getNumSamples());

    BiquadFilter filter(cutoffHz, 0.707f, type);
    filter.prepare(sampleRate, buffer.getNumSamples(), 1);
    filter.process(sideOnly);

    buffer.copyFrom(1, 0, sideOnly, 0, 0, buffer.getNumSamples());
}

void applySideLfMono(juce::AudioBuffer<float>& buffer, float sideLowCutHz, double sampleRate)
{
    if (buffer.getNumChannels() < 2 || sideLowCutHz <= 0.0f)
        return;

    MidSideProcessor ms;
    ms.process(buffer, [&](juce::AudioBuffer<float>& midSide) {
        applySideChannelFilter(midSide, sideLowCutHz, FilterType::HighPass, sampleRate);
    });
}

void applySideHfCut(juce::AudioBuffer<float>& buffer, float sideHighCutHz, double sampleRate)
{
    if (buffer.getNumChannels() < 2 || sideHighCutHz >= 19000.0f)
        return;

    MidSideProcessor ms;
    ms.process(buffer, [&](juce::AudioBuffer<float>& midSide) {
        applySideChannelFilter(midSide, sideHighCutHz, FilterType::LowPass, sampleRate);
    });
}

void applyMaskingAwareHfTamer(juce::AudioBuffer<float>& buffer,
                              const CassetteProfile& profile,
                              const AudioFeatures& features,
                              double sampleRate,
                              const MasteringOptions& options)
{
    const float intensity = juce::jlimit(0.0f, 1.0f, options.hfTamerIntensity);
    if (intensity < 0.01f)
        return;

    const auto& psycho = features.psycho;
    const float effectiveStrength = psycho.hfTamerStrength * intensity;
    if (effectiveStrength < 0.08f)
        return;

    const float ratio = juce::jmap(effectiveStrength,
                                   0.0f,
                                   1.0f,
                                   1.0f,
                                   juce::jmin(profile.hfTamerMaxRatio, profile.hfTamerRatio * 1.6f));
    const float thresholdDb = features.rmsLevelDb + profile.hfTamerThresholdOffsetDb
                            + juce::jmap(psycho.hfAboveMaskingDb, 0.0f, 12.0f, 0.0f, -4.0f);

    DynamicEQ hfTamer(4500.0f, 14000.0f, thresholdDb, ratio);
    hfTamer.prepare(sampleRate, buffer.getNumSamples());
    hfTamer.process(buffer);

    const float shelfCutDb = -intensity * juce::jmin(3.5f,
        effectiveStrength * 2.8f + psycho.sharpnessAcum * 0.35f + psycho.roughnessAsper * 0.9f);
    if (shelfCutDb < -0.1f)
    {
        const float cornerHz = juce::jlimit(5500.0f, 12000.0f, profile.sideHighCutHz * 0.65f);
        applyShelfGain(buffer, sampleRate, true, cornerHz, shelfCutDb, 0.55f);
    }
}

bool needsHfMitigation(const AudioFeatures& features)
{
    const auto& p = features.psycho;
    return p.hfTamerStrength > 0.25f
           || p.sharpnessAcum > 1.55f
           || p.roughnessAsper > 0.18f
           || p.streamingRingingIndex > 0.45f
           || p.hfAboveMaskingDb > 4.0f
           || features.hfEnergyRatio > 2.0f;
}

void applyTapePrintCompensation(juce::AudioBuffer<float>& buffer,
                                const CassetteProfile& profile,
                                const AudioFeatures& features,
                                double sampleRate)
{
    const auto& psycho = features.psycho;

    switch (profile.formulation)
    {
        case TapeFormulation::TypeI:
        {
            const float shelfDb = profile.recordingDeck == RecordingDeck::KenwoodKX1100G ? 1.5f : 2.0f;
            applyShelfGain(buffer, sampleRate, true, 7500.0f, shelfDb, 0.65f);
            break;
        }
        case TapeFormulation::TypeII:
            applyShelfGain(buffer, sampleRate, false, 110.0f, 1.2f, 0.7f);
            if (needsHfMitigation(features))
            {
                const float ratio = profile.recordingDeck == RecordingDeck::KenwoodKX1100G ? 1.18f : 1.35f;
                DynamicEQ hfDeEss(5500.0f, 12000.0f, features.rmsLevelDb - 8.0f, ratio);
                hfDeEss.prepare(sampleRate, buffer.getNumSamples());
                hfDeEss.process(buffer);
            }
            break;
        case TapeFormulation::TypeIV:
            if (features.lfEnergyRatio < 0.12f || psycho.sharpnessAcum > 1.8f)
            {
                const float lfBoost = features.lfEnergyRatio < 0.12f ? 0.8f : 0.0f;
                if (lfBoost > 0.05f)
                    applyShelfGain(buffer, sampleRate, false, 90.0f, lfBoost, 0.7f);
            }
            break;
        default:
            break;
    }
}

float computeGainDb(const CassetteProfile& profile,
                    const AudioFeatures& features,
                    const MasteringOptions& options)
{
    const float sourceLUFS = features.integratedLUFS;
    const float capLUFS = profile.maxIntegratedLUFS;
    const float lufsGain = CassetteAutoMaster::calculateGainForTargetLUFS(sourceLUFS, capLUFS);
    const float peakGain = profile.truePeakCeilingDbfs - features.truePeakDbfs;

    if (options.maximumDigital)
    {
        const bool hotLufs = sourceLUFS > capLUFS + 0.25f;
        const bool hotPeak = features.truePeakDbfs > profile.truePeakCeilingDbfs + 0.05f;

        if (hotLufs && hotPeak)
            return juce::jmax(lufsGain, peakGain);

        if (hotLufs)
            return lufsGain;

        if (hotPeak)
            return peakGain;

        return 0.0f;
    }

    const bool hotLufs = sourceLUFS > capLUFS + 0.25f;
    const bool hotPeak = features.truePeakDbfs > profile.truePeakCeilingDbfs + 0.05f;

    if (hotLufs && hotPeak)
        return juce::jmax(lufsGain, peakGain);

    if (hotLufs)
        return lufsGain;

    if (hotPeak)
        return peakGain;

    return 0.0f;
}

void applyPostChainLoudnessTrim(juce::AudioBuffer<float>& buffer,
                              const CassetteProfile& profile,
                              const AudioFeatures& sourceFeatures,
                              const MasteringOptions& options,
                              double sampleRate)
{
    if (buffer.getNumSamples() == 0)
        return;

    const float target = profile.maxIntegratedLUFS;

    const bool sourceNeededLevelTrim = sourceFeatures.integratedLUFS > target + 0.25f
                                       || sourceFeatures.truePeakDbfs > profile.truePeakCeilingDbfs + 0.05f;

    TruePeakLimiter tpl;
    tpl.setThresholdDbfs(profile.truePeakCeilingDbfs);
    tpl.prepare(sampleRate);

    for (int pass = 0; pass < 4; ++pass)
    {
        const float outLufs = EssentiaAnalyzer::extractFeaturesForMastering(buffer, sampleRate).integratedLUFS;
        const float error = target - outLufs;

        if (std::abs(error) <= kLoudnessToleranceLu)
            break;

        float makeupDb = 0.0f;
        if (error > 0.0f)
        {
            if (!sourceNeededLevelTrim)
                break;

            makeupDb = juce::jmin(kMaxPostMakeupDb, error);
        }
        else
        {
            makeupDb = juce::jmax(-kMaxPostCutDb, error);
        }

        if (std::abs(makeupDb) <= 0.05f)
            break;

        buffer.applyGain(juce::Decibels::decibelsToGain(makeupDb));

        if (makeupDb > 0.05f && !options.maximumDigital)
        {
            const float tp = TruePeakMeter::measurePeakDbfs(buffer, sampleRate);
            if (tp > profile.truePeakCeilingDbfs + 0.03f)
            {
                const float beforePassLufs = outLufs;
                tpl.process(buffer);
                const float afterLimitLufs =
                    EssentiaAnalyzer::extractFeaturesForMastering(buffer, sampleRate).integratedLUFS;
                if (std::abs(afterLimitLufs - beforePassLufs) < 0.08f)
                    break;
            }
        }
    }

    const float finalLufs = EssentiaAnalyzer::extractFeaturesForMastering(buffer, sampleRate).integratedLUFS;
    const float overshoot = finalLufs - target;
    if (overshoot > kLoudnessToleranceLu)
    {
        const float cutDb = juce::jmax(-kMaxPostCutDb, -overshoot);
        if (std::abs(cutDb) > 0.05f)
            buffer.applyGain(juce::Decibels::decibelsToGain(cutDb));
    }
}

}

void CassetteAutoMaster::prepare(double sr, int maxBlockSize)
{
    sampleRate = sr;
    maxBlock = maxBlockSize;
    juce::ignoreUnused(maxBlock);
}

float CassetteAutoMaster::calculateGainForTargetLUFS(float currentLUFS, float targetLUFS)
{
    return targetLUFS - currentLUFS;
}

void CassetteAutoMaster::applyGain(juce::AudioBuffer<float>& buffer, float gainDb)
{
    buffer.applyGain(juce::Decibels::decibelsToGain(gainDb));
}

void CassetteAutoMaster::processTrack(juce::AudioBuffer<float>& buffer,
                                      const CassetteProfile& profile,
                                      const AudioFeatures& features,
                                      const MasteringOptions& options)
{
    const auto gainDb = computeGainDb(profile, features, options);
    if (std::abs(gainDb) > 0.05f)
        applyGain(buffer, gainDb);

    if (!options.maximumDigital)
    {
        BiquadFilter hpf(profile.hpfCutoffHz, kInfraHpfBesselQ, FilterType::HighPass);
        hpf.prepare(sampleRate, buffer.getNumSamples(), buffer.getNumChannels());
        hpf.process(buffer);
    }

    if (!options.maximumDigital)
    {
        applySideLfMono(buffer, profile.sideLowCutHz, sampleRate);
        if (options.hfTamerIntensity > 0.15f)
            applySideHfCut(buffer, profile.sideHighCutHz, sampleRate);
        applyMaskingAwareHfTamer(buffer, profile, features, sampleRate, options);

        RoughnessDeEsser roughnessDeEsser;
        roughnessDeEsser.process(buffer, features.psycho, sampleRate, options.hfTamerIntensity);

        applyTapePrintCompensation(buffer, profile, features, sampleRate);

        if (profile.formulation == TapeFormulation::TypeI)
        {
            TapeAwareSoftClipper softSat;
            auto satProfile = profile;
            satProfile.applyTapeSaturation = true;
            satProfile.saturationDrive = profile.saturationDrive;
            softSat.prepare(sampleRate, 4);
            softSat.setProfile(satProfile);
            softSat.process(buffer);
        }
    }

    if (options.reducePerceivedPitch && buffer.getNumChannels() >= 2)
    {
        const float pct = juce::jlimit(0.0f, 10.0f, options.perceivedPitchReductionPercent);
        const float t = pct / 10.0f;

        const float lpfCutoffHz = juce::jmap(t, 0.0f, 1.0f, 14500.0f, 7000.0f);
        BiquadFilter pitchMaskLpf(lpfCutoffHz, 0.707f, FilterType::LowPass);
        pitchMaskLpf.prepare(sampleRate, buffer.getNumSamples(), buffer.getNumChannels());
        pitchMaskLpf.process(buffer);

        MidSideProcessor ms;
        ms.process(buffer, [t](juce::AudioBuffer<float>& midSide) {
            if (midSide.getNumChannels() < 2)
                return;

            auto* side = midSide.getWritePointer(1);
            const int n = midSide.getNumSamples();
            const float sideGain = juce::jmap(t, 0.0f, 1.0f, 1.0f, 0.72f);
            for (int i = 0; i < n; ++i)
                side[i] *= sideGain;
        });
    }

    if (profile.gentleTransientShaping)
    {
        TransientShaper shaper;
        shaper.setAttackDb(0.5f);
        shaper.setSustainDb(-0.5f);
        shaper.prepare(sampleRate);
        shaper.process(buffer);
    }

    if (profile.applyTapeSaturation)
    {
        TapeAwareSoftClipper onnxModel;
        onnxModel.prepare(sampleRate, 4);
        onnxModel.setProfile(profile);
        onnxModel.process(buffer);
    }

    if (!options.maximumDigital && features.noiseFloorDbfs > -50.0f)
    {
        MaskingNoiseGate noiseGate;
        noiseGate.process(buffer, sampleRate, features, options.hfTamerIntensity);
    }

    TruePeakLimiter tpl;
    tpl.setThresholdDbfs(profile.truePeakCeilingDbfs);
    tpl.prepare(sampleRate);

    if (!options.maximumDigital)
        tpl.process(buffer);

    if (profile.addMicroWowMasking)
    {
        RubberBandWowProcessor wowProcessor;
        wowProcessor.prepare(sampleRate, buffer.getNumSamples(), buffer.getNumChannels());
        wowProcessor.setProfile(profile);
        wowProcessor.process(buffer);
    }

    if (profile.addTapeHissDither)
    {
        std::mt19937 rng(42);
        std::normal_distribution<float> noise(0.0f, 1.0f);
        const auto hissGain = juce::Decibels::decibelsToGain(profile.tapeHissLevelDbfs);

        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        {
            auto* data = buffer.getWritePointer(ch);
            for (int i = 0; i < buffer.getNumSamples(); ++i)
                data[i] += noise(rng) * hissGain * 0.25f;
        }
    }

    applyPostChainLoudnessTrim(buffer, profile, features, options, sampleRate);

    if (options.maximumDigital)
    {
        const float tp = TruePeakMeter::measurePeakDbfs(buffer, sampleRate);
        if (tp > profile.truePeakCeilingDbfs + 0.03f)
            tpl.process(buffer);
    }
}

}
