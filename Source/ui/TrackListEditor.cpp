#include "TrackListEditor.h"
#include "ConfirmDialog.h"
#include "UiTheme.h"

namespace cassette
{

namespace
{

constexpr int kToolbarH = 30;
constexpr int kFooterH = 44;
constexpr int kTrashH = 48;

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

TrackListEditor::SideList::SideList(TrackListEditor& ownerIn, int sideIndexIn, juce::Colour accentIn)
    : owner(ownerIn), sideIndex(sideIndexIn), accent(accentIn)
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

int TrackListEditor::SideList::getContentHeight() const
{
    return juce::jmax(getHeight(), 8 + getTrackCount() * kRowH);
}

int TrackListEditor::SideList::rowAtY(int y) const
{
    const int count = getTrackCount();
    if (count <= 0)
        return 0;
    return juce::jlimit(0, count, (y - 4 + kRowH / 2) / kRowH);
}

juce::Rectangle<int> TrackListEditor::SideList::rowBounds(int row) const
{
    return { 4, 4 + row * kRowH, getWidth() - 8, kRowH - 2 };
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

bool TrackListEditor::SideList::isDeleteHit(juce::Point<int> pos, int row) const
{
    return rowBounds(row).withWidth(28).withX(rowBounds(row).getRight() - 28).contains(pos);
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
    const bool sourceDrag = dragging && row == dragSourceRow;
    const bool insertBefore = dragging && dropInsertRow == row && dragSourceRow != row;

    auto rowArea = area.reduced(0, 1);
    g.setColour(sourceDrag ? Theme::card().withAlpha(0.4f)
                           : (hover ? Theme::sidebarHighlight() : Theme::card()));
    g.fillRoundedRectangle(rowArea.toFloat(), 4.0f);

    if (selected)
    {
        g.setColour(Theme::border());
        g.drawRoundedRectangle(rowArea.toFloat(), 4.0f, 1.0f);
    }
    else
    {
        g.setColour(Theme::borderLight());
        g.drawRoundedRectangle(rowArea.toFloat(), 4.0f, 1.0f);
    }

    if (insertBefore)
    {
        g.setColour(Theme::accent());
        g.fillRect(rowArea.getX(), rowArea.getY() - 1, rowArea.getWidth(), 2);
    }

    auto content = rowArea.reduced(8, 4);
    auto indexArea = content.removeFromLeft(30);
    g.setFont(Theme::metricFont());
    g.drawText("[" + juce::String(row + 1) + "]", indexArea, juce::Justification::centredLeft);

    auto deleteArea = content.removeFromRight(22);
    g.setColour(Theme::failRed().withAlpha(hover || selected ? 0.95f : 0.55f));
    g.drawText("x", deleteArea, juce::Justification::centred);

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

    Theme::drawCard(g, getLocalBounds(), juce::String());

    if (owner.controller == nullptr)
        return;

    const int count = getTrackCount();
    const int firstRow = juce::jmax(0, (g.getClipBounds().getY() - 4) / kRowH - 1);
    const int lastRow = juce::jmin(count - 1, firstRow + getHeight() / kRowH + 3);

    for (int row = firstRow; row <= lastRow; ++row)
        paintRow(g, row, rowBounds(row));

    if (dragging && dropInsertRow == count)
    {
        const auto lineY = rowBounds(count - 1).getBottom() + 1;
        g.setColour(Theme::accent());
        g.fillRect(4, lineY, getWidth() - 8, 2);
    }

    if (count == 0)
    {
        g.setColour(Theme::textMuted());
        g.setFont(Theme::captionFont());
        g.drawText("Drop tracks here", getLocalBounds(), juce::Justification::centred);
    }
}

void TrackListEditor::SideList::showContextMenu(int row)
{
    if (owner.controller == nullptr || row < 0)
        return;

    juce::PopupMenu menu;
    const int otherSide = sideIndex == 0 ? 1 : 0;
    menu.addItem(1, "Move to Side " + juce::String(otherSide == 0 ? "A" : "B"));
    menu.addItem(2, "Delete track");

    menu.showMenuAsync(juce::PopupMenu::Options(), [this, row, otherSide](int result) {
        if (owner.controller == nullptr)
            return;

        if (result == 1)
        {
            if (owner.controller->moveToSide(sideIndex, row, otherSide, 9999))
                owner.layoutChanged();
        }
        else if (result == 2)
            owner.confirmAndDeleteTrack(sideIndex, row);
    });
}

void TrackListEditor::SideList::mouseDown(const juce::MouseEvent& e)
{
    if (owner.controller == nullptr)
        return;

    if (getTrackCount() == 0)
        return;

    const int row = juce::jlimit(0, getTrackCount() - 1, rowAtY(e.y));

    if (e.mods.isPopupMenu())
    {
        showContextMenu(row);
        return;
    }

    if (isDeleteHit(e.getPosition(), row))
    {
        owner.confirmAndDeleteTrack(sideIndex, row);
        return;
    }

    if (!rowBounds(row).contains(e.getPosition()))
        return;

    selectedRow = row;
    dragSourceRow = row;
    dropInsertRow = row;
    dragging = true;
    owner.setDragActive(true);
    repaint();
}

void TrackListEditor::SideList::mouseDrag(const juce::MouseEvent& e)
{
    if (!dragging || owner.controller == nullptr)
        return;

    dropInsertRow = rowAtY(e.y);

    auto& other = sideIndex == 0 ? owner.sideBList : owner.sideAList;
    const auto otherPos = e.getEventRelativeTo(&other).getPosition();
    if (other.getLocalBounds().contains(otherPos))
        other.dropInsertRow = other.rowAtY(otherPos.y);

    const bool trashHover = owner.trashContains(e.getEventRelativeTo(&owner).getPosition());
    owner.trashZone.setActive(true, trashHover);
    repaint();
    other.repaint();
}

void TrackListEditor::SideList::mouseUp(const juce::MouseEvent& e)
{
    if (!dragging || owner.controller == nullptr)
        return;

    bool changed = false;

    if (owner.trashContains(e.getEventRelativeTo(&owner).getPosition()))
    {
        owner.endDrag();
        owner.confirmAndDeleteTrack(sideIndex, dragSourceRow);
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
        {
            changed = owner.controller->reorderWithinSide(sideIndex, dragSourceRow, target);
            selectedRow = target;
        }
    }
    else
    {
        auto& other = sideIndex == 0 ? owner.sideBList : owner.sideAList;
        const auto otherPos = e.getEventRelativeTo(&other).getPosition();
        if (other.getLocalBounds().contains(otherPos))
        {
            const int otherSide = sideIndex == 0 ? 1 : 0;
            const int insertRow = juce::jlimit(0, other.getTrackCount(), other.rowAtY(otherPos.y));
            changed = owner.controller->moveToSide(sideIndex, dragSourceRow, otherSide, insertRow);
            other.setSelectedRow(juce::jlimit(0, juce::jmax(0, other.getTrackCount() - 1), insertRow));
            selectedRow = -1;
        }
    }

    owner.endDrag();
    if (changed)
        owner.layoutChanged();
    refresh();
}

void TrackListEditor::SideList::mouseMove(const juce::MouseEvent& e)
{
    const int row = getTrackCount() > 0 ? juce::jlimit(0, getTrackCount() - 1, rowAtY(e.y)) : -1;
    if (row != hoverRow)
    {
        hoverRow = row;
        repaint();
    }

    if (row >= 0)
        setTooltip(rowTooltip(row));
}

void TrackListEditor::SideList::mouseExit(const juce::MouseEvent& e)
{
    juce::ignoreUnused(e);
    hoverRow = -1;
    repaint();
}

void TrackListEditor::SideList::refresh()
{
    const int count = getTrackCount();
    if (count == 0)
        selectedRow = -1;
    else
        selectedRow = juce::jlimit(0, count - 1, selectedRow < 0 ? 0 : selectedRow);

    setSize(getWidth(), getContentHeight());
    repaint();
}

void TrackListEditor::SideList::setSelectedRow(int row)
{
    selectedRow = row;
    repaint();
}

TrackListEditor::TrackListEditor()
    : sideAList(*this, 0, ui::Theme::trackBlue()),
      sideBList(*this, 1, ui::Theme::trackGreen())
{
    using namespace ui;

    addAndMakeVisible(cassetteSelector);
    addAndMakeVisible(sideALabel);
    addAndMakeVisible(sideBLabel);
    addAndMakeVisible(sideAViewport);
    addAndMakeVisible(sideBViewport);
    addAndMakeVisible(rebalanceButton);
    addChildComponent(trashZone);
    addChildComponent(loadingLabel);

    sideAViewport.setViewedComponent(&sideAList, false);
    sideAViewport.setScrollBarsShown(true, false);
    sideBViewport.setViewedComponent(&sideBList, false);
    sideBViewport.setScrollBarsShown(true, false);

    cassetteSelector.addListener(this);
    Theme::applyLabel(sideALabel, Theme::sectionFont(), Theme::trackBlue());
    Theme::applyLabel(sideBLabel, Theme::sectionFont(), Theme::trackGreen());
    Theme::applyLabel(loadingLabel, Theme::sectionFont(), Theme::textSecondary(), juce::Justification::centred);
    loadingLabel.setText("Loading tracks...", juce::dontSendNotification);

    Theme::styleNeutralButton(rebalanceButton);

    rebalanceButton.onClick = [this] {
        if (onRebalanceRequested)
            onRebalanceRequested();
        else if (controller != nullptr)
        {
            controller->rebalance(tape);
            layoutChanged();
        }
    };

    setWantsKeyboardFocus(true);
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
    rebalanceButton.setEnabled(showEditor);
    cassetteSelector.setEnabled(showEditor);
    resized();
    repaint();
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

void TrackListEditor::setDragActive(bool active)
{
    dragActive = active;
    trashZone.setActive(active, false);
    resized();
}

void TrackListEditor::endDrag()
{
    dragActive = false;
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

void TrackListEditor::confirmAndDeleteTrack(int sideIndex, int row)
{
    if (controller == nullptr || row < 0)
        return;

    const auto& tracks = sideIndex == 0 ? controller->sideA() : controller->sideB();
    if (row >= static_cast<int>(tracks.size()))
        return;

    const auto& track = tracks[static_cast<size_t>(row)];
    ui::ConfirmDialogOptions options;
    options.title = "Remove track";
    options.message = "Remove \"" + track.displayName + "\" from the mixtape?\n"
                      "The file on disk will not be deleted.";
    options.confirmLabel = "Remove";
    options.cancelLabel = "Cancel";

    ui::showConfirmDialog(this, options, [this, sideIndex, row](bool confirmed) {
        if (!confirmed || controller == nullptr)
            return;

        if (controller->deleteTrack(sideIndex, row))
            layoutChanged();
    });
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

    const auto sideHeader = [&](int sideIndex) {
        if (controller == nullptr)
            return juce::String(sideIndex == 0 ? "Side A" : "Side B");

        const auto& tracks = sideIndex == 0 ? controller->sideA() : controller->sideB();
        const double used = FolderMixBuilder::sideDurationSec(tracks, controller->gapBetweenTracksSec());
        const double cap = tape.minutesPerSide * 60.0;
        const auto side = juce::String(sideIndex == 0 ? "Side A" : "Side B");
        const bool overflow = used > cap + controller->gapBetweenTracksSec() * 2.0 + 1.0;
        return side + "  " + juce::String(tracks.size()) + " tracks  "
               + FolderMixBuilder::formatDuration(used) + " / " + FolderMixBuilder::formatDuration(cap)
               + (overflow ? "  OVERFLOW" : "");
    };

    sideALabel.setText(sideHeader(0), juce::dontSendNotification);
    sideBLabel.setText(sideHeader(1), juce::dontSendNotification);
    sideALabel.setColour(juce::Label::textColourId,
                         sideALabel.getText().contains("OVERFLOW") ? ui::Theme::failRed() : ui::Theme::trackBlue());
    sideBLabel.setColour(juce::Label::textColourId,
                         sideBLabel.getText().contains("OVERFLOW") ? ui::Theme::failRed() : ui::Theme::trackGreen());

    sideAList.refresh();
    sideBList.refresh();
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

    auto footer = area.removeFromBottom(kFooterH);
    rebalanceButton.setBounds(footer.removeFromLeft(160).reduced(0, 6));

    if (cassetteSelector.isVisible())
    {
        auto toolbar = area.removeFromTop(kToolbarH);
        cassetteSelector.setBounds(toolbar.removeFromLeft(160));
        area.removeFromTop(4);
    }

    auto header = area.removeFromTop(20);
    const int gap = 10;
    const int colW = (header.getWidth() - gap) / 2;
    sideALabel.setBounds(header.getX(), header.getY(), colW, 18);
    sideBLabel.setBounds(header.getX() + colW + gap, header.getY(), colW, 18);
    area.removeFromTop(4);

    sideAViewport.setBounds(area.getX(), area.getY(), colW, area.getHeight());
    sideBViewport.setBounds(area.getX() + colW + gap, area.getY(), colW, area.getHeight());
    sideAList.setSize(colW - 4, sideAList.getContentHeight());
    sideBList.setSize(colW - 4, sideBList.getContentHeight());
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
    if (controller == nullptr)
        return;

    if (sideAList.getSelectedRow() >= 0)
    {
        confirmAndDeleteTrack(0, sideAList.getSelectedRow());
        return;
    }

    if (sideBList.getSelectedRow() >= 0)
        confirmAndDeleteTrack(1, sideBList.getSelectedRow());
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
