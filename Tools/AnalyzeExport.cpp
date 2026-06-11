#include <iostream>
#include <iomanip>
#include <cmath>
#include <vector>
#include <juce_audio_basics/juce_audio_basics.h>
#include "../Source/io/AudioFileLoader.h"
#include "../Source/analysis/EssentiaAnalyzer.h"
#include "../Source/analysis/AudioFeatures.h"
#include "../Source/dsp/CassetteProfile.h"
#include "../Source/dsp/graph/AdaptiveMasteringProcessor.h"
#include "../Source/dsp/MasteringOptions.h"

namespace
{

struct Report
{
    cassette::AudioFeatures features;
    double durationSec = 0.0;
    juce::String path;
};

Report analyzeFile(const juce::File& file)
{
    Report r;
    r.path = file.getFullPathName();
    const auto loaded = cassette::AudioFileLoader::loadToBufferWithDiagnostics(file);
    if (!loaded.audio.hasValue())
    {
        std::cerr << "Failed to load: " << file.getFullPathName() << " — " << loaded.error << '\n';
        return r;
    }
    r.durationSec = loaded.audio->buffer.getNumSamples() / loaded.audio->sampleRate;
    r.features = cassette::EssentiaAnalyzer::extractFeatures(loaded.audio->buffer, loaded.audio->sampleRate);
    return r;
}

void printReport(const char* label, const Report& r)
{
    std::cout << label << ": " << juce::File(r.path).getFileName() << '\n'
              << "  duration          " << std::fixed << std::setprecision(1) << (r.durationSec / 60.0) << " min\n"
              << r.features.toDisplayString();
}

int scoreCassetteReadiness(const cassette::AudioFeatures& f, cassette::TapeFormulation tape)
{
    const auto profile = cassette::CassetteProfile::fromFormulation(tape);
    int score = 0;
    const int max = 8;

    if (f.integratedLUFS >= -17.0f && f.integratedLUFS <= -10.0f) ++score;
    if (f.truePeakDbfs <= profile.truePeakCeilingDbfs + 0.5f) ++score;
    if (f.hfEnergyRatio < 0.35f) ++score;
    if (f.lfStereoCorrelation > 0.3f) ++score;
    if (f.crestFactorDb > 6.0f) ++score;
    if (f.clippingPercent < 0.01f) ++score;
    if (f.loudnessRangeDb > 2.0f && f.loudnessRangeDb < 20.0f) ++score;
    if (f.noiseFloorDbfs < -50.0f) ++score;

    std::cout << "Cassette readiness (Type IV): " << score << "/" << max << '\n';
    return score;
}

void printSummaryRow(const Report& r)
{
    const auto& f = r.features;
    std::cout << std::left << std::setw(26) << juce::File(r.path).getFileName().substring(0, 25).toStdString()
              << std::setw(6) << std::fixed << std::setprecision(1) << (r.durationSec / 60.0)
              << std::setw(7) << std::setprecision(1) << f.integratedLUFS
              << std::setw(6) << f.truePeakDbfs
              << std::setw(6) << f.loudnessRangeDb
              << std::setw(6) << (f.lfEnergyRatio * 100.0f)
              << std::setw(6) << (f.hfEnergyRatio * 100.0f)
              << std::setw(6) << f.stereoWidthPercent
              << std::setw(6) << f.stereoCorrelation
              << std::setw(7) << f.noiseFloorDbfs
              << std::setw(6) << f.clippingPercent << '\n';
}

} // namespace

