#include "CassetteMasteringPlanner.h"

namespace cassette
{

namespace
{

float computeTapeThreatScore(const AudioFeatures& f, const CassetteProfile& profile)
{
    float threat = 0.0f;

    const float lufsOver = f.integratedLUFS - profile.maxIntegratedLUFS;
    if (lufsOver > 0.25f)
        threat += juce::jmin(0.30f, lufsOver * 0.07f);

    const float tpOver = f.truePeakDbfs - profile.truePeakCeilingDbfs;
    if (tpOver > 0.0f)
        threat += juce::jmin(0.18f, tpOver * 0.12f);

    threat += f.psycho.hfTamerStrength * 0.32f;

    if (f.psycho.roughnessAsper > 0.12f)
        threat += juce::jmin(0.22f, (f.psycho.roughnessAsper - 0.12f) * 0.55f);

    if (f.psycho.sharpnessAcum > 1.3f)
        threat += juce::jmin(0.14f, (f.psycho.sharpnessAcum - 1.3f) * 0.08f);

    if (f.psycho.hfAboveMaskingDb > 3.0f)
        threat += juce::jmin(0.20f, (f.psycho.hfAboveMaskingDb - 3.0f) * 0.025f);

    if (f.psycho.streamingRingingIndex > 0.25f)
        threat += juce::jmin(0.12f, (f.psycho.streamingRingingIndex - 0.25f) * 0.35f);

    if (f.lfStereoCorrelation < 0.35f)
        threat += juce::jmin(0.14f, (0.35f - f.lfStereoCorrelation) * 0.35f);

    if (f.hfEnergyRatio > 1.8f)
        threat += juce::jmin(0.14f, (f.hfEnergyRatio - 1.8f) * 0.07f);

    if (f.clippingPercent > 0.005f)
        threat += juce::jmin(0.18f, f.clippingPercent * 4.0f);

    const float relief = juce::jlimit(0.0f, 0.35f, profile.autoPrepRelief);
    threat *= 1.0f - relief;

    return juce::jlimit(0.0f, 1.0f, threat);
}

bool isDeckFriendly(const AudioFeatures& f, const CassetteProfile& profile, float threat)
{
    const float relief = juce::jlimit(0.0f, 0.35f, profile.autoPrepRelief);
    const float threatGate = 0.20f + relief * 0.08f;
    const float hfGate = 0.14f + relief * 0.05f;
    const float roughGate = 0.14f + relief * 0.04f;

    if (threat >= threatGate)
        return false;

    const float lufsOver = f.integratedLUFS - profile.maxIntegratedLUFS;
    if (lufsOver > 0.75f + relief * 0.35f)
        return false;

    if (f.truePeakDbfs > profile.truePeakCeilingDbfs + 0.25f + relief * 0.15f)
        return false;

    if (f.psycho.hfTamerStrength > hfGate)
        return false;

    if (f.psycho.roughnessAsper > roughGate)
        return false;

    if (f.lfStereoCorrelation < 0.45f - relief * 0.08f)
        return false;

    if (f.clippingPercent > 0.003f)
        return false;

    return true;
}

bool isHotLevelCleanSpectrum(const AudioFeatures& f, const CassetteProfile& profile)
{
    if (f.clippingPercent > 0.003f)
        return false;

    if (f.psycho.roughnessAsper >= 0.26f)
        return false;

    if (f.psycho.sharpnessAcum >= 1.45f)
        return false;

    if (f.psycho.streamingRingingIndex >= 0.38f)
        return false;

    if (f.psycho.hfTamerStrength > 0.22f)
        return false;

    if (f.psycho.hfAboveMaskingDb > 4.5f)
        return false;

    if (f.lfStereoCorrelation < 0.45f)
        return false;

    if (f.hfEnergyRatio > 2.2f)
        return false;

    const bool hotLevel = f.integratedLUFS > profile.maxIntegratedLUFS + 0.15f;
    const bool hotPeak = f.truePeakDbfs > profile.truePeakCeilingDbfs + 0.05f;
    return hotLevel || hotPeak;
}

float computeHfTamerIntensity(const AudioFeatures& f, float threat, float autoPrepRelief)
{
    const float psycho = f.psycho.hfTamerStrength;
    const float rough = juce::jmap(f.psycho.roughnessAsper, 0.12f, 0.40f, 0.0f, 0.35f);
    const float sharp = juce::jmap(f.psycho.sharpnessAcum, 1.2f, 2.2f, 0.0f, 0.20f);
    const float mask = juce::jmap(f.psycho.hfAboveMaskingDb, 2.0f, 10.0f, 0.0f, 0.25f);

    const float relief = juce::jlimit(0.0f, 0.35f, autoPrepRelief);
    const float blend = psycho * 0.55f + rough + sharp + mask + threat * 0.25f;
    const float scaled = blend * (1.0f - relief * 0.30f);
    return juce::jlimit(0.18f, 1.0f, scaled);
}

}

CassetteMasteringPlan CassetteMasteringPlanner::plan(const AudioFeatures& features,
                                                     const CassetteProfile& profile)
{
    CassetteMasteringPlan plan;
    plan.tapeThreatScore = computeTapeThreatScore(features, profile);
    plan.options.perceptualAutoFallback = true;

    if (isHotLevelCleanSpectrum(features, profile))
    {
        plan.options.maximumDigital = true;
        plan.options.hfTamerIntensity = 0.0f;
        plan.summary = profile.recordingDeck == RecordingDeck::KenwoodKX1100G
                           ? "Auto: KX-1100G hot/clean (level + limiter)"
                           : "Auto: hot level, clean spectrum (level + limiter)";
        return plan;
    }

    if (isDeckFriendly(features, profile, plan.tapeThreatScore))
    {
        plan.options.maximumDigital = true;
        plan.options.hfTamerIntensity = 0.0f;
        plan.summary = profile.recordingDeck == RecordingDeck::KenwoodKX1100G
                           ? "Auto: KX-1100G deck-friendly transfer"
                           : "Auto: deck-friendly transfer (level + limiter only)";
        return plan;
    }

    plan.options.maximumDigital = false;
    plan.options.hfTamerIntensity = computeHfTamerIntensity(features,
                                                            plan.tapeThreatScore,
                                                            profile.autoPrepRelief);

    juce::StringArray reasons;
    if (features.integratedLUFS > profile.maxIntegratedLUFS + 0.5f)
        reasons.add("hot level");
    if (features.psycho.roughnessAsper > 0.16f)
        reasons.add("rough HF");
    if (features.psycho.hfAboveMaskingDb > 4.0f)
        reasons.add("bright top");
    if (features.lfStereoCorrelation < 0.35f)
        reasons.add("wide bass");
    if (features.clippingPercent > 0.01f)
        reasons.add("clipping");

    plan.summary = "Auto: tape prep "
                   + juce::String(static_cast<int>(plan.options.hfTamerIntensity * 100.0f + 0.5f)) + "%";
    if (profile.recordingDeck == RecordingDeck::KenwoodKX1100G)
        plan.summary = "Auto: KX-1100G prep "
                       + juce::String(static_cast<int>(plan.options.hfTamerIntensity * 100.0f + 0.5f)) + "%";
    if (!reasons.isEmpty())
        plan.summary << " (" << reasons.joinIntoString(", ") << ")";

    return plan;
}

}
