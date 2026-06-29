#include "FolderMixBuilder.h"
#include "../io/AudioFileLoader.h"
#include "../util/AppLog.h"
#include <algorithm>
#include <cmath>
#include <limits>

namespace cassette
{

namespace
{

bool isExportedSideWav(const juce::File& file)
{
    const auto name = file.getFileName();
    return name.endsWithIgnoreCase(" Side A.wav") || name.endsWithIgnoreCase(" Side B.wav");
}

double durationRangeSec(const FolderScanResult& scan, int beginIndex, int endIndex)
{
    if (beginIndex >= endIndex)
        return 0.0;

    double cursor = 0.0;
    for (int i = beginIndex; i < endIndex; ++i)
    {
        if (i > beginIndex)
            cursor += scan.gapBetweenTracksSec;
        cursor += scan.tracks[static_cast<size_t>(i)].durationSec;
    }
    return cursor;
}

double sideCapacitySec(const FolderScanResult& scan, double capSec)
{
    return capSec + 2.0 * scan.gapBetweenTracksSec + 1.0;
}

bool sideFitsCapacity(const FolderScanResult& scan, double durationSec, double capSec)
{
    return durationSec <= sideCapacitySec(scan, capSec);
}

int greedyPackEndIndex(const FolderScanResult& scan, int beginIndex, int endIndex, double capSec)
{
    double cursor = 0.0;
    int i = beginIndex;
    for (; i < endIndex; ++i)
    {
        double add = scan.tracks[static_cast<size_t>(i)].durationSec;
        if (i > beginIndex)
            add += scan.gapBetweenTracksSec;

        if (cursor + add <= sideCapacitySec(scan, capSec))
        {
            cursor += add;
            continue;
        }
        break;
    }
    return i;
}

int findBalancedSplitIndex(const FolderScanResult& scan, int beginIndex, int endIndex, double capSec)
{
    if (beginIndex >= endIndex)
        return beginIndex;

    const int trackCount = endIndex - beginIndex;
    if (trackCount <= 1)
        return endIndex;

    const double totalSec = durationRangeSec(scan, beginIndex, endIndex);
    if (sideFitsCapacity(scan, totalSec, capSec))
        return endIndex;

    int bestSplit = -1;
    double bestDiff = std::numeric_limits<double>::max();

    for (int split = beginIndex + 1; split < endIndex; ++split)
    {
        const double sideASec = durationRangeSec(scan, beginIndex, split);
        const double sideBSec = durationRangeSec(scan, split, endIndex);
        if (!sideFitsCapacity(scan, sideASec, capSec) || !sideFitsCapacity(scan, sideBSec, capSec))
            continue;

        const double diff = std::abs(sideASec - sideBSec);
        if (diff < bestDiff)
        {
            bestDiff = diff;
            bestSplit = split;
        }
    }

    if (bestSplit >= 0)
        return bestSplit;

    return greedyPackEndIndex(scan, beginIndex, endIndex, capSec);
}

int endIndexForCassetteChunk(const FolderScanResult& scan, int beginIndex, double maxCassetteSec)
{
    const int n = static_cast<int>(scan.tracks.size());
    int endIndex = beginIndex + 1;
    while (endIndex < n
           && durationRangeSec(scan, beginIndex, endIndex + 1) <= maxCassetteSec + 1.0)
        ++endIndex;
    return endIndex;
}

}

TapeLengthSpec tapeLengthSpecForPreset(TapeLengthPreset preset, double customTotalMinutes)
{
    switch (preset)
    {
        case TapeLengthPreset::C60: return { "C60", 30.0 };
        case TapeLengthPreset::C120: return { "C120", 60.0 };
        case TapeLengthPreset::Custom:
        {
            const auto totalMins = juce::jmax(2.0, customTotalMinutes);
            const auto perSideMins = totalMins * 0.5;
            const auto label = "Custom (" + juce::String(juce::roundToInt(totalMins)) + " min)";
            return { label, perSideMins };
        }
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
    ScopedTimer scanTimer("folder-scan", folder.getFileName());

    FolderScanResult result;
    result.folder = folder;
    result.gapBetweenTracksSec = juce::jmax(0.0, gapBetweenTracksSec);

    if (!folder.isDirectory())
    {
        result.error = "Not a folder: " + folder.getFullPathName();
        log("folder-scan error: " + result.error);
        return result;
    }

    juce::Array<juce::File> files;
    folder.findChildFiles(files, juce::File::findFiles, false);
    log("folder-scan: found " + juce::String(files.size()) + " files in " + folder.getFullPathName());

    for (const auto& f : files)
    {
        if (!AudioFileLoader::isSupportedAudioFile(f))
            continue;

        if (isExportedSideWav(f))
        {
            log("folder-scan skip (export output): " + f.getFileName());
            continue;
        }

        const double t0 = juce::Time::getMillisecondCounterHiRes();
        FolderTrackInfo info;
        info.file = f;
        info.displayName = f.getFileNameWithoutExtension();
        info.durationSec = AudioFileLoader::probeDurationSec(f);
        const double readMs = juce::Time::getMillisecondCounterHiRes() - t0;

        if (info.durationSec <= 0.0)
        {
            log("folder-scan skip (no duration): " + f.getFileName());
            continue;
        }

        logTiming("folder-scan-track",
                  readMs,
                  f.getFileName() + " (" + formatDuration(info.durationSec) + ")");
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
    log("folder-scan done: " + juce::String(result.tracks.size()) + " tracks, total "
        + formatDuration(result.totalDurationSec));
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
    const double maxCassetteSec = cap * 2.0;
    const int n = static_cast<int>(scan.tracks.size());
    int trackIdx = 0;
    int cassetteNum = 1;

    while (trackIdx < n)
    {
        const int chunkEnd = endIndexForCassetteChunk(scan, trackIdx, maxCassetteSec);
        const double chunkSec = durationRangeSec(scan, trackIdx, chunkEnd);

        CassettePlan plan;
        plan.cassetteNumber = cassetteNum++;
        plan.sideAStartTrack = trackIdx;

        if (sideFitsCapacity(scan, chunkSec, cap))
        {
            plan.sideAEndTrack = chunkEnd;
            plan.sideADurationSec = chunkSec;
            plan.hasSideB = false;
            cassettes.push_back(plan);
            trackIdx = chunkEnd;
            continue;
        }

        const int split = findBalancedSplitIndex(scan, trackIdx, chunkEnd, cap);
        plan.sideAEndTrack = split;
        plan.sideADurationSec = durationRangeSec(scan, trackIdx, split);

        if (split < chunkEnd)
        {
            plan.sideBStartTrack = split;
            plan.sideBEndTrack = chunkEnd;
            plan.sideBDurationSec = durationRangeSec(scan, split, chunkEnd);
            plan.hasSideB = true;
        }

        cassettes.push_back(plan);
        trackIdx = chunkEnd;
    }

    return cassettes;
}

FolderFitReport FolderMixBuilder::analyzeFit(const FolderScanResult& scan, const TapeLengthSpec& tape)
{
    return analyzeLayout(scan, -1, tape);
}

double FolderMixBuilder::sideDurationSec(const std::vector<FolderTrackInfo>& tracks, double gapBetweenTracksSec)
{
    if (tracks.empty())
        return 0.0;

    double total = 0.0;
    for (size_t i = 0; i < tracks.size(); ++i)
    {
        total += tracks[i].durationSec;
        if (i + 1 < tracks.size())
            total += gapBetweenTracksSec;
    }
    return total;
}

FolderFitReport FolderMixBuilder::analyzeLayout(const FolderScanResult& scan,
                                                int sideAEndIndex,
                                                const TapeLengthSpec& tape)
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

    if (sideAEndIndex < 0)
        report.split = computeSideSplit(scan, report.allowedSec);
    else
    {
        const int split = juce::jlimit(0, report.trackCount, sideAEndIndex);
        report.split.sideAEndIndex = split;
        report.split.sideADurationSec = durationRangeSec(scan, 0, split);
        report.split.needsSideB = split < report.trackCount;
        report.split.sideBDurationSec = durationRangeSec(scan, split, report.trackCount);
    }

    report.sideATrackCount = report.split.sideAEndIndex;
    report.sideBTrackCount = report.trackCount - report.sideATrackCount;

    const bool sideAFits = sideFitsCapacity(scan, report.split.sideADurationSec, report.allowedSec);
    const bool sideBFits = !report.split.needsSideB
                           || sideFitsCapacity(scan, report.split.sideBDurationSec, report.allowedSec);
    report.fits = report.fits && sideAFits && sideBFits;
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

MixtapeProject FolderMixBuilder::buildProjectFromSides(const std::vector<FolderTrackInfo>& sideATracks,
                                                       const std::vector<FolderTrackInfo>& sideBTracks,
                                                       const juce::File& folder,
                                                       double gapBetweenTracksSec,
                                                       const juce::String& projectName,
                                                       CassetteProfile profile,
                                                       const MasteringOptions& mastering,
                                                       double allowedSecPerSide)
{
    FolderScanResult scan;
    scan.success = true;
    scan.folder = folder;
    scan.gapBetweenTracksSec = gapBetweenTracksSec;
    scan.tracks.reserve(sideATracks.size() + sideBTracks.size());

    for (const auto& t : sideATracks)
        scan.tracks.push_back(t);
    for (const auto& t : sideBTracks)
        scan.tracks.push_back(t);

    scan.totalDurationSec = 0.0;
    for (size_t i = 0; i < scan.tracks.size(); ++i)
    {
        scan.totalDurationSec += scan.tracks[i].durationSec;
        if (i + 1 < scan.tracks.size())
            scan.totalDurationSec += gapBetweenTracksSec;
    }

    CassettePlan plan;
    plan.cassetteNumber = 1;
    plan.sideAStartTrack = 0;
    plan.sideAEndTrack = static_cast<int>(sideATracks.size());
    plan.sideADurationSec = sideDurationSec(sideATracks, gapBetweenTracksSec);
    plan.hasSideB = !sideBTracks.empty();
    if (plan.hasSideB)
    {
        plan.sideBStartTrack = plan.sideAEndTrack;
        plan.sideBEndTrack = static_cast<int>(scan.tracks.size());
        plan.sideBDurationSec = sideDurationSec(sideBTracks, gapBetweenTracksSec);
    }

    return buildCassetteProject(scan,
                                plan,
                                projectName,
                                std::move(profile),
                                mastering,
                                allowedSecPerSide);
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
