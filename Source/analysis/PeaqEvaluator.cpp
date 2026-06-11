#include "PeaqEvaluator.h"
#include "PsychoacousticMetrics.h"
#include "../export/WavExporter.h"
#include <cmath>
#include <cstdlib>

namespace cassette
{

namespace
{

float peaqBasicOdgFromMovs(float bandwidthLossPct,
                           float nmrDb,
                           float modDiffDb,
                           float sharpnessDelta,
                           float roughnessDelta)
{
    float odg = 0.0f;
    odg -= juce::jlimit(0.0f, 2.2f, bandwidthLossPct / 12.0f);
    odg -= juce::jlimit(0.0f, 1.6f, std::abs(nmrDb) / 8.0f);
    odg -= juce::jlimit(0.0f, 1.4f, modDiffDb / 6.0f);
    odg -= juce::jlimit(0.0f, 1.0f, std::abs(sharpnessDelta) * 0.65f);
    odg -= juce::jlimit(0.0f, 1.2f, std::abs(roughnessDelta) * 1.4f);
    return juce::jlimit(-4.0f, 0.0f, odg);
}

PeaqEvaluation evaluateBasic(const juce::AudioBuffer<float>& reference,
                             const juce::AudioBuffer<float>& processed,
                             double sampleRate)
{
    PeaqEvaluation result;
    result.backend = PeaqBackend::Basic;

    const auto refM = PsychoacousticMetrics::analyze(reference, sampleRate);
    const auto testM = PsychoacousticMetrics::analyze(processed, sampleRate);

    double refHf = 0.0;
    double testHf = 0.0;
    for (int i = 17; i < PsychoacousticMetrics::kBarkBands; ++i)
    {
        refHf += refM.barkSpecificLoudness[static_cast<size_t>(i)];
        testHf += testM.barkSpecificLoudness[static_cast<size_t>(i)];
    }

    float bandwidthLossPct = 0.0f;
    if (refHf > 1.0e-6)
        bandwidthLossPct = juce::jmax(0.0f, static_cast<float>((refHf - testHf) / refHf * 100.0));

    const float nmrDb = testM.hfAboveMaskingDb - refM.hfAboveMaskingDb;
    const float modDiffDb = std::abs(testM.roughnessAsper - refM.roughnessAsper) * 6.0f
                          + std::abs(testM.fluctuationVacil - refM.fluctuationVacil) * 4.0f;

    result.objectiveDifferenceGrade = peaqBasicOdgFromMovs(bandwidthLossPct,
                                                            nmrDb,
                                                            modDiffDb,
                                                            testM.sharpnessAcum - refM.sharpnessAcum,
                                                            testM.roughnessAsper - refM.roughnessAsper);
    result.success = true;
    result.detail = "PEAQ Basic (BS.1387-inspired MOVs)";
    return result;
}

#if defined(CASSETTE_HAS_GSTPEAQ)
bool runGstPeaq(const juce::File& refWav, const juce::File& testWav, float& odgOut, juce::String& detail)
{
    juce::Array<juce::File> candidates;

    const auto appFile = juce::File::getSpecialLocation(juce::File::currentApplicationFile);
    candidates.add(appFile.getChildFile("Contents/Resources/scripts/run_peaq.sh"));
    candidates.add(appFile.getParentDirectory().getParentDirectory().getChildFile("Resources/scripts/run_peaq.sh"));
    candidates.add(juce::File(juce::File::getCurrentWorkingDirectory()).getChildFile("scripts/run_peaq.sh"));

    if (const char* envScript = std::getenv("CASSETTE_PEAQ_SCRIPT"))
        candidates.add(juce::File(envScript));

    juce::File scriptFile;
    for (const auto& c : candidates)
    {
        if (c.existsAsFile())
        {
            scriptFile = c;
            break;
        }
    }

    if (!scriptFile.existsAsFile())
    {
        detail = "GstPEAQ script not found (install gstreamer or set CASSETTE_PEAQ_SCRIPT)";
        return false;
    }

    juce::StringArray args;
    args.add("/bin/bash");
    args.add(scriptFile.getFullPathName());
    args.add(refWav.getFullPathName());
    args.add(testWav.getFullPathName());

    juce::ChildProcess proc;
    if (!proc.start(args, juce::ChildProcess::wantStdOut | juce::ChildProcess::wantStdErr))
        return false;

    const auto output = proc.readAllProcessOutput();
    proc.waitForProcessToFinish(120000);

    if (proc.getExitCode() != 0)
    {
        detail = output.trim();
        return false;
    }

    const auto lines = juce::StringArray::fromLines(output);
    for (const auto& line : lines)
    {
        if (line.startsWithIgnoreCase("ODG="))
        {
            odgOut = line.fromFirstOccurrenceOf("=", false, false).trim().getFloatValue();
            detail = "GstPEAQ";
            return true;
        }
    }

    detail = output.trim();
    return false;
}

PeaqEvaluation evaluateGStreamer(const juce::AudioBuffer<float>& reference,
                                 const juce::AudioBuffer<float>& processed,
                                 double sampleRate)
{
    PeaqEvaluation result;
    result.backend = PeaqBackend::GStreamer;

    const auto tempDir = juce::File::getSpecialLocation(juce::File::tempDirectory)
                             .getChildFile("CassetteAutoMaster_peaq");
    tempDir.createDirectory();

    const auto refFile = tempDir.getNonexistentChildFile("ref", ".wav");
    const auto testFile = tempDir.getNonexistentChildFile("test", ".wav");

    if (!WavExporter::writeWav32Float(refFile, reference, sampleRate)
        || !WavExporter::writeWav32Float(testFile, processed, sampleRate))
    {
        result.detail = "Failed to write temp WAV for GstPEAQ";
        return result;
    }

    float odg = 0.0f;
    juce::String detail;
    if (runGstPeaq(refFile, testFile, odg, detail))
    {
        result.objectiveDifferenceGrade = juce::jlimit(-4.0f, 0.0f, odg);
        result.success = true;
        result.detail = detail;
        refFile.deleteFile();
        testFile.deleteFile();
        return result;
    }

    refFile.deleteFile();
    testFile.deleteFile();
    result.detail = detail;
    return result;
}
#endif

}

bool PeaqEvaluator::isGStreamerPeaqAvailable()
{
#if defined(CASSETTE_HAS_GSTPEAQ)
    juce::ChildProcess proc;
    juce::StringArray args { "gst-launch-1.0", "--version" };
    if (!proc.start(args, juce::ChildProcess::wantStdOut))
        return false;
    proc.waitForProcessToFinish(5000);
    if (proc.getExitCode() != 0)
        return false;

    juce::ChildProcess inspect;
    juce::StringArray inspectArgs { "gst-inspect-1.0", "peaq" };
    if (!inspect.start(inspectArgs, juce::ChildProcess::wantStdOut))
        return false;
    inspect.waitForProcessToFinish(5000);
    return inspect.getExitCode() == 0;
#else
    return false;
#endif
}

PeaqEvaluation PeaqEvaluator::evaluate(const juce::AudioBuffer<float>& reference,
                                       const juce::AudioBuffer<float>& processed,
                                       double sampleRate)
{
#if defined(CASSETTE_HAS_GSTPEAQ)
    if (isGStreamerPeaqAvailable())
    {
        auto gst = evaluateGStreamer(reference, processed, sampleRate);
        if (gst.success)
            return gst;
    }
#endif

    return evaluateBasic(reference, processed, sampleRate);
}

}
