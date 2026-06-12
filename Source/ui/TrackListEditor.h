#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../project/MixtapeEditController.h"
#include "../project/FolderMixBuilder.h"

namespace cassette
{

class TrackListEditor : public juce::Component,
                          private juce::ComboBox::Listener
{
public:
    std::function<void()> onLayoutChanged;
    std::function<void()> onRebalanceRequested;

    TrackListEditor();

    void setController(MixtapeEditController* controller);
    void setTapeSpec(const TapeLengthSpec& tape);
    void refresh();
    void setLoading(bool loading);

    void setDragActive(bool active);
    bool isDragActive() const { return dragActive; }

    void paint(juce::Graphics& g) override;
    void resized() override;
    bool keyPressed(const juce::KeyPress& key) override;

private:
    friend class SideList;

    class TrashZone : public juce::Component
    {
    public:
        void setActive(bool activeIn, bool hoverIn);
        void paint(juce::Graphics& g) override;

    private:
        bool active = false;
        bool hover = false;
    };

    class SideList : public juce::Component,
                     public juce::SettableTooltipClient
    {
    public:
        SideList(TrackListEditor& ownerIn, int sideIndexIn, juce::Colour accentIn);

        void refresh();
        void setSelectedRow(int row);
        int getSelectedRow() const { return selectedRow; }
        int rowAtY(int y) const;
        int getContentHeight() const;

        void paint(juce::Graphics& g) override;
        void mouseDown(const juce::MouseEvent& e) override;
        void mouseDrag(const juce::MouseEvent& e) override;
        void mouseUp(const juce::MouseEvent& e) override;
        void mouseMove(const juce::MouseEvent& e) override;
        void mouseExit(const juce::MouseEvent& e) override;

        int dragSourceRow = -1;
        int dropInsertRow = -1;
        bool dragging = false;

    private:
        int getTrackCount() const;
        juce::Rectangle<int> rowBounds(int row) const;
        void paintRow(juce::Graphics& g, int row, juce::Rectangle<int> area) const;
        juce::String rowTooltip(int row) const;
        bool isDeleteHit(juce::Point<int> pos, int row) const;
        void showContextMenu(int row);

        TrackListEditor& owner;
        int sideIndex = 0;
        juce::Colour accent;
        int selectedRow = -1;
        int hoverRow = -1;
        static constexpr int kRowH = 32;
    };

    void layoutChanged();
    void confirmAndDeleteTrack(int sideIndex, int row);
    void deleteSelectedTrack();
    void moveSelectedRow(int delta);
    void endDrag();
    bool trashContains(juce::Point<int> pos) const;
    void comboBoxChanged(juce::ComboBox* box) override;

    MixtapeEditController* controller = nullptr;
    TapeLengthSpec tape;
    bool dragActive = false;
    bool loading = false;

    juce::Label loadingLabel;
    juce::Label sideALabel;
    juce::Label sideBLabel;
    juce::ComboBox cassetteSelector;
    SideList sideAList;
    SideList sideBList;
    juce::Viewport sideAViewport;
    juce::Viewport sideBViewport;
    juce::TextButton rebalanceButton { "Rebalance sides" };
    TrashZone trashZone;
};

} // namespace cassette