int main(int argc, char* argv[])
{
    if (argc >= 3 && juce::String(argv[1]) == "--master")
    {
        const juce::File input(argv[2]);
        const auto loaded = cassette::AudioFileLoader::loadToBufferWithDiagnostics(input);
        if (!loaded.audio.hasValue())
        {
            std::cerr << "Load failed: " << loaded.error << '\n';
            return 1;
        }

        auto deck = cassette::RecordingDeck::Generic;
        auto tape = cassette::TapeFormulation::TypeI;
        for (int i = 3; i < argc; ++i)
        {
            const juce::String arg(argv[i]);
            if (arg.equalsIgnoreCase("kenwood"))
                deck = cassette::RecordingDeck::KenwoodKX1100G;
            else if (arg.equalsIgnoreCase("typeII") || arg.equalsIgnoreCase("type2"))
                tape = cassette::TapeFormulation::TypeII;
            else if (arg.equalsIgnoreCase("typeIV") || arg.equalsIgnoreCase("type4"))
                tape = cassette::TapeFormulation::TypeIV;
        }

        const auto profile = deck == cassette::RecordingDeck::KenwoodKX1100G
                                 ? cassette::CassetteProfile::forRecording(deck, tape)
                                 : cassette::CassetteProfile::fromFormulation(tape);

        auto buffer = loaded.audio->buffer;
        const double sr = loaded.audio->sampleRate;
        const auto before = cassette::EssentiaAnalyzer::extractFeatures(buffer, sr);
        const auto beforeMastering =
            cassette::EssentiaAnalyzer::extractFeaturesForMastering(buffer, sr);

        cassette::MasteringOptions opts;
        opts.skipQualityCompare = true;
        cassette::AdaptiveMasteringProcessor processor;
        const auto result = processor.process(buffer, profile, opts, sr);
        const auto after = cassette::EssentiaAnalyzer::extractFeatures(buffer, sr);

        std::cout << "=== Master probe: " << input.getFileName() << " ===\n"
                  << "  plan: " << result.autoPlanSummary << '\n'
                  << "  cap:  " << profile.maxIntegratedLUFS << " LUFS, TP "
                  << profile.truePeakCeilingDbfs << " dBFS\n"
                  << "  before: LUFS " << before.integratedLUFS << ", TP " << before.truePeakDbfs << '\n'
                  << "  analyze(master): LUFS " << beforeMastering.integratedLUFS << ", TP "
                  << beforeMastering.truePeakDbfs << '\n'
                  << "  after:  LUFS " << after.integratedLUFS << ", TP " << after.truePeakDbfs << '\n'
                  << "  delta:  LUFS " << (after.integratedLUFS - before.integratedLUFS)
                  << ", TP " << (after.truePeakDbfs - before.truePeakDbfs) << '\n';
        return 0;
    }

    if (argc >= 3 && juce::String(argv[1]) == "--batch")
    {
        std::cout << "=== Batch tape archive analysis ===\n\n";
        std::vector<Report> reports;
        for (int i = 2; i < argc; ++i)
            reports.push_back(analyzeFile(juce::File(argv[i])));

        for (size_t i = 0; i < reports.size(); ++i)
        {
            if (reports[i].durationSec <= 0)
                continue;
            printReport(juce::String("[" + juce::String(static_cast<int>(i + 1)) + "]").toRawUTF8(), reports[i]);
            std::cout << '\n';
        }

        std::cout << "--- Summary ---\n";
        std::cout << std::left << std::setw(26) << "File"
                  << std::setw(6) << "min"
                  << std::setw(7) << "LUFS"
                  << std::setw(6) << "TP"
                  << std::setw(6) << "LRA"
                  << std::setw(6) << "LF%"
                  << std::setw(6) << "HF%"
                  << std::setw(6) << "Width"
                  << std::setw(6) << "Corr"
                  << std::setw(7) << "Noise"
                  << std::setw(6) << "Clip%\n";

        for (const auto& r : reports)
            if (r.durationSec > 0)
                printSummaryRow(r);

        return 0;
    }

    if (argc < 3)
    {
        const char* prog = argc > 0 ? argv[0] : "CassetteAnalyzeExport";
        std::cerr << "Usage:\n"
                  << "  " << prog << " <original> <mastered>\n"
                  << "  " << prog << " --batch <file> [file...]\n"
                  << "  " << prog << " --master <input> [kenwood] [typeII|typeIV]\n";
        return 1;
    }

    const auto a = analyzeFile(juce::File(argv[1]));
    const auto b = analyzeFile(juce::File(argv[2]));
    printReport("A", a);
    printReport("B", b);
    if (a.durationSec > 0 && b.durationSec > 0)
    {
        const auto& af = a.features;
        const auto& bf = b.features;
        std::cout << "--- Delta (B - A) ---\n"
                  << "  LUFS:      " << (bf.integratedLUFS - af.integratedLUFS) << '\n'
                  << "  TP:        " << (bf.truePeakDbfs - af.truePeakDbfs) << " dB\n"
                  << "  LRA:       " << (bf.loudnessRangeDb - af.loudnessRangeDb) << " LU\n"
                  << "  HF%:       " << ((bf.hfEnergyRatio - af.hfEnergyRatio) * 100.0f) << '\n'
                  << "  Width%:    " << (bf.stereoWidthPercent - af.stereoWidthPercent) << '\n'
                  << "  Clip%:     " << (bf.clippingPercent - af.clippingPercent) << '\n';
    }
    scoreCassetteReadiness(b.features, cassette::TapeFormulation::TypeIV);
    return 0;
}
