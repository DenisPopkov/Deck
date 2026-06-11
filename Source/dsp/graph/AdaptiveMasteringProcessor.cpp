#include "AdaptiveMasteringProcessor.h"
#include "MasteringGraph.h"
#include "../CassetteMasteringPlanner.h"
#include "../../analysis/EssentiaAnalyzer.h"

namespace cassette
{

namespace
{

void applyAutoPlan(MasteringOptions& options, const CassetteMasteringPlan& plan)
{
    options.maximumDigital = plan.options.maximumDigital;
    options.hfTamerIntensity = plan.options.hfTamerIntensity;
    options.perceptualAutoFallback = true;
}

} // namespace

AdaptiveMasteringResult AdaptiveMasteringProcessor::process(juce::AudioBuffer<float>& buffer,
                                                            const CassetteProfile& profile,
                                                            MasteringOptions options,
                                                            double sampleRate)
{
    AdaptiveMasteringResult result;

    juce::AudioBuffer<float> reference;
    reference.makeCopyOf(buffer);

    const auto features = options.skipQualityCompare
                              ? EssentiaAnalyzer::extractFeaturesForMastering(buffer, sampleRate)
                              : EssentiaAnalyzer::extractFeatures(buffer, sampleRate);

    const auto plan = CassetteMasteringPlanner::plan(features, profile);
    result.autoPlanSummary = plan.summary;

    MasteringOptions current = options;
    applyAutoPlan(current, plan);

    const bool runQualityLoop = !options.skipQualityCompare && !current.maximumDigital;

    if (!runQualityLoop)
    {
        MasteringGraph graph;
        graph.prepare(sampleRate, buffer.getNumSamples());
        graph.process(buffer, profile, features, current);

        if (!options.skipQualityCompare)
        {
            result.featuresAfter = EssentiaAnalyzer::extractFeatures(buffer, sampleRate);
            result.quality = PerceptualQualityReport::compare(reference, buffer, sampleRate);
        }

        result.quality.fallbackIterations = 0;
        result.quality.hfTamerIntensityUsed = current.hfTamerIntensity;
        result.optionsUsed = current;
        result.tapeThreatScore = plan.tapeThreatScore;
        result.iterations = 1;
        return result;
    }

    for (int iter = 0; iter < 6; ++iter)
    {
        buffer.makeCopyOf(reference);
        const auto iterFeatures = EssentiaAnalyzer::extractFeatures(buffer, sampleRate);

        MasteringGraph graph;
        graph.prepare(sampleRate, buffer.getNumSamples());
        graph.process(buffer, profile, iterFeatures, current);

        result.featuresAfter = EssentiaAnalyzer::extractFeatures(buffer, sampleRate);
        result.quality = PerceptualQualityReport::compare(reference, buffer, sampleRate);
        result.quality.fallbackIterations = iter;
        result.quality.hfTamerIntensityUsed = current.hfTamerIntensity;
        result.optionsUsed = current;
        result.tapeThreatScore = plan.tapeThreatScore;
        result.iterations = iter + 1;

        if (result.quality.passesQualityGate)
            return result;

        if (result.quality.objectiveDifferenceGrade >= -1.2f)
            return result;

        if (result.quality.objectiveDifferenceGrade < -1.8f)
        {
            if (current.hfTamerIntensity > 0.2f)
            {
                current.hfTamerIntensity = juce::jmax(0.0f, current.hfTamerIntensity - 0.25f);
                continue;
            }

            current.maximumDigital = true;
            current.hfTamerIntensity = 0.0f;
            result.quality.usedMinimalFallback = true;
            continue;
        }

        return result;
    }

    return result;
}

} // namespace cassette
