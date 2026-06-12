#include "MixtapeEditController.h"

namespace cassette
{

namespace
{

class LayoutUndoAction : public juce::UndoableAction
{
public:
    LayoutUndoAction(MixtapeEditController& ownerIn, MixtapeEditController::LayoutSnapshot beforeIn)
        : owner(ownerIn), before(std::move(beforeIn))
    {
    }

    bool perform() override
    {
        after = owner.captureSnapshot();
        owner.restoreSnapshot(before);
        return true;
    }

    bool undo() override
    {
        owner.restoreSnapshot(after);
        return true;
    }

private:
    MixtapeEditController& owner;
    MixtapeEditController::LayoutSnapshot before;
    MixtapeEditController::LayoutSnapshot after;
};

} // namespace

MixtapeEditController::LayoutSnapshot MixtapeEditController::captureSnapshot() const
{
    LayoutSnapshot snap;
    snap.sideA = sideATracks;
    snap.sideB = sideBTracks;
    return snap;
}

void MixtapeEditController::restoreSnapshot(const LayoutSnapshot& snapshot)
{
    sideATracks = snapshot.sideA;
    sideBTracks = snapshot.sideB;
}

void MixtapeEditController::pushUndoSnapshot()
{
    LayoutSnapshot snap;
    snap.sideA = sideATracks;
    snap.sideB = sideBTracks;
    undoManager.beginNewTransaction();
    undoManager.perform(new LayoutUndoAction(*this, std::move(snap)));
    manualEdits = true;
}

void MixtapeEditController::clear()
{
    folder = juce::File();
    gapSec = 2.0;
    fullScan = {};
    cassetteLayouts.clear();
    cassetteCount = 1;
    activeCassetteIndex = 0;
    sideATracks.clear();
    sideBTracks.clear();
    manualEdits = false;
    undoManager.clearUndoHistory();
}

MixtapeEditController::LayoutSnapshot MixtapeEditController::layoutFromPlan(const FolderScanResult& scan,
                                                                            const CassettePlan& plan) const
{
    LayoutSnapshot snap;
    for (int i = plan.sideAStartTrack; i < plan.sideAEndTrack; ++i)
        snap.sideA.push_back(scan.tracks[static_cast<size_t>(i)]);

    if (plan.hasSideB)
    {
        for (int i = plan.sideBStartTrack; i < plan.sideBEndTrack; ++i)
            snap.sideB.push_back(scan.tracks[static_cast<size_t>(i)]);
    }
    return snap;
}

FolderScanResult MixtapeEditController::scanFromLayout(const juce::File& sourceFolder,
                                                       double gap,
                                                       const LayoutSnapshot& layout)
{
    FolderScanResult scan;
    scan.success = true;
    scan.folder = sourceFolder;
    scan.gapBetweenTracksSec = gap;

    for (const auto& t : layout.sideA)
        scan.tracks.push_back(t);
    for (const auto& t : layout.sideB)
        scan.tracks.push_back(t);

    scan.totalDurationSec = 0.0;
    for (size_t i = 0; i < scan.tracks.size(); ++i)
    {
        scan.totalDurationSec += scan.tracks[i].durationSec;
        if (i + 1 < scan.tracks.size())
            scan.totalDurationSec += gap;
    }
    return scan;
}

void MixtapeEditController::loadCassetteLayout(int index)
{
    if (index < 0 || index >= static_cast<int>(cassetteLayouts.size()))
        return;
    restoreSnapshot(cassetteLayouts[static_cast<size_t>(index)]);
}

void MixtapeEditController::saveActiveCassetteLayout()
{
    if (activeCassetteIndex >= 0 && activeCassetteIndex < static_cast<int>(cassetteLayouts.size()))
        cassetteLayouts[static_cast<size_t>(activeCassetteIndex)] = captureSnapshot();
}

void MixtapeEditController::loadFromScan(const FolderScanResult& scan, const FolderFitReport& fit)
{
    clear();
    folder = scan.folder;
    gapSec = scan.gapBetweenTracksSec;
    fullScan = scan;
    cassetteCount = juce::jmax(1, fit.cassetteCount);

    if (fit.cassettes.empty())
    {
        cassetteLayouts.push_back(layoutFromPlan(scan, CassettePlan {}));
        const int split = juce::jlimit(0, static_cast<int>(scan.tracks.size()), fit.split.sideAEndIndex);
        cassetteLayouts[0].sideA.clear();
        cassetteLayouts[0].sideB.clear();
        for (int i = 0; i < static_cast<int>(scan.tracks.size()); ++i)
        {
            if (i < split)
                cassetteLayouts[0].sideA.push_back(scan.tracks[static_cast<size_t>(i)]);
            else
                cassetteLayouts[0].sideB.push_back(scan.tracks[static_cast<size_t>(i)]);
        }
    }
    else
    {
        for (const auto& plan : fit.cassettes)
            cassetteLayouts.push_back(layoutFromPlan(scan, plan));
    }

    activeCassetteIndex = 0;
    loadCassetteLayout(0);
}

void MixtapeEditController::setActiveCassetteIndex(int index)
{
    const int clamped = juce::jlimit(0, cassetteCount - 1, index);
    if (clamped == activeCassetteIndex)
        return;

    saveActiveCassetteLayout();
    activeCassetteIndex = clamped;
    loadCassetteLayout(activeCassetteIndex);
}

MixtapeEditController::LayoutSnapshot MixtapeEditController::layoutForCassette(int cassetteIndex) const
{
    if (cassetteIndex == activeCassetteIndex)
        return captureSnapshot();

    if (cassetteIndex >= 0 && cassetteIndex < static_cast<int>(cassetteLayouts.size()))
        return cassetteLayouts[static_cast<size_t>(cassetteIndex)];

    return {};
}

FolderScanResult MixtapeEditController::mergedScan() const
{
    return scanFromLayout(folder, gapSec, captureSnapshot());
}

FolderScanResult MixtapeEditController::mergedFullScan() const
{
    FolderScanResult scan;
    scan.success = true;
    scan.folder = folder;
    scan.gapBetweenTracksSec = gapSec;

    for (int c = 0; c < cassetteCount; ++c)
    {
        const auto layout = layoutForCassette(c);
        for (const auto& t : layout.sideA)
            scan.tracks.push_back(t);
        for (const auto& t : layout.sideB)
            scan.tracks.push_back(t);
    }

    scan.totalDurationSec = 0.0;
    for (size_t i = 0; i < scan.tracks.size(); ++i)
    {
        scan.totalDurationSec += scan.tracks[i].durationSec;
        if (i + 1 < scan.tracks.size())
            scan.totalDurationSec += gapSec;
    }
    return scan;
}

FolderFitReport MixtapeEditController::computeFit(const TapeLengthSpec& tape) const
{
    return FolderMixBuilder::analyzeLayout(mergedScan(), static_cast<int>(sideATracks.size()), tape);
}

FolderFitReport MixtapeEditController::computeFullFit(const TapeLengthSpec& tape) const
{
    return FolderMixBuilder::analyzeFit(mergedFullScan(), tape);
}

MixtapeProject MixtapeEditController::buildPreviewProject(double allowedSecPerSide) const
{
    return buildProject(folder.getFileName(), CassetteProfile::fromFormulation(TapeFormulation::TypeIV), {}, allowedSecPerSide);
}

MixtapeProject MixtapeEditController::buildProject(const juce::String& projectName,
                                                   CassetteProfile profile,
                                                   const MasteringOptions& mastering,
                                                   double allowedSecPerSide) const
{
    return FolderMixBuilder::buildProjectFromSides(sideATracks,
                                                   sideBTracks,
                                                   folder,
                                                   gapSec,
                                                   projectName,
                                                   std::move(profile),
                                                   mastering,
                                                   allowedSecPerSide);
}

MixtapeProject MixtapeEditController::buildProjectForCassette(int cassetteIndex,
                                                              const juce::String& projectName,
                                                              CassetteProfile profile,
                                                              const MasteringOptions& mastering,
                                                              double allowedSecPerSide) const
{
    const auto layout = layoutForCassette(cassetteIndex);
    return FolderMixBuilder::buildProjectFromSides(layout.sideA,
                                                   layout.sideB,
                                                   folder,
                                                   gapSec,
                                                   projectName,
                                                   std::move(profile),
                                                   mastering,
                                                   allowedSecPerSide);
}

bool MixtapeEditController::hasSideOverflow(const TapeLengthSpec& tape) const
{
    const auto fit = computeFit(tape);
    return FolderMixBuilder::sideDurationSec(sideATracks, gapSec) > fit.allowedSec + 2.0 * gapSec + 1.0
           || FolderMixBuilder::sideDurationSec(sideBTracks, gapSec) > fit.allowedSec + 2.0 * gapSec + 1.0;
}

bool MixtapeEditController::canPrepare(const TapeLengthSpec& tape) const
{
    for (int c = 0; c < cassetteCount; ++c)
    {
        const auto layout = layoutForCassette(c);
        const auto scan = scanFromLayout(folder, gapSec, layout);
        const auto fit = FolderMixBuilder::analyzeLayout(scan, static_cast<int>(layout.sideA.size()), tape);
        if (!fit.fits)
            return false;
    }
    return true;
}

bool MixtapeEditController::reorderWithinSide(int sideIndex, int fromRow, int toRow)
{
    auto& tracks = sideIndex == 0 ? sideATracks : sideBTracks;
    if (fromRow < 0 || toRow < 0 || fromRow >= static_cast<int>(tracks.size())
        || toRow >= static_cast<int>(tracks.size()) || fromRow == toRow)
        return false;

    pushUndoSnapshot();
    auto item = std::move(tracks[static_cast<size_t>(fromRow)]);
    tracks.erase(tracks.begin() + fromRow);
    tracks.insert(tracks.begin() + toRow, std::move(item));
    saveActiveCassetteLayout();
    return true;
}

bool MixtapeEditController::moveToSide(int fromSide, int fromRow, int toSide, int toRow)
{
    if (fromSide == toSide)
        return reorderWithinSide(fromSide, fromRow, toRow);

    auto& from = fromSide == 0 ? sideATracks : sideBTracks;
    auto& to = toSide == 0 ? sideATracks : sideBTracks;
    if (fromRow < 0 || fromRow >= static_cast<int>(from.size()))
        return false;

    toRow = juce::jlimit(0, static_cast<int>(to.size()), toRow);
    pushUndoSnapshot();

    auto item = std::move(from[static_cast<size_t>(fromRow)]);
    from.erase(from.begin() + fromRow);
    to.insert(to.begin() + static_cast<size_t>(toRow), std::move(item));
    saveActiveCassetteLayout();
    return true;
}

bool MixtapeEditController::moveRowUp(int sideIndex, int row)
{
    if (row <= 0)
        return false;
    return reorderWithinSide(sideIndex, row, row - 1);
}

bool MixtapeEditController::moveRowDown(int sideIndex, int row)
{
    const int count = sideIndex == 0 ? static_cast<int>(sideATracks.size()) : static_cast<int>(sideBTracks.size());
    if (row < 0 || row >= count - 1)
        return false;
    return reorderWithinSide(sideIndex, row, row + 1);
}

bool MixtapeEditController::deleteTrack(int sideIndex, int row)
{
    auto& tracks = sideIndex == 0 ? sideATracks : sideBTracks;
    if (row < 0 || row >= static_cast<int>(tracks.size()))
        return false;

    pushUndoSnapshot();
    tracks.erase(tracks.begin() + row);
    saveActiveCassetteLayout();
    return true;
}

void MixtapeEditController::rebalance(const TapeLengthSpec& tape)
{
    saveActiveCassetteLayout();

    const auto scan = mergedFullScan();
    const auto fit = FolderMixBuilder::analyzeFit(scan, tape);
    fullScan = scan;
    cassetteCount = juce::jmax(1, fit.cassetteCount);
    cassetteLayouts.clear();

    if (fit.cassettes.empty())
    {
        LayoutSnapshot snap;
        const int split = juce::jlimit(0, static_cast<int>(scan.tracks.size()), fit.split.sideAEndIndex);
        for (int i = 0; i < static_cast<int>(scan.tracks.size()); ++i)
        {
            if (i < split)
                snap.sideA.push_back(scan.tracks[static_cast<size_t>(i)]);
            else
                snap.sideB.push_back(scan.tracks[static_cast<size_t>(i)]);
        }
        cassetteLayouts.push_back(std::move(snap));
    }
    else
    {
        for (const auto& plan : fit.cassettes)
            cassetteLayouts.push_back(layoutFromPlan(scan, plan));
    }

    activeCassetteIndex = juce::jmin(activeCassetteIndex, cassetteCount - 1);
    loadCassetteLayout(activeCassetteIndex);
    manualEdits = false;
    undoManager.clearUndoHistory();
}

} // namespace cassette
