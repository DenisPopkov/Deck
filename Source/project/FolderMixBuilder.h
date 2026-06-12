#pragma once

#include <juce_core/juce_core.h>
#include "MixtapeProject.h"
#include "../dsp/CassetteProfile.h"
#include "../dsp/MasteringOptions.h"

namespace cassette
{

enum class TapeLengthPreset
{
    C60 = 0,
    C90,
    C120,
    Custom
};

struct TapeLengthSpec
{
    juce::String label;
    double minutesPerSide = 45.0;
};

TapeLengthSpec tapeLengthSpecForPreset(TapeLengthPreset preset, double customMinutesPerSide);

struct FolderTrackInfo
{
    juce::File file;
    juce::String displayName;
    double durationSec = 0.0;
};

struct FolderScanResult
{
    bool success = false;
    juce::String error;
    juce::File folder;
    std::vector<FolderTrackInfo> tracks;
    double totalDurationSec = 0.0;
    double gapBetweenTracksSec = 2.0;
};

struct SideSplitPlan
{
    int sideAEndIndex = 0;
    double sideADurationSec = 0.0;
    double sideBDurationSec = 0.0;
    bool needsSideB = false;
};

struct CassettePlan
{
    int cassetteNumber = 1;
    int sideAStartTrack = 0;
    int sideAEndTrack = 0;
    int sideBStartTrack = 0;
    int sideBEndTrack = 0;
    double sideADurationSec = 0.0;
    double sideBDurationSec = 0.0;
    bool hasSideB = false;

    int sideATrackCount() const { return juce::jmax(0, sideAEndTrack - sideAStartTrack); }
    int sideBTrackCount() const { return hasSideB ? juce::jmax(0, sideBEndTrack - sideBStartTrack) : 0; }
};

struct FolderFitReport
{
    TapeLengthSpec tape;
    double allowedSec = 0.0;
    double requiredSec = 0.0;
    double overOneSideSec = 0.0;
    double overCassetteSec = 0.0;
    bool fitsOneSide = false;
    bool fitsOnCassette = false;
    bool fits = false;
    int trackCount = 0;
    int sideATrackCount = 0;
    int sideBTrackCount = 0;
    int cassetteCount = 1;
    SideSplitPlan split;
    std::vector<CassettePlan> cassettes;

    juce::String summary() const;
};

class FolderMixBuilder
{
public:
    static FolderScanResult scanFolder(const juce::File& folder, double gapBetweenTracksSec = 2.0);

    static FolderFitReport analyzeFit(const FolderScanResult& scan, const TapeLengthSpec& tape);

    static FolderFitReport analyzeLayout(const FolderScanResult& scan,
                                         int sideAEndIndex,
                                         const TapeLengthSpec& tape);

    static double sideDurationSec(const std::vector<FolderTrackInfo>& tracks, double gapBetweenTracksSec);

    static SideSplitPlan computeSideSplit(const FolderScanResult& scan, double allowedSecPerSide);

    static std::vector<CassettePlan> computeMultiCassetteSplit(const FolderScanResult& scan,
                                                               double allowedSecPerSide);

    static MixtapeProject buildSplitProject(const FolderScanResult& scan,
                                            const juce::String& projectName,
                                            CassetteProfile profile,
                                            const MasteringOptions& mastering,
                                            double allowedSecPerSide);

    static MixtapeProject buildProjectFromSides(const std::vector<FolderTrackInfo>& sideATracks,
                                                const std::vector<FolderTrackInfo>& sideBTracks,
                                                const juce::File& folder,
                                                double gapBetweenTracksSec,
                                                const juce::String& projectName,
                                                CassetteProfile profile,
                                                const MasteringOptions& mastering,
                                                double allowedSecPerSide);

    static MixtapeProject buildCassetteProject(const FolderScanResult& scan,
                                               const CassettePlan& plan,
                                               const juce::String& projectName,
                                               CassetteProfile profile,
                                               const MasteringOptions& mastering,
                                               double allowedSecPerSide);

    static MixtapeProject buildSequentialProject(const FolderScanResult& scan,
                                                 const juce::String& projectName,
                                                 CassetteProfile profile,
                                                 const MasteringOptions& mastering);

    static juce::String formatDuration(double seconds);
};

}
