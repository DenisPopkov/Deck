#include "TrackListEditor.h"
#include "ConfirmDialog.h"
#include "UiTheme.h"
#include "../io/AudioFileLoader.h"
#include "../util/AppLocale.h"

namespace cassette
{

namespace
{

juce::String formatClock(double sec)
{
    const int total = juce::jmax(0, static_cast<int>(sec));
    return juce::String(total / 60) + ":" + juce::String(total % 60).paddedLeft('0', 2);
}

void drawPlayIcon(juce::Graphics& g, juce::Rectangle<int> area, juce::Colour colour)
{
    juce::Path triangle;
    const auto cx = area.getCentreX();
    const auto cy = area.getCentreY();
    triangle.addTriangle(static_cast<float>(cx - 4), static_cast<float>(cy - 5),
                         static_cast<float>(cx - 4), static_cast<float>(cy + 5),
                         static_cast<float>(cx + 6), static_cast<float>(cy));
    g.setColour(colour);
    g.fillPath(triangle);
}

void drawPauseIcon(juce::Graphics& g, juce::Rectangle<int> area, juce::Colour colour)
{
    g.setColour(colour);
    const auto bar = area.withSizeKeepingCentre(3, 10);
    g.fillRect(bar.withX(bar.getX() - 3));
    g.fillRect(bar.withX(bar.getX() + 3));
}

void drawCheckbox(juce::Graphics& g, juce::Rectangle<int> area, bool checked, bool hover)
{
    using namespace ui;

    auto box = area.withSizeKeepingCentre(14, 14);
    g.setColour(checked ? Theme::accent() : Theme::card());
    g.fillRoundedRectangle(box.toFloat(), 2.0f);
    g.setColour(hover || checked ? Theme::accent() : Theme::borderLight());
    g.drawRoundedRectangle(box.toFloat(), 2.0f, 1.0f);

    if (checked)
    {
        g.setColour(juce::Colours::white);
        g.drawLine(static_cast<float>(box.getX() + 3),
                   static_cast<float>(box.getCentreY()),
                   static_cast<float>(box.getCentreX()),
                   static_cast<float>(box.getBottom() - 3),
                   1.6f);
        g.drawLine(static_cast<float>(box.getCentreX()),
                   static_cast<float>(box.getBottom() - 3),
                   static_cast<float>(box.getRight() - 3),
                   static_cast<float>(box.getY() + 3),
                   1.6f);
    }
}

} // namespace

void TrackListEditor::TrackPreviewPlayback::attach(juce::AudioDeviceManager& deviceManagerIn)
{
    detach();
    deviceManager = &deviceManagerIn;
    player.setSource(this);
    deviceManager->addAudioCallback(&player);
    prepareForOutput();
}

void TrackListEditor::TrackPreviewPlayback::shutdown()
{
    transport.stop();
    transport.setSource(nullptr);
    readerSource.reset();

    if (deviceManager != nullptr)
    {
        deviceManager->removeAudioCallback(&player);
        deviceManager = nullptr;
    }

    player.setSource(nullptr);
    trackName.clear();
}

void TrackListEditor::TrackPreviewPlayback::detach()
{
    shutdown();
}

bool TrackListEditor::TrackPreviewPlayback::loadFile(const juce::File& file)
{
    stop();
    transport.setSource(nullptr);
    readerSource.reset();

    if (!AudioFileLoader::isSupportedAudioFile(file))
        return false;

    auto* reader = AudioFileLoader::getFormatManager().createReaderFor(file);
    if (reader == nullptr)
        return false;

    readerSource = std::make_unique<juce::AudioFormatReaderSource>(reader, true);
    transport.setSource(readerSource.get(), 0, nullptr, reader->sampleRate);
    trackName = file.getFileNameWithoutExtension();
    prepareForOutput();
    return true;
}

void TrackListEditor::TrackPreviewPlayback::prepareForOutput()
{
    if (deviceManager == nullptr)
        return;

    if (auto* device = deviceManager->getCurrentAudioDevice())
        prepareToPlay(device->getCurrentBufferSizeSamples(), device->getCurrentSampleRate());
}

void TrackListEditor::TrackPreviewPlayback::play()
{
    prepareForOutput();
    transport.start();
}

void TrackListEditor::TrackPreviewPlayback::pause()
{
    transport.stop();
}

void TrackListEditor::TrackPreviewPlayback::togglePlayPause()
{
    if (transport.isPlaying())
        pause();
    else
        play();
}

void TrackListEditor::TrackPreviewPlayback::stop()
{
    transport.stop();
    transport.setPosition(0.0);
}

double TrackListEditor::TrackPreviewPlayback::getPositionSec() const
{
    return transport.getCurrentPosition();
}

double TrackListEditor::TrackPreviewPlayback::getLengthSec() const
{
    return transport.getLengthInSeconds();
}

void TrackListEditor::TrackPreviewPlayback::setPositionSec(double sec)
{
    const auto len = getLengthSec();
    if (len <= 0.0)
        return;

    transport.setPosition(juce::jlimit(0.0, len, sec));
}

void TrackListEditor::TrackPreviewPlayback::prepareToPlay(int samplesPerBlockExpected, double sampleRate)
{
    transport.prepareToPlay(samplesPerBlockExpected, sampleRate);
}

void TrackListEditor::TrackPreviewPlayback::releaseResources()
{
    transport.releaseResources();
}

void TrackListEditor::TrackPreviewPlayback::getNextAudioBlock(const juce::AudioSourceChannelInfo& bufferToFill)
{
    transport.getNextAudioBlock(bufferToFill);
}

TrackListEditor::MiniPlayerBar::MiniPlayerBar(TrackListEditor& ownerIn) : owner(ownerIn)
{
    using namespace ui;

    addAndMakeVisible(playPauseButton);
    addAndMakeVisible(trackLabel);
    addAndMakeVisible(timeLabel);
    addAndMakeVisible(seekSlider);
    addAndMakeVisible(closeButton);

    Theme::styleNeutralButton(playPauseButton);
    Theme::styleNeutralButton(closeButton);
    Theme::applyLabel(trackLabel, Theme::bodyFont(), Theme::textPrimary());
    Theme::applyLabel(timeLabel, Theme::metricFont(), Theme::accent(), juce::Justification::centredRight);
    trackLabel.setInterceptsMouseClicks(false, false);
    timeLabel.setInterceptsMouseClicks(false, false);

    seekSlider.setSliderStyle(juce::Slider::LinearBar);
    seekSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    seekSlider.setRange(0.0, 1.0, 0.01);
    Theme::styleAccentSlider(seekSlider);
    seekSlider.addListener(this);

    playPauseButton.onClick = [this] {
        owner.previewPlayback.togglePlayPause();
        owner.updateMiniPlayerUi();
    };

    closeButton.onClick = [this] { owner.hideMiniPlayer(); };
}

void TrackListEditor::MiniPlayerBar::setSeekFromPlayback(double positionSec, double lengthSec)
{
    seekFromPlayback = true;
    seekSlider.setRange(0.0, juce::jmax(lengthSec, 0.01), 0.01);
    seekSlider.setValue(juce::jlimit(0.0, juce::jmax(lengthSec, 0.01), positionSec), juce::dontSendNotification);
    seekFromPlayback = false;
}

void TrackListEditor::MiniPlayerBar::sliderValueChanged(juce::Slider* slider)
{
    if (slider != &seekSlider || seekFromPlayback)
        return;

    if (owner.miniPlayerSeekDragging)
    {
        owner.updateMiniPlayerUi();
        return;
    }

    owner.previewPlayback.setPositionSec(seekSlider.getValue());
    owner.updateMiniPlayerUi();
}

void TrackListEditor::MiniPlayerBar::sliderDragStarted(juce::Slider* slider)
{
    if (slider != &seekSlider)
        return;

    owner.miniPlayerSeekDragging = true;
    owner.resumePlaybackAfterSeek = owner.previewPlayback.isPlaying();
    if (owner.resumePlaybackAfterSeek)
        owner.previewPlayback.pause();
    owner.updateMiniPlayerUi();
}

void TrackListEditor::MiniPlayerBar::sliderDragEnded(juce::Slider* slider)
{
    if (slider != &seekSlider)
        return;

    owner.previewPlayback.setPositionSec(seekSlider.getValue());
    owner.miniPlayerSeekDragging = false;

    if (owner.resumePlaybackAfterSeek)
    {
        owner.previewPlayback.play();
        owner.resumePlaybackAfterSeek = false;
    }

    owner.updateMiniPlayerUi();
}

void TrackListEditor::MiniPlayerBar::setVisible(bool shouldBeVisible)
{
    juce::Component::setVisible(shouldBeVisible);
}

void TrackListEditor::MiniPlayerBar::paint(juce::Graphics& g)
{
    using namespace ui;

    g.setColour(Theme::panel());
    g.fillRect(getLocalBounds());
    g.setColour(Theme::borderLight());
    g.drawHorizontalLine(0, 0.0f, static_cast<float>(getWidth()));
}

void TrackListEditor::MiniPlayerBar::resized()
{
    static constexpr int kPadH = 12;
    static constexpr int kPadV = 5;
    static constexpr int kRowGap = 6;
    static constexpr int kTopH = 24;
    static constexpr int kSeekH = 14;
    static constexpr int kBtnW = 30;
    static constexpr int kTimeW = 86;

    const auto bounds = getLocalBounds();
    const int contentX = bounds.getX() + kPadH;
    const int contentW = bounds.getWidth() - kPadH * 2;
    const int contentRight = contentX + contentW;

    const int topY = bounds.getY() + kPadV;
    closeButton.setBounds(contentRight - kBtnW, topY, kBtnW, kTopH);
    timeLabel.setBounds(contentRight - kBtnW - kRowGap - kTimeW, topY, kTimeW, kTopH);
    playPauseButton.setBounds(contentX, topY, kBtnW, kTopH);

    const int titleX = contentX + kBtnW + kRowGap;
    const int titleW = timeLabel.getX() - kRowGap - titleX;
    trackLabel.setBounds(titleX, topY, juce::jmax(0, titleW), kTopH);

    const int seekY = topY + kTopH + kRowGap;
    seekSlider.setBounds(contentX, seekY, contentW, kSeekH);
}

void TrackListEditor::TrashZone::setActive(bool activeIn, bool hoverIn)
{
    if (active != activeIn || hover != hoverIn)
    {
        active = activeIn;
        hover = hoverIn;
        setVisible(active);
        repaint();
    }
}

void TrackListEditor::TrashZone::paint(juce::Graphics& g)
{
    using namespace ui;

    const auto bg = hover ? Theme::failRed().withAlpha(0.12f) : Theme::panel();
    g.setColour(bg);
    g.fillRoundedRectangle(getLocalBounds().toFloat(), 6.0f);
    g.setColour(hover ? Theme::failRed() : Theme::borderLight());
    g.drawRoundedRectangle(getLocalBounds().toFloat(), 6.0f, hover ? 2.0f : 1.0f);

    g.setColour(hover ? Theme::failRed() : Theme::textSecondary());
    g.setFont(Theme::bodyFont());
    g.drawText("Drop to remove", getLocalBounds(), juce::Justification::centred);
}

TrackListEditor::SideList::SideList(TrackListEditor& ownerIn, int sideIndexIn)
    : owner(ownerIn), sideIndex(sideIndexIn)
{
    setWantsKeyboardFocus(false);
}

int TrackListEditor::SideList::getTrackCount() const
{
    if (owner.controller == nullptr)
        return 0;
    return sideIndex == 0 ? static_cast<int>(owner.controller->sideA().size())
                          : static_cast<int>(owner.controller->sideB().size());
}

int TrackListEditor::SideList::getContentHeight(int minimumHeight) const
{
    return juce::jmax(minimumHeight, 8 + getTrackCount() * kRowH);
}

int TrackListEditor::SideList::rowAtY(int y) const
{
    const int count = getTrackCount();
    if (count <= 0)
        return 0;

    for (int row = 0; row < count; ++row)
    {
        if (y < rowBounds(row).getCentreY())
            return row;
    }

    return count;
}

int TrackListEditor::SideList::hitTestRow(juce::Point<int> pos) const
{
    const int count = getTrackCount();
    for (int row = 0; row < count; ++row)
    {
        if (rowBounds(row).contains(pos))
            return row;
    }
    return -1;
}

juce::Rectangle<int> TrackListEditor::SideList::rowBounds(int row) const
{
    static constexpr int kRowInnerH = kRowH - 2;
    const int y = 4 + row * kRowH + (kRowH - kRowInnerH) / 2;
    return { kSideListInset, y, getWidth() - 2 * kSideListInset, kRowInnerH };
}

juce::Rectangle<int> TrackListEditor::SideList::checkboxBounds(int row) const
{
    auto rowArea = rowBounds(row).reduced(8, 4);
    return rowArea.removeFromLeft(kCheckboxW);
}

juce::Rectangle<int> TrackListEditor::SideList::playBounds(int row) const
{
    auto rowArea = rowBounds(row).reduced(8, 4);
    if (owner.selectionMode)
        rowArea.removeFromLeft(kCheckboxW + 2);
    return rowArea.removeFromLeft(kPlayW);
}

juce::String TrackListEditor::SideList::rowTooltip(int row) const
{
    if (owner.controller == nullptr)
        return {};

    const auto& list = sideIndex == 0 ? owner.controller->sideA() : owner.controller->sideB();
    if (row < 0 || row >= static_cast<int>(list.size()))
        return {};

    const auto& track = list[static_cast<size_t>(row)];
    return "[" + juce::String(row + 1) + "] " + track.displayName + "\n"
           + FolderMixBuilder::formatDuration(track.durationSec) + "  |  "
           + track.file.getFileExtension().toUpperCase() + "\n"
           + track.file.getFullPathName();
}

bool TrackListEditor::SideList::isCheckboxHit(juce::Point<int> pos, int row) const
{
    if (!owner.selectionMode)
        return false;
    return checkboxBounds(row).contains(pos);
}

bool TrackListEditor::SideList::isPlayHit(juce::Point<int> pos, int row) const
{
    return playBounds(row).contains(pos);
}

bool TrackListEditor::SideList::isChecked(int row) const
{
    return row >= 0 && row < static_cast<int>(checkedRows.size()) && checkedRows[static_cast<size_t>(row)];
}

void TrackListEditor::SideList::toggleChecked(int row)
{
    if (!owner.selectionMode || row < 0 || row >= getTrackCount())
        return;

    if (static_cast<int>(checkedRows.size()) != getTrackCount())
        checkedRows.assign(static_cast<size_t>(getTrackCount()), false);

    checkedRows[static_cast<size_t>(row)] = !checkedRows[static_cast<size_t>(row)];
    owner.updateDeleteButtonState();
    repaint();
}

int TrackListEditor::SideList::getCheckedCount() const
{
    int count = 0;
    for (bool checked : checkedRows)
        if (checked)
            ++count;
    return count;
}

std::vector<int> TrackListEditor::SideList::getCheckedRows() const
{
    std::vector<int> rows;
    for (int row = 0; row < static_cast<int>(checkedRows.size()); ++row)
        if (checkedRows[static_cast<size_t>(row)])
            rows.push_back(row);
    return rows;
}

void TrackListEditor::SideList::clearChecked()
{
    checkedRows.clear();
    owner.updateDeleteButtonState();
    repaint();
}

void TrackListEditor::SideList::setRowChecked(int row, bool checked)
{
    if (row < 0 || row >= getTrackCount())
        return;

    if (static_cast<int>(checkedRows.size()) != getTrackCount())
        checkedRows.assign(static_cast<size_t>(getTrackCount()), false);

    checkedRows[static_cast<size_t>(row)] = checked;
    owner.updateDeleteButtonState();
    repaint();
}

void TrackListEditor::SideList::paintRow(juce::Graphics& g, int row, juce::Rectangle<int> area) const
{
    using namespace ui;

    if (owner.controller == nullptr)
        return;

    const auto& list = sideIndex == 0 ? owner.controller->sideA() : owner.controller->sideB();
    if (row < 0 || row >= static_cast<int>(list.size()))
        return;

    const auto& track = list[static_cast<size_t>(row)];
    const bool selected = row == selectedRow;
    const bool hover = row == hoverRow;
    const bool checked = isChecked(row);
    const bool sourceDrag = (dragging && row == dragSourceRow)
                            || (owner.dragActive && owner.dragSourceSide == sideIndex
                                && row == owner.dragSourceRow);
    const bool sameSideInsert = dragging && dropInsertRow == row && dragSourceRow != row;
    const bool crossSideInsert = owner.dragActive && owner.crossDropSide == sideIndex
                                 && owner.crossDropRow == row && owner.dragSourceSide != sideIndex;
    const bool insertBefore = sameSideInsert || crossSideInsert;
    const bool isCurrentTrack = owner.playingSide == sideIndex && owner.playingRow == row
                                  && owner.miniPlayerVisible;
    const bool isPlayingRow = isCurrentTrack && owner.previewPlayback.isPlaying();

    const bool highlighted = hover || checked || isCurrentTrack;
    const auto rowArea = area;
    g.setColour(sourceDrag ? Theme::card().withAlpha(0.4f)
                           : (highlighted ? Theme::sidebarHighlight() : Theme::card()));
    g.fillRoundedRectangle(rowArea.toFloat(), 4.0f);

    if (insertBefore)
    {
        g.setColour(Theme::accent());
        g.fillRect(rowArea.getX(), rowArea.getY() - 1, rowArea.getWidth(), 2);
    }

    auto content = rowArea.reduced(8, 4);
    if (owner.selectionMode)
    {
        drawCheckbox(g, content.removeFromLeft(kCheckboxW), checked, hover || selected);
        content.removeFromLeft(2);
    }
    const auto playArea = content.removeFromLeft(kPlayW);
    if (isPlayingRow)
        drawPauseIcon(g, playArea, Theme::accent());
    else
        drawPlayIcon(g, playArea, Theme::accent());

    auto indexArea = content.removeFromLeft(26);
    g.setColour(Theme::textMuted());
    g.setFont(Theme::metricFont());
    g.drawText("[" + juce::String(row + 1) + "]", indexArea, juce::Justification::centredLeft);

    auto durationArea = content.removeFromRight(52);
    g.setColour(Theme::textMuted());
    g.setFont(Theme::metricFont());
    g.drawText(FolderMixBuilder::formatDuration(track.durationSec), durationArea, juce::Justification::centredRight);

    g.setColour(Theme::textPrimary());
    g.setFont(Theme::bodyFont());
    g.drawText(track.displayName, content, juce::Justification::centredLeft, true);
}

void TrackListEditor::SideList::paint(juce::Graphics& g)
{
    using namespace ui;

    if (owner.dragActive && owner.crossDropSide == sideIndex)
    {
        g.setColour(Theme::accent().withAlpha(0.08f));
        g.fillRect(getLocalBounds());
        g.setColour(Theme::accent().withAlpha(0.35f));
        g.drawRect(getLocalBounds(), 1);
    }

    if (owner.controller == nullptr)
        return;

    const int count = getTrackCount();
    const int firstRow = juce::jmax(0, (g.getClipBounds().getY() - 4) / kRowH - 1);
    const int lastRow = juce::jmin(count - 1, firstRow + getHeight() / kRowH + 3);

    for (int row = firstRow; row <= lastRow; ++row)
        paintRow(g, row, rowBounds(row));

    const int endDropRow = (dragging && owner.dragSourceSide == sideIndex) ? dropInsertRow
                          : (owner.crossDropSide == sideIndex) ? owner.crossDropRow
                                                               : -1;
    if (owner.dragActive && endDropRow == count && count > 0)
    {
        const auto lineY = rowBounds(count - 1).getBottom() + 1;
        g.setColour(Theme::accent());
        g.fillRect(TrackListEditor::kSideListInset,
                   lineY,
                   getWidth() - 2 * TrackListEditor::kSideListInset,
                   2);
    }
}

void TrackListEditor::SideList::showContextMenu(int row)
{
    if (owner.controller == nullptr || row < 0)
        return;

    juce::PopupMenu menu;
    const int otherSide = sideIndex == 0 ? 1 : 0;
    menu.addItem(1, "Play");
    menu.addItem(4, isChecked(row) ? "Clear selection" : "Select for removal");
    menu.addSeparator();
    menu.addItem(2, "Move to Side " + juce::String(otherSide == 0 ? "A" : "B"));
    menu.addItem(3, "Remove track");

    menu.showMenuAsync(juce::PopupMenu::Options(), [this, row, otherSide](int result) {
        if (owner.controller == nullptr)
            return;

        if (result == 1)
            owner.playTrack(sideIndex, row);
        else if (result == 4)
        {
            owner.setSelectionMode(true);
            toggleChecked(row);
        }
        else if (result == 2)
        {
            if (owner.controller->moveToSide(sideIndex, row, otherSide, 9999))
            {
                owner.resetPlaybackAfterTrackLayoutChange();
                owner.layoutChanged();
            }
        }
        else if (result == 3)
        {
            owner.setSelectionMode(true);
            setRowChecked(row);
            owner.confirmAndDeleteSelected();
        }
    });
}

void TrackListEditor::SideList::mouseDown(const juce::MouseEvent& e)
{
    if (owner.controller == nullptr)
        return;

    if (getTrackCount() == 0)
        return;

    const int row = hitTestRow(e.getPosition());
    if (row < 0)
        return;

    if (e.mods.isPopupMenu())
    {
        showContextMenu(row);
        return;
    }

    selectedRow = row;
    pendingDragRow = row;
    pendingDrag = true;
    dragStartPos = e.getPosition();
}

void TrackListEditor::SideList::mouseDrag(const juce::MouseEvent& e)
{
    if (owner.controller == nullptr)
        return;

    if (pendingDrag && !dragging)
    {
        if (e.getPosition().getDistanceFrom(dragStartPos) < kDragThreshold)
            return;

        dragging = true;
        pendingDrag = false;
        dragSourceRow = pendingDragRow;
        dropInsertRow = dragSourceRow;
        owner.dragSourceSide = sideIndex;
        owner.dragSourceRow = dragSourceRow;
        owner.setDragActive(true);
    }

    if (!dragging)
        return;

    dropInsertRow = rowAtY(e.y);
    owner.crossDropSide = -1;
    owner.crossDropRow = -1;

    auto& other = sideIndex == 0 ? owner.sideBList : owner.sideAList;
    const auto otherPos = e.getEventRelativeTo(&other).getPosition();
    if (other.getLocalBounds().contains(otherPos))
    {
        const int otherSide = sideIndex == 0 ? 1 : 0;
        owner.crossDropSide = otherSide;
        owner.crossDropRow = other.rowAtY(otherPos.y);
        other.dropInsertRow = owner.crossDropRow;
    }
    else
    {
        other.dropInsertRow = -1;
    }

    const bool trashHover = owner.trashContains(e.getEventRelativeTo(&owner).getPosition());
    owner.trashZone.setActive(true, trashHover);
    repaint();
    other.repaint();
}

void TrackListEditor::SideList::mouseUp(const juce::MouseEvent& e)
{
    if (pendingDrag && !dragging)
    {
        pendingDrag = false;
        if (owner.controller != nullptr)
        {
            if (owner.selectionMode)
                toggleChecked(pendingDragRow);
            else
                owner.playTrack(sideIndex, pendingDragRow);
        }
        return;
    }

    if (!dragging || owner.controller == nullptr)
        return;

    bool changed = false;

    if (owner.trashContains(e.getEventRelativeTo(&owner).getPosition()))
    {
        owner.endDrag();
        owner.setSelectionMode(true);
        setRowChecked(dragSourceRow);
        owner.confirmAndDeleteSelected();
        refresh();
        return;
    }
    else if (getLocalBounds().contains(e.getPosition()))
    {
        int target = dropInsertRow;
        if (target > dragSourceRow)
            --target;
        target = juce::jlimit(0, getTrackCount() - 1, target);
        if (target != dragSourceRow)
            changed = owner.controller->reorderWithinSide(sideIndex, dragSourceRow, target);
    }
    else
    {
        auto& other = sideIndex == 0 ? owner.sideBList : owner.sideAList;
        const auto otherPos = e.getEventRelativeTo(&other).getPosition();
        if (other.getLocalBounds().contains(otherPos))
        {
            const int otherSide = sideIndex == 0 ? 1 : 0;
            const int insertRow = owner.crossDropSide == otherSide
                                      ? juce::jlimit(0, other.getTrackCount(), owner.crossDropRow)
                                      : juce::jlimit(0, other.getTrackCount(), other.rowAtY(otherPos.y));
            changed = owner.controller->moveToSide(sideIndex, dragSourceRow, otherSide, insertRow);
        }
    }

    owner.endDrag();
    if (changed)
    {
        owner.resetPlaybackAfterTrackLayoutChange();
        owner.layoutChanged();
    }
    refresh();
}

void TrackListEditor::SideList::mouseMove(const juce::MouseEvent& e)
{
    const int row = hitTestRow(e.getPosition());
    if (row != hoverRow)
    {
        hoverRow = row;
        repaint();
    }

    if (row >= 0)
        setTooltip(rowTooltip(row));
    else
        setTooltip({});
}

void TrackListEditor::SideList::mouseEnter(const juce::MouseEvent& e)
{
    juce::ignoreUnused(e);
    owner.updateSideScrollbars();
}

void TrackListEditor::SideList::mouseExit(const juce::MouseEvent& e)
{
    juce::ignoreUnused(e);
    hoverRow = -1;
    repaint();
    owner.updateSideScrollbars();
}

void TrackListEditor::SideList::refresh()
{
    const int count = getTrackCount();
    if (static_cast<int>(checkedRows.size()) != count)
        checkedRows.assign(static_cast<size_t>(count), false);

    if (count == 0)
        selectedRow = -1;
    else if (selectedRow >= 0)
        selectedRow = juce::jlimit(0, count - 1, selectedRow);

    const auto& viewport = sideIndex == 0 ? owner.sideAViewport : owner.sideBViewport;
    setSize(getWidth(), getContentHeight(viewport.getHeight()));
    owner.updateDeleteButtonState();
    repaint();
}

void TrackListEditor::SideList::setSelectedRow(int row)
{
    selectedRow = row;
    repaint();
}

void TrackListEditor::SideList::clearHover()
{
    if (hoverRow < 0)
        return;

    hoverRow = -1;
    repaint();
}

TrackListEditor::TrackListEditor()
    : sideAList(*this, 0),
      sideBList(*this, 1),
      miniPlayer(*this)
{
    using namespace ui;

    addAndMakeVisible(cassetteSelector);
    addAndMakeVisible(sideALabel);
    addAndMakeVisible(sideBLabel);
    addAndMakeVisible(sideAViewport);
    addAndMakeVisible(sideBViewport);
    addAndMakeVisible(selectTracksButton);
    addAndMakeVisible(rebalanceSidesButton);
    addAndMakeVisible(deleteSelectedButton);
    addChildComponent(miniPlayer);
    addChildComponent(trashZone);
    addChildComponent(loadingLabel);

    sideAViewport.setViewedComponent(&sideAList, false);
    sideAViewport.setScrollBarsShown(false, false);
    sideBViewport.setViewedComponent(&sideBList, false);
    sideBViewport.setScrollBarsShown(false, false);
    sideAViewport.addMouseListener(this, true);
    sideBViewport.addMouseListener(this, true);
    juce::Desktop::getInstance().addFocusChangeListener(this);

    cassetteSelector.addListener(this);
    Theme::styleCombo(cassetteSelector);
    Theme::applyLabel(sideALabel, Theme::sectionFont(), Theme::accent());
    Theme::applyLabel(sideBLabel, Theme::sectionFont(), Theme::accent());
    sideALabel.setBorderSize(juce::BorderSize<int>());
    sideBLabel.setBorderSize(juce::BorderSize<int>());
    Theme::applyLabel(loadingLabel, Theme::sectionFont(), Theme::textSecondary(), juce::Justification::centred);
    loadingLabel.setText(tr("track.loading"), juce::dontSendNotification);

    Theme::styleNeutralButton(selectTracksButton);
    Theme::styleNeutralButton(rebalanceSidesButton);
    Theme::styleNeutralButton(deleteSelectedButton);
    deleteSelectedButton.setVisible(false);
    deleteSelectedButton.setEnabled(false);

    selectTracksButton.onClick = [this] { setSelectionMode(!selectionMode); };

    rebalanceSidesButton.onClick = [this] { rebalanceSides(); };

    deleteSelectedButton.onClick = [this] { confirmAndDeleteSelected(); };

    setWantsKeyboardFocus(true);
}

TrackListEditor::~TrackListEditor()
{
    juce::Desktop::getInstance().removeFocusChangeListener(this);
    stopTimer();
    shutdownPreviewAudio();
}

void TrackListEditor::shutdownPreviewAudio()
{
    previewPlayback.shutdown();
    if (ownsAudioDevice)
    {
        ownedDeviceManager.closeAudioDevice();
        ownsAudioDevice = false;
    }
}

void TrackListEditor::attachToAudioDevice(juce::AudioDeviceManager& deviceManager)
{
    previewPlayback.attach(deviceManager);
}

void TrackListEditor::updateSideScrollbars()
{
    const auto showFor = [](juce::Viewport& viewport, juce::Component& list) {
        return viewport.isMouseOver(true) || list.hasKeyboardFocus(true) || viewport.hasKeyboardFocus(true);
    };

    sideAViewport.setScrollBarsShown(showFor(sideAViewport, sideAList), false);
    sideBViewport.setScrollBarsShown(showFor(sideBViewport, sideBList), false);
}

void TrackListEditor::globalFocusChanged(juce::Component* focusedComponent)
{
    juce::ignoreUnused(focusedComponent);
    updateSideScrollbars();
}

void TrackListEditor::mouseEnter(const juce::MouseEvent& e)
{
    juce::ignoreUnused(e);
    updateSideScrollbars();
}

void TrackListEditor::mouseExit(const juce::MouseEvent& e)
{
    juce::ignoreUnused(e);
    updateSideScrollbars();
}

void TrackListEditor::ensureAudioReady()
{
    if (previewPlayback.isAttached())
    {
        previewPlayback.prepareForOutput();
        return;
    }

    if (ownedDeviceManager.getCurrentAudioDevice() == nullptr)
    {
        ownedDeviceManager.initialiseWithDefaultDevices(0, 2);
        ownsAudioDevice = true;
    }

    previewPlayback.attach(ownedDeviceManager);
}

void TrackListEditor::setSelectionMode(bool on)
{
    if (selectionMode == on)
        return;

    selectionMode = on;
    if (!on)
    {
        sideAList.clearChecked();
        sideBList.clearChecked();
    }

    selectTracksButton.setButtonText(on ? tr("btn.done") : tr("btn.select_tracks"));
    deleteSelectedButton.setVisible(on);
    updateDeleteButtonState();
    sideAList.repaint();
    sideBList.repaint();
    resized();
}

void TrackListEditor::setLoading(bool loadingIn)
{
    if (loading == loadingIn)
        return;

    loading = loadingIn;
    loadingLabel.setVisible(loading);
    const bool showEditor = !loading;
    sideALabel.setVisible(showEditor);
    sideBLabel.setVisible(showEditor);
    sideAViewport.setVisible(showEditor);
    sideBViewport.setVisible(showEditor);
    selectTracksButton.setEnabled(showEditor);
    rebalanceSidesButton.setEnabled(showEditor && controller != nullptr
                                    && (!controller->sideA().empty() || !controller->sideB().empty()));
    deleteSelectedButton.setEnabled(showEditor && selectionMode && totalCheckedCount() > 0);
    cassetteSelector.setEnabled(showEditor);
    miniPlayer.setVisible(showEditor && miniPlayerVisible);
    resized();
    repaint();
}

void TrackListEditor::refreshLocalisedText()
{
    loadingLabel.setText(tr("track.loading"), juce::dontSendNotification);
    selectTracksButton.setButtonText(selectionMode ? tr("btn.done") : tr("btn.select_tracks"));
    rebalanceSidesButton.setButtonText(tr("btn.rebalance_sides"));
    updateDeleteButtonState();
    refresh();
}

void TrackListEditor::setController(MixtapeEditController* controllerIn)
{
    controller = controllerIn;
    refresh();
}

void TrackListEditor::setTapeSpec(const TapeLengthSpec& tapeIn)
{
    tape = tapeIn;
    refresh();
}

void TrackListEditor::rebalanceSides()
{
    if (controller == nullptr || loading)
        return;

    if (controller->sideA().empty() && controller->sideB().empty())
        return;

    resetPlaybackAfterTrackLayoutChange();
    controller->rebalance(tape);
    layoutChanged();
}

void TrackListEditor::setDragActive(bool active)
{
    dragActive = active;
    trashZone.setActive(active, false);
    resized();
}

void TrackListEditor::endDrag()
{
    dragActive = false;
    dragSourceSide = -1;
    dragSourceRow = -1;
    crossDropSide = -1;
    crossDropRow = -1;
    sideAList.pendingDrag = false;
    sideBList.pendingDrag = false;
    sideAList.dragging = false;
    sideBList.dragging = false;
    sideAList.dragSourceRow = -1;
    sideBList.dragSourceRow = -1;
    sideAList.dropInsertRow = -1;
    sideBList.dropInsertRow = -1;
    trashZone.setActive(false, false);
    resized();
    sideAList.repaint();
    sideBList.repaint();
}

bool TrackListEditor::trashContains(juce::Point<int> pos) const
{
    return trashZone.isVisible() && trashZone.getBounds().contains(pos);
}

void TrackListEditor::comboBoxChanged(juce::ComboBox* box)
{
    if (box != &cassetteSelector || controller == nullptr)
        return;

    controller->setActiveCassetteIndex(cassetteSelector.getSelectedItemIndex());
    layoutChanged();
}

juce::Colour TrackListEditor::sideAccentColour() const
{
    return ui::Theme::accent();
}

int TrackListEditor::totalCheckedCount() const
{
    return sideAList.getCheckedCount() + sideBList.getCheckedCount();
}

std::vector<std::pair<int, int>> TrackListEditor::collectCheckedTracks() const
{
    std::vector<std::pair<int, int>> items;
    for (int row : sideAList.getCheckedRows())
        items.push_back({ 0, row });
    for (int row : sideBList.getCheckedRows())
        items.push_back({ 1, row });
    return items;
}

void TrackListEditor::updateDeleteButtonState()
{
    const int count = totalCheckedCount();
    deleteSelectedButton.setEnabled(!loading && selectionMode && count > 0);
    deleteSelectedButton.setButtonText(count > 0 ? tr("btn.remove_selected") + " (" + juce::String(count) + ")"
                                                 : tr("btn.remove_selected"));
}

void TrackListEditor::confirmAndDeleteSelected()
{
    if (controller == nullptr || !selectionMode)
        return;

    const auto items = collectCheckedTracks();
    if (items.empty())
        return;

    const int count = static_cast<int>(items.size());
    ui::ConfirmDialogOptions options;
    options.title = tr("dialog.remove_tracks.title");
    options.message = trRemoveTracksMessage(count);
    options.confirmLabel = tr("dialog.remove_tracks.confirm");
    options.cancelLabel = tr("dialog.cancel");

    ui::showConfirmDialog(this, options, [this, items](bool confirmed) {
        if (!confirmed || controller == nullptr)
            return;

        if (controller->deleteTracks(items))
        {
            stopPlaybackIfTracksRemoved(items);
            sideAList.clearChecked();
            sideBList.clearChecked();
            sideAList.setSelectedRow(-1);
            sideBList.setSelectedRow(-1);
            layoutChanged();
            setSelectionMode(false);
        }
    });
}

void TrackListEditor::playTrack(int sideIndex, int row)
{
    if (controller == nullptr || row < 0)
        return;

    const auto& tracks = sideIndex == 0 ? controller->sideA() : controller->sideB();
    if (row >= static_cast<int>(tracks.size()))
        return;

    if (playingSide == sideIndex && playingRow == row && miniPlayerVisible)
    {
        if (previewPlayback.isPlaying())
            previewPlayback.pause();
        else
            previewPlayback.play();

        updateMiniPlayerUi();
        startTimerHz(8);
        sideAList.clearHover();
        sideBList.clearHover();
        sideAList.repaint();
        sideBList.repaint();
        return;
    }

    ensureAudioReady();

    const auto& track = tracks[static_cast<size_t>(row)];
    if (!previewPlayback.loadFile(track.file))
        return;

    playingSide = sideIndex;
    playingRow = row;
    sideAList.setSelectedRow(sideIndex == 0 ? row : -1);
    sideBList.setSelectedRow(sideIndex == 1 ? row : -1);
    miniPlayerVisible = true;
    miniPlayer.setVisible(true);
    previewPlayback.play();
    updateMiniPlayerUi();
    startTimerHz(8);
    sideAList.clearHover();
    sideBList.clearHover();
    resized();
    sideAList.repaint();
    sideBList.repaint();
}

void TrackListEditor::hideMiniPlayer()
{
    previewPlayback.stop();
    playingSide = -1;
    playingRow = -1;
    miniPlayerVisible = false;
    miniPlayerSeekDragging = false;
    resumePlaybackAfterSeek = false;
    miniPlayer.setVisible(false);
    stopTimer();
    sideAList.setSelectedRow(-1);
    sideBList.setSelectedRow(-1);
    resized();
    sideAList.repaint();
    sideBList.repaint();
}

void TrackListEditor::stopPlaybackIfTracksRemoved(const std::vector<std::pair<int, int>>& removed)
{
    if (!miniPlayerVisible || playingSide < 0 || playingRow < 0)
        return;

    for (const auto& [side, row] : removed)
    {
        if (side == playingSide && row == playingRow)
        {
            hideMiniPlayer();
            return;
        }
    }

    int removedBefore = 0;
    for (const auto& [side, row] : removed)
    {
        if (side == playingSide && row < playingRow)
            ++removedBefore;
    }

    if (removedBefore > 0)
        playingRow -= removedBefore;
}

void TrackListEditor::resetPlaybackAfterTrackLayoutChange()
{
    hideMiniPlayer();
    sideAList.setSelectedRow(-1);
    sideBList.setSelectedRow(-1);
    sideAList.clearHover();
    sideBList.clearHover();
}

void TrackListEditor::validatePlaybackState()
{
    if (!miniPlayerVisible || controller == nullptr || playingSide < 0 || playingRow < 0)
        return;

    const auto& tracks = playingSide == 0 ? controller->sideA() : controller->sideB();
    if (playingRow >= static_cast<int>(tracks.size()))
        hideMiniPlayer();
}

void TrackListEditor::updateMiniPlayerUi()
{
    const auto& tracks = controller != nullptr && playingSide == 0 ? controller->sideA()
                        : controller != nullptr && playingSide == 1 ? controller->sideB()
                                                                    : std::vector<FolderTrackInfo>{};

    juce::String title = previewPlayback.currentTrackName();
    if (playingRow >= 0 && playingRow < static_cast<int>(tracks.size()))
        title = tracks[static_cast<size_t>(playingRow)].displayName;

    const double lengthSec = previewPlayback.getLengthSec();
    const double positionSec = miniPlayerSeekDragging ? miniPlayer.seekSlider.getValue()
                                                      : previewPlayback.getPositionSec();

    miniPlayer.trackLabel.setText(title, juce::dontSendNotification);
    miniPlayer.timeLabel.setText(formatClock(positionSec) + " / " + formatClock(lengthSec),
                                 juce::dontSendNotification);
    miniPlayer.playPauseButton.setButtonText(previewPlayback.isPlaying() || resumePlaybackAfterSeek ? "||" : ">");

    if (!miniPlayerSeekDragging)
        miniPlayer.setSeekFromPlayback(positionSec, lengthSec);
}

void TrackListEditor::timerCallback()
{
    if (!miniPlayerVisible)
    {
        stopTimer();
        return;
    }

    updateMiniPlayerUi();
    sideAList.repaint();
    sideBList.repaint();

    if (!previewPlayback.isPlaying()
        && previewPlayback.getPositionSec() >= previewPlayback.getLengthSec() - 0.05)
    {
        return;
    }
}

void TrackListEditor::refresh()
{
    if (loading)
        return;

    if (controller != nullptr)
    {
        cassetteSelector.clear(juce::dontSendNotification);
        for (int i = 0; i < controller->getCassetteCount(); ++i)
            cassetteSelector.addItem("Cassette " + juce::String(i + 1), i + 1);
        cassetteSelector.setSelectedItemIndex(controller->getActiveCassetteIndex(), juce::dontSendNotification);
        cassetteSelector.setVisible(controller->getCassetteCount() > 1);
    }
    else
    {
        cassetteSelector.setVisible(false);
    }

    const auto accent = sideAccentColour();
    const auto sideHeader = [&](int sideIndex) {
        if (controller == nullptr)
            return sideIndex == 0 ? tr("track.side_a") : tr("track.side_b");

        const auto& tracks = sideIndex == 0 ? controller->sideA() : controller->sideB();
        const double used = FolderMixBuilder::sideDurationSec(tracks, controller->gapBetweenTracksSec());
        const double cap = tape.minutesPerSide * 60.0;
        const auto side = sideIndex == 0 ? tr("track.side_a") : tr("track.side_b");
        const bool overflow = used > cap + controller->gapBetweenTracksSec() * 2.0 + 1.0;
        return side + "  " + juce::String(tracks.size()) + " " + tr("track.tracks") + "  "
               + FolderMixBuilder::formatDuration(used) + " / " + FolderMixBuilder::formatDuration(cap)
               + (overflow ? "  " + tr("track.overflow") : "");
    };

    sideALabel.setText(sideHeader(0), juce::dontSendNotification);
    sideBLabel.setText(sideHeader(1), juce::dontSendNotification);
    sideALabel.setColour(juce::Label::textColourId, accent);
    sideBLabel.setColour(juce::Label::textColourId, accent);

    sideAList.refresh();
    sideBList.refresh();
    rebalanceSidesButton.setEnabled(!loading && controller != nullptr
                                    && (!controller->sideA().empty() || !controller->sideB().empty()));
    validatePlaybackState();
    resized();
}

void TrackListEditor::resized()
{
    auto area = getLocalBounds().reduced(8, 4);

    if (loading)
    {
        loadingLabel.setBounds(area);
        return;
    }

    if (dragActive)
    {
        trashZone.setBounds(area.removeFromBottom(kTrashH));
        area.removeFromBottom(6);
    }

    if (miniPlayerVisible)
    {
        miniPlayer.setBounds(area.removeFromBottom(kMiniPlayerH));
        area.removeFromBottom(4);
    }

    auto footer = area.removeFromBottom(kFooterH);
    selectTracksButton.setBounds(footer.removeFromLeft(130).reduced(0, 6));
    rebalanceSidesButton.setBounds(footer.removeFromLeft(156).reduced(0, 6));
    if (selectionMode)
        deleteSelectedButton.setBounds(footer.removeFromLeft(180).reduced(0, 6));

    if (cassetteSelector.isVisible())
    {
        auto toolbar = area.removeFromTop(kToolbarH);
        cassetteSelector.setBounds(toolbar.removeFromLeft(juce::jmin(180, toolbar.getWidth())).reduced(0, 3));
        area.removeFromTop(6);
    }

    auto header = area.removeFromTop(20);
    const int gap = 10;
    const int colW = (header.getWidth() - gap) / 2;
    sideALabel.setBounds(header.getX() + kSideHeaderInset,
                         header.getY(),
                         colW - kSideHeaderInset,
                         header.getHeight());
    sideBLabel.setBounds(header.getX() + colW + gap + kSideHeaderInset,
                         header.getY(),
                         colW - kSideHeaderInset,
                         header.getHeight());
    area.removeFromTop(4);

    const int listH = area.getHeight();
    sideAViewport.setBounds(area.getX(), area.getY(), colW, listH);
    sideBViewport.setBounds(area.getX() + colW + gap, area.getY(), colW, listH);
    sideAList.setSize(colW, sideAList.getContentHeight(listH));
    sideBList.setSize(colW, sideBList.getContentHeight(listH));
}

void TrackListEditor::paint(juce::Graphics& g)
{
    using namespace ui;

    Theme::drawCard(g, getLocalBounds(), juce::String());

    if (!loading)
        return;

    auto spinnerArea = getLocalBounds().withSizeKeepingCentre(220, 72);
    spinnerArea.translate(0, -12);
    loadingLabel.setBounds(spinnerArea);

    g.setColour(Theme::accent().withAlpha(0.18f));
    g.fillEllipse(spinnerArea.getCentreX() - 10.0f,
                  static_cast<float>(spinnerArea.getBottom() + 6),
                  20.0f,
                  20.0f);
    g.setColour(Theme::accent());
    g.drawEllipse(spinnerArea.getCentreX() - 10.0f,
                  static_cast<float>(spinnerArea.getBottom() + 6),
                  20.0f,
                  20.0f,
                  2.0f);
}

void TrackListEditor::layoutChanged()
{
    refresh();
    if (onLayoutChanged)
        onLayoutChanged();
}

void TrackListEditor::deleteSelectedTrack()
{
    if (!selectionMode)
        return;

    if (totalCheckedCount() > 0)
    {
        confirmAndDeleteSelected();
        return;
    }

    if (controller == nullptr)
        return;

    if (sideAList.getSelectedRow() >= 0)
    {
        sideAList.setRowChecked(sideAList.getSelectedRow());
        confirmAndDeleteSelected();
        return;
    }

    if (sideBList.getSelectedRow() >= 0)
    {
        sideBList.setRowChecked(sideBList.getSelectedRow());
        confirmAndDeleteSelected();
    }
}

void TrackListEditor::moveSelectedRow(int delta)
{
    if (controller == nullptr)
        return;

    if (sideAList.getSelectedRow() >= 0)
    {
        const int row = sideAList.getSelectedRow();
        const bool ok = delta < 0 ? controller->moveRowUp(0, row) : controller->moveRowDown(0, row);
        if (ok)
        {
            sideAList.setSelectedRow(row + delta);
            layoutChanged();
        }
        return;
    }

    if (sideBList.getSelectedRow() >= 0)
    {
        const int row = sideBList.getSelectedRow();
        const bool ok = delta < 0 ? controller->moveRowUp(1, row) : controller->moveRowDown(1, row);
        if (ok)
        {
            sideBList.setSelectedRow(row + delta);
            layoutChanged();
        }
    }
}

bool TrackListEditor::keyPressed(const juce::KeyPress& key)
{
    if (controller == nullptr)
        return false;

    if (key == juce::KeyPress::deleteKey || key == juce::KeyPress::backspaceKey)
    {
        deleteSelectedTrack();
        return true;
    }

    if (key.getModifiers().isCommandDown() && key.getModifiers().isShiftDown() && key.getTextCharacter() == 'z')
    {
        if (controller->getUndoManager().canRedo())
        {
            controller->getUndoManager().redo();
            layoutChanged();
            return true;
        }
    }

    if (key.getModifiers().isCommandDown() && key.getTextCharacter() == 'z')
    {
        if (controller->getUndoManager().canUndo())
        {
            controller->getUndoManager().undo();
            layoutChanged();
            return true;
        }
    }

    if (key.getModifiers().isAltDown() && key == juce::KeyPress::upKey)
    {
        moveSelectedRow(-1);
        return true;
    }

    if (key.getModifiers().isAltDown() && key == juce::KeyPress::downKey)
    {
        moveSelectedRow(1);
        return true;
    }

    return false;
}

} // namespace cassette
