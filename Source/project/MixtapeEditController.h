#pragma once

#include <juce_data_structures/juce_data_structures.h>
#include "FolderMixBuilder.h"
#include "MixtapeProject.h"
#include "../dsp/CassetteProfile.h"
#include "../dsp/MasteringOptions.h"

namespace cassette
{

class MixtapeEditController
{
public:
    static constexpr int kMaxUndoSteps = 50;

    void loadFromScan(const FolderScanResult& scan, const FolderFitReport& fit);
    void clear();

    const std::vector<FolderTrackInfo>& sideA() const { return sideATracks; }
    const std::vector<FolderTrackInfo>& sideB() const { return sideBTracks; }

    juce::File sourceFolder() const { return folder; }
    double gapBetweenTracksSec() const { return gapSec; }

    int getCassetteCount() const { return cassetteCount; }
    int getActiveCassetteIndex() const { return activeCassetteIndex; }
    void setActiveCassetteIndex(int index);

    FolderScanResult mergedScan() const;
    FolderScanResult mergedFullScan() const;
    FolderFitReport computeFit(const TapeLengthSpec& tape) const;
    FolderFitReport computeFullFit(const TapeLengthSpec& tape) const;

    MixtapeProject buildProject(const juce::String& projectName,
                                CassetteProfile profile,
                                const MasteringOptions& mastering,
                                double allowedSecPerSide) const;

    MixtapeProject buildPreviewProject(double allowedSecPerSide) const;
    MixtapeProject buildProjectForCassette(int cassetteIndex,
                                           const juce::String& projectName,
                                           CassetteProfile profile,
                                           const MasteringOptions& mastering,
                                           double allowedSecPerSide) const;

    bool hasSideOverflow(const TapeLengthSpec& tape) const;
    bool canPrepare(const TapeLengthSpec& tape) const;

    bool reorderWithinSide(int sideIndex, int fromRow, int toRow);
    bool moveToSide(int fromSide, int fromRow, int toSide, int toRow);
    bool moveRowUp(int sideIndex, int row);
    bool moveRowDown(int sideIndex, int row);
    bool deleteTrack(int sideIndex, int row);
    void rebalance(const TapeLengthSpec& tape);
    /** Recompute cassette count from the current track list (respects deletions). */
    void syncCassettePlan(const TapeLengthSpec& tape);

    juce::UndoManager& getUndoManager() { return undoManager; }
    bool hasManualEdits() const { return manualEdits; }
    void saveActiveCassetteLayout();

    struct LayoutSnapshot
    {
        std::vector<FolderTrackInfo> sideA;
        std::vector<FolderTrackInfo> sideB;
    };

    LayoutSnapshot captureSnapshot() const;
    void restoreSnapshot(const LayoutSnapshot& snapshot);
    LayoutSnapshot layoutForCassette(int cassetteIndex) const;

private:
    void pushUndoSnapshot();
    void loadCassetteLayout(int index);
    LayoutSnapshot layoutFromPlan(const FolderScanResult& scan, const CassettePlan& plan) const;
    static FolderScanResult scanFromLayout(const juce::File& sourceFolder,
                                           double gap,
                                           const LayoutSnapshot& layout);

    juce::File folder;
    double gapSec = 2.0;
    FolderScanResult fullScan;
    std::vector<LayoutSnapshot> cassetteLayouts;
    int cassetteCount = 1;
    int activeCassetteIndex = 0;
    std::vector<FolderTrackInfo> sideATracks;
    std::vector<FolderTrackInfo> sideBTracks;
    bool manualEdits = false;
    juce::UndoManager undoManager;
};

} // namespace cassette
