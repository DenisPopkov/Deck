#include "PerceptualQualityGuard.h"
#include "PeaqEvaluator.h"

namespace cassette
{

namespace
{

float highBandEnergy(const PsychoacousticMetrics& m)
{
    double sum = 0.0;
    for (int i = 17; i < PsychoacousticMetrics::kBarkBands; ++i)
        sum += m.barkSpecificLoudness[static_cast<size_t>(i)];
    return static_cast<float>(sum);
}

void fillHeuristicMovs(PerceptualQualityReport& report,
                       const PsychoacousticMetrics& refMetrics,
                       const PsychoacousticMetrics& testMetrics)
{
    report.sharpnessDeltaAcum = testMetrics.sharpnessAcum - refMetrics.sharpnessAcum;
    report.roughnessDeltaAsper = testMetrics.roughnessAsper - refMetrics.roughnessAsper;
    report.fluctuationDeltaVacil = testMetrics.fluctuationVacil - refMetrics.fluctuationVacil;

    const auto refHf = highBandEnergy(refMetrics);
    const auto testHf = highBandEnergy(testMetrics);
    if (refHf > 1.0e-4f)
    {
        const auto loss = juce::jmax(0.0f, (refHf - testHf) / refHf);
        report.bandwidthLossPercent = loss * 100.0f;
    }

    const auto refMask = juce::jmax(1.0f, refMetrics.hfAboveMaskingDb);
    report.noiseToMaskRatioDb = testMetrics.hfAboveMaskingDb - refMask;
    report.modDiffDb = std::abs(report.roughnessDeltaAsper) * 6.0f + std::abs(report.fluctuationDeltaVacil) * 4.0f;
}

}

PerceptualQualityReport PerceptualQualityReport::compare(const juce::AudioBuffer<float>& reference,
                                                         const juce::AudioBuffer<float>& processed,
                                                         double sampleRate)
{
    PerceptualQualityReport report;

    const auto refMetrics = PsychoacousticMetrics::analyze(reference, sampleRate);
    const auto testMetrics = PsychoacousticMetrics::analyze(processed, sampleRate);
    fillHeuristicMovs(report, refMetrics, testMetrics);

    const auto peaq = PeaqEvaluator::evaluate(reference, processed, sampleRate);
    report.peaqBackend = peaq.backend;
    report.peaqDetail = peaq.detail;

    if (peaq.success)
    {
        report.objectiveDifferenceGrade = peaq.objectiveDifferenceGrade;
        report.passesQualityGate = report.objectiveDifferenceGrade >= -1.2f;
        return report;
    }

    float odg = 0.0f;
    odg -= juce::jlimit(0.0f, 1.8f, report.bandwidthLossPercent / 14.0f);
    odg -= juce::jlimit(0.0f, 1.4f, std::abs(report.sharpnessDeltaAcum) * 0.75f);
    odg -= juce::jlimit(0.0f, 1.2f, std::abs(report.roughnessDeltaAsper) * 1.6f);
    odg -= juce::jlimit(0.0f, 0.8f, std::abs(report.fluctuationDeltaVacil) * 1.2f);
    odg -= juce::jlimit(0.0f, 0.9f, std::abs(report.noiseToMaskRatioDb) / 11.0f);
    report.objectiveDifferenceGrade = juce::jlimit(-4.0f, 0.0f, odg);
    report.passesQualityGate = report.objectiveDifferenceGrade >= -1.2f;
    report.peaqBackend = PeaqBackend::Heuristic;
    report.peaqDetail = "Heuristic fallback";

    return report;
}

}
