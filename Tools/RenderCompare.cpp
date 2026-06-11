#include <iostream>
#include <iomanip>
#include <cmath>
#include <juce_audio_basics/juce_audio_basics.h>
#include "../Source/io/AudioFileLoader.h"
#include "../Source/analysis/EssentiaAnalyzer.h"
#include "../Source/dsp/graph/MasteringGraph.h"
#include "../Source/dsp/CassetteProfile.h"
#include "../Source/dsp/MasteringOptions.h"

namespace
{

struct Metrics
{
    cassette::AudioFeatures f;
    double durationSec = 0.0;
};

Metrics analyzeBuffer(const juce::AudioBuffer<float>& buffer, double sr)
{
    Metrics m;
    m.durationSec = buffer.getNumSamples() / sr;
    m.f = cassette::EssentiaAnalyzer::extractFeatures(buffer, sr);
    return m;
}

float fidelityPercent(const cassette::AudioFeatures& src, const cassette::AudioFeatures& proc)
{
    const float lufsErr = std::abs(proc.integratedLUFS - src.integratedLUFS) / 2.0f;
    const float tpErr = std::abs(proc.truePeakDbfs - src.truePeakDbfs) / 2.0f;
    const float lraErr = std::abs(proc.loudnessRangeDb - src.loudnessRangeDb) / 4.0f;
    const float hfErr = std::abs(proc.hfEnergyRatio - src.hfEnergyRatio) / 0.05f;
    const float widthErr = std::abs(proc.stereoWidthPercent - src.stereoWidthPercent) / 8.0f;
    const float lfErr = std::abs(proc.lfEnergyRatio - src.lfEnergyRatio) / 0.12f;

    const float meanErr = (lufsErr + tpErr + lraErr + hfErr + widthErr + lfErr) / 6.0f;
    return juce::jlimit(0.0f, 100.0f, 100.0f * (1.0f - meanErr));
}

} // namespace

int main(int argc, char* argv[])
{
    if (argc < 2)
    {
        const char* prog = argc > 0 ? argv[0] : "CassetteRenderCompare";
        std::cerr << "Usage: " << prog << " <source> [reference-export]\n";
        return 1;
    }

    const juce::File sourceFile(argv[1]);
    const juce::File referenceFile = argc >= 3 ? juce::File(argv[2]) : juce::File();

    const auto loaded = cassette::AudioFileLoader::loadToBufferWithDiagnostics(sourceFile);
    if (!loaded.audio.hasValue())
    {
        std::cerr << "Load failed: " << loaded.error << '\n';
        return 1;
    }

    const auto srcMetrics = analyzeBuffer(loaded.audio->buffer, loaded.audio->sampleRate);
    const auto profile = cassette::CassetteProfile::fromFormulation(cassette::TapeFormulation::TypeIV);

    juce::AudioBuffer<float> normal = loaded.audio->buffer;
    juce::AudioBuffer<float> maxDigital = loaded.audio->buffer;

    cassette::MasteringGraph graph;
    graph.prepare(loaded.audio->sampleRate, normal.getNumSamples());

    const auto featuresIn = cassette::EssentiaAnalyzer::extractFeatures(normal, loaded.audio->sampleRate);
    graph.process(normal, profile, featuresIn, {});

    const auto featuresIn2 = cassette::EssentiaAnalyzer::extractFeatures(maxDigital, loaded.audio->sampleRate);
    cassette::MasteringOptions maxOpts;
    maxOpts.maximumDigital = true;
    graph.process(maxDigital, profile, featuresIn2, maxOpts);

    const auto normalMetrics = analyzeBuffer(normal, loaded.audio->sampleRate);
    const auto maxMetrics = analyzeBuffer(maxDigital, loaded.audio->sampleRate);

    Metrics referenceMetrics;
    if (referenceFile.existsAsFile())
    {
        const auto refLoaded = cassette::AudioFileLoader::loadToBufferWithDiagnostics(referenceFile);
        if (refLoaded.audio.hasValue())
            referenceMetrics = analyzeBuffer(refLoaded.audio->buffer, refLoaded.audio->sampleRate);
    }

    const float referenceFidelity = referenceMetrics.durationSec > 0
                                        ? fidelityPercent(srcMetrics.f, referenceMetrics.f)
                                        : 0.0f;
    const float newFidelity = fidelityPercent(srcMetrics.f, normalMetrics.f);
    const float maxFidelity = fidelityPercent(srcMetrics.f, maxMetrics.f);

    std::cout << "=== Render compare (Type IV) ===\n\n";
    std::cout << std::setw(22) << "Variant"
              << " LUFS    TP     LRA   HF%   W%    score\n";
    std::cout << std::setw(22) << "Source"
              << " " << srcMetrics.f.integratedLUFS
              << " " << srcMetrics.f.truePeakDbfs
              << " " << srcMetrics.f.loudnessRangeDb
              << " " << (srcMetrics.f.hfEnergyRatio * 100.0f)
              << " " << srcMetrics.f.stereoWidthPercent << "  100%\n";

    if (referenceMetrics.durationSec > 0)
        std::cout << std::setw(22) << "Reference export"
                  << " " << referenceMetrics.f.integratedLUFS
                  << " " << referenceMetrics.f.truePeakDbfs
                  << " " << referenceMetrics.f.loudnessRangeDb
                  << " " << (referenceMetrics.f.hfEnergyRatio * 100.0f)
                  << " " << referenceMetrics.f.stereoWidthPercent
                  << "  " << std::setprecision(1) << referenceFidelity << "%\n";

    std::cout << std::setw(22) << "Standard chain"
              << " " << normalMetrics.f.integratedLUFS
              << " " << normalMetrics.f.truePeakDbfs
              << " " << normalMetrics.f.loudnessRangeDb
              << " " << (normalMetrics.f.hfEnergyRatio * 100.0f)
              << " " << normalMetrics.f.stereoWidthPercent
              << "  " << std::setprecision(1) << newFidelity << "%\n";

    std::cout << std::setw(22) << "Maximum Digital"
              << " " << maxMetrics.f.integratedLUFS
              << " " << maxMetrics.f.truePeakDbfs
              << " " << maxMetrics.f.loudnessRangeDb
              << " " << (maxMetrics.f.hfEnergyRatio * 100.0f)
              << " " << maxMetrics.f.stereoWidthPercent
              << "  " << maxFidelity << "%\n";

    if (referenceFidelity > 0.0f)
    {
        const float gainVsRef = ((newFidelity - referenceFidelity) / referenceFidelity) * 100.0f;
        const float gainMaxVsRef = ((maxFidelity - referenceFidelity) / referenceFidelity) * 100.0f;
        std::cout << "\nVs reference export:\n"
                  << "  Standard:        " << std::showpos << std::setprecision(0) << gainVsRef
                  << "% score (" << referenceFidelity << "% -> " << newFidelity << "%)\n"
                  << "  Maximum Digital: " << gainMaxVsRef
                  << "% (" << referenceFidelity << "% -> " << maxFidelity << "%)\n"
                  << std::noshowpos;
    }

    return 0;
}
