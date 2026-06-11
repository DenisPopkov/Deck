#include "FolderMixBuilder.h"
#include "../io/AudioFileLoader.h"
#include <algorithm>

namespace cassette
{

namespace
{

double readFileDurationSec(const juce::File& file)
{
    auto in = file.createInputStream();
    if (in == nullptr)
        return 0.0;

    auto& fm = AudioFileLoader::getFormatManager();
    std::unique_ptr<juce::AudioFormatReader> reader(
        fm.createReaderFor(std::unique_ptr<juce::InputStream>(in.release())));
    if (reader == nullptr)
        return 0.0;

    return static_cast<double>(reader->lengthInSamples) / reader->sampleRate;
}

}

TapeLengthSpec tapeLengthSpecForPreset(TapeLengthPreset preset, double customMinutesPerSide)
{
    switch (preset)
    {
        case TapeLengthPreset::C60: return { "C60", 30.0 };
        case TapeLengthPreset::C120: return { "C120", 60.0 };
        case TapeLengthPreset::Custom:
            return { "Custom", juce::jmax(1.0, customMinutesPerSide) };
        case TapeLengthPreset::C90:
        default: return { "C90", 45.0 };
    }
}

juce::String FolderFitReport::summary() const
{
    const auto req = FolderMixBuilder::formatDuration(requiredSec);
    const auto lim = FolderMixBuilder::formatDuration(allowedSec);

    if (!fits)
        return juce::String(trackCount) + " tracks - a single track exceeds " + lim + " per side";

    if (cassetteCount > 1)
    {
        juce::String msg = juce::String(trackCount) + " tracks, " + req + " - " + juce::String(cassetteCount)
                           + " cassettes (" + tape.label + ", " + lim + " per side): ";
        for (size_t i = 0; i < cassettes.size(); ++i)
        {
            if (i > 0)
                msg << " | ";
            const auto& c = cassettes[i];
            msg << "Cassette " << c.cassetteNumber << " A:" << c.sideATrackCount() << " ("
                << FolderMixBuilder::formatDuration(c.sideADurationSec) << ")";
            if (c.hasSideB)
                msg << " B:" << c.sideBTrackCount() << " (" << FolderMixBuilder::formatDuration(c.sideBDurationSec)
                    << ")";
        }
        return msg;
    }

    if (!fitsOnCassette)
        return juce::String(trackCount) + " tracks, " + req + " - over cassette (2x" + lim + ") by "
               + FolderMixBuilder::formatDuration(overCassetteSec);

    if (fitsOneSide)
        return juce::String(trackCount) + " tracks, " + req + " / " + lim + " - fits on " + tape.label
               + " Side A";

    const auto aDur = FolderMixBuilder::formatDuration(split.sideADurationSec);
    const auto bDur = FolderMixBuilder::formatDuration(split.sideBDurationSec);
    return juce::String(trackCount) + " tracks, " + req + " - Side A: " + juce::String(sideATrackCount)
           + " (" + aDur + ") | Side B: " + juce::String(sideBTrackCount) + " (" + bDur + ") / " + lim
           + " per side";
}

juce::String FolderMixBuilder::formatDuration(double seconds)
{
    const auto s = juce::jmax(0.0, seconds);
    const int mins = static_cast<int>(s) / 60;
    const int secs = static_cast<int>(s) % 60;
    return juce::String::formatted("%d:%02d", mins, secs);
}

FolderScanResult FolderMixBuilder::scanFolder(const juce::File& folder, double gapBetweenTracksSec)
{
    FolderScanResult result;
    result.folder = folder;
    result.gapBetweenTracksSec = juce::jmax(0.0, gapBetweenTracksSec);

    if (!folder.isDirectory())
    {
        result.error = "Not a folder: " + folder.getFullPathName();
        return result;
    }

    juce::Array<juce::File> files;
    folder.findChildFiles(files, juce::File::findFiles, false);

    for (const auto& f : files)
    {
        if (!AudioFileLoader::isSupportedAudioFile(f))
            continue;

        FolderTrackInfo info;
        info.file = f;
        info.displayName = f.getFileNameWithoutExtension();
        info.durationSec = readFileDurationSec(f);
        if (info.durationSec <= 0.0)
            continue;

        result.tracks.push_back(std::move(info));
    }

    if (result.tracks.empty())
    {
        result.error = "No audio files in folder";
        return result;
    }

    std::sort(result.tracks.begin(), result.tracks.end(), [](const FolderTrackInfo& a, const FolderTrackInfo& b) {
        return a.file.getFileName().compareNatural(b.file.getFileName()) < 0;
    });

    result.totalDurationSec = 0.0;
    for (size_t i = 0; i < result.tracks.size(); ++i)
    {
        result.totalDurationSec += result.tracks[i].durationSec;
        if (i + 1 < result.tracks.size())
            result.totalDurationSec += result.gapBetweenTracksSec;
    }

    result.success = true;
    return result;
}

SideSplitPlan FolderMixBuilder::computeSideSplit(const FolderScanResult& scan, double allowedSecPerSide)
{
    const auto cassettes = computeMultiCassetteSplit(scan, allowedSecPerSide);
    SideSplitPlan plan;
    if (cassettes.empty())
        return plan;

    const auto& first = cassettes.front();
    plan.sideAEndIndex = first.sideAEndTrack;
    plan.sideADurationSec = first.sideADurationSec;
    plan.needsSideB = first.hasSideB;
    plan.sideBDurationSec = first.sideBDurationSec;
    return plan;
}

std::vector<CassettePlan> FolderMixBuilder::computeMultiCassetteSplit(const FolderScanResult& scan,
                                                                      double allowedSecPerSide)
{
    std::vector<CassettePlan> cassettes;
    const double cap = juce::jmax(1.0, allowedSecPerSide);
    const int n = static_cast<int>(scan.tracks.size());
    int trackIdx = 0;
    int cassetteNum = 1;

    auto packSide = [&](int startIdx, double& outDuration) {
        double cursor = 0.0;
        int i = startIdx;
        for (; i < n; ++i)
        {
            double add = scan.tracks[static_cast<size_t>(i)].durationSec;
            if (i > startIdx)
                add += scan.gapBetweenTracksSec;

            if (cursor + add <= cap + 1.0)
            {
                cursor += add;
                continue;
            }
            break;
        }
        outDuration = cursor;
        return i;
    };

    while (trackIdx < n)
    {
        CassettePlan plan;
        plan.cassetteNumber = cassetteNum++;
        plan.sideAStartTrack = trackIdx;
        trackIdx = packSide(trackIdx, plan.sideADurationSec);
        plan.sideAEndTrack = trackIdx;

        if (trackIdx >= n)
        {
            cassettes.push_back(plan);
            break;
        }

        plan.sideBStartTrack = trackIdx;
        trackIdx = packSide(trackIdx, plan.sideBDurationSec);
        plan.sideBEndTrack = trackIdx;
        plan.hasSideB = plan.sideBEndTrack > plan.sideBStartTrack;
        cassettes.push_back(plan);
    }

    return cassettes;
}

FolderFitReport FolderMixBuilder::analyzeFit(const FolderScanResult& scan, const TapeLengthSpec& tape)
{
    FolderFitReport report;
    report.tape = tape;
    report.trackCount = static_cast<int>(scan.tracks.size());
    report.allowedSec = tape.minutesPerSide * 60.0;
    report.requiredSec = scan.totalDurationSec;
    report.overOneSideSec = juce::jmax(0.0, report.requiredSec - report.allowedSec);
    report.overCassetteSec = juce::jmax(0.0, report.requiredSec - 2.0 * report.allowedSec);
    report.fitsOneSide = report.overOneSideSec < 1.0;
    report.fitsOnCassette = report.overCassetteSec < 1.0;

    report.fits = true;
    for (const auto& t : scan.tracks)
    {
        if (t.durationSec > report.allowedSec + 1.0)
            report.fits = false;
    }

    report.cassettes = computeMultiCassetteSplit(scan, report.allowedSec);
    report.cassetteCount = static_cast<int>(report.cassettes.size());
    report.split = computeSideSplit(scan, report.allowedSec);
    report.sideATrackCount = report.split.sideAEndIndex;
    report.sideBTrackCount = report.trackCount - report.sideATrackCount;
    return report;
}

namespace
{

void appendTracksToSide(TapeSide& side,
                        const FolderScanResult& scan,
                        size_t beginIndex,
                        size_t endIndex,
                        double allowedSecPerSide,
                        int& trackIndexCounter)
{
    side.maxDurationSec = allowedSecPerSide;
    double cursor = 0.0;

    for (size_t i = beginIndex; i < endIndex; ++i)
    {
        if (i > beginIndex)
            cursor += scan.gapBetweenTracksSec;

        TapeClip clip;
        clip.sourceFile = scan.tracks[i].file;
        clip.displayTitle = scan.tracks[i].displayName;
        clip.durationSec = scan.tracks[i].durationSec;
        clip.startTimeSec = cursor;
        clip.trackIndex = trackIndexCounter++;
        side.clips.push_back(clip);
        cursor += clip.durationSec;
    }
}

}

MixtapeProject FolderMixBuilder::buildCassetteProject(const FolderScanResult& scan,
                                                    const CassettePlan& plan,
                                                    const juce::String& projectName,
                                                    CassetteProfile profile,
                                                    const MasteringOptions& mastering,
                                                    double allowedSecPerSide)
{
    MixtapeProject project;
    project.name = projectName;
    project.profile = std::move(profile);
    project.mastering = mastering;

    int trackIndex = plan.sideAStartTrack + 1;

    appendTracksToSide(project.sideA,
                       scan,
                       static_cast<size_t>(plan.sideAStartTrack),
                       static_cast<size_t>(plan.sideAEndTrack),
                       allowedSecPerSide,
                       trackIndex);

    if (plan.hasSideB)
    {
        appendTracksToSide(project.sideB,
                           scan,
                           static_cast<size_t>(plan.sideBStartTrack),
                           static_cast<size_t>(plan.sideBEndTrack),
                           allowedSecPerSide,
                           trackIndex);
    }

    return project;
}

MixtapeProject FolderMixBuilder::buildSplitProject(const FolderScanResult& scan,
                                                   const juce::String& projectName,
                                                   CassetteProfile profile,
                                                   const MasteringOptions& mastering,
                                                   double allowedSecPerSide)
{
    const auto cassettes = computeMultiCassetteSplit(scan, allowedSecPerSide);
    if (cassettes.empty())
        return {};

    return buildCassetteProject(scan,
                                cassettes.front(),
                                projectName,
                                std::move(profile),
                                mastering,
                                allowedSecPerSide);
}

MixtapeProject FolderMixBuilder::buildSequentialProject(const FolderScanResult& scan,
                                                        const juce::String& projectName,
                                                        CassetteProfile profile,
                                                        const MasteringOptions& mastering)
{
    const double cap = juce::jmax(scan.totalDurationSec + 60.0, 30.0 * 60.0);
    return buildSplitProject(scan, projectName, std::move(profile), mastering, cap);
}

}
