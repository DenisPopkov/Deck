#pragma once

#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "../project/MixtapeEditController.h"
#include "../project/FolderMixBuilder.h"

namespace cassette
{

class TrackListEditor : public juce::Component,
                          private juce::ComboBox::Listener,
                          private juce::Timer,
                          private juce::FocusChangeListener
{
public:
    std::function<void()> onLayoutChanged;

    TrackListEditor();
    ~TrackListEditor() override;

    void setController(MixtapeEditController* controller);
    void setTapeSpec(const TapeLengthSpec& tape);
    void refresh();
    void setLoading(bool loading);
    void attachToAudioDevice(juce::AudioDeviceManager& deviceManager);
    void shutdownPreviewAudio();

    void setDragActive(bool active);
    bool isDragActive() const { return dragActive; }

    void paint(juce::Graphics& g) override;
    void resized() override;
    bool keyPressed(const juce::KeyPress& key) override;

private:
    friend class SideList;

    class TrackPreviewPlayback : public juce::AudioSource
    {
    public:
        void attach(juce::AudioDeviceManager& deviceManagerIn);
        void detach();
        void shutdown();

        bool loadFile(const juce::File& file);
        void play();
        void pause();
        void togglePlayPause();
        void stop();
        bool isPlaying() const { return transport.isPlaying(); }
        double getPositionSec() const;
        double getLengthSec() const;
        void setPositionSec(double sec);
        juce::String currentTrackName() const { return trackName; }
        bool isAttached() const { return deviceManager != nullptr; }

        void prepareToPlay(int samplesPerBlockExpected, double sampleRate) override;
        void releaseResources() override;
        void getNextAudioBlock(const juce::AudioSourceChannelInfo& bufferToFill) override;

    private:
        juce::AudioSourcePlayer player;
        std::unique_ptr<juce::AudioFormatReaderSource> readerSource;
        juce::AudioTransportSource transport;
        juce::AudioDeviceManager* deviceManager = nullptr;
        juce::String trackName;
    };

    class MiniPlayerBar : public juce::Component,
                          private juce::Slider::Listener
    {
    public:
        explicit MiniPlayerBar(TrackListEditor& ownerIn);

        void setVisible(bool shouldBeVisible) override;
        void paint(juce::Graphics& g) override;
        void resized() override;
        void setSeekFromPlayback(double positionSec, double lengthSec);

        juce::TextButton playPauseButton { ">" };
        juce::Label trackLabel;
        juce::Label timeLabel;
        juce::Slider seekSlider;
        juce::TextButton closeButton { "X" };

    private:
        void sliderValueChanged(juce::Slider* slider) override;
        void sliderDragStarted(juce::Slider* slider) override;
        void sliderDragEnded(juce::Slider* slider) override;

        TrackListEditor& owner;
        bool seekFromPlayback = false;
    };

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
        SideList(TrackListEditor& ownerIn, int sideIndexIn);

        void refresh();
        void setSelectedRow(int row);
        void clearHover();
        int getSelectedRow() const { return selectedRow; }
        int rowAtY(int y) const;
        int getContentHeight() const;
        int getCheckedCount() const;
        std::vector<int> getCheckedRows() const;
        void clearChecked();
        void setRowChecked(int row, bool checked = true);

        void paint(juce::Graphics& g) override;
        void mouseDown(const juce::MouseEvent& e) override;
        void mouseDrag(const juce::MouseEvent& e) override;
        void mouseUp(const juce::MouseEvent& e) override;
        void mouseMove(const juce::MouseEvent& e) override;
        void mouseEnter(const juce::MouseEvent& e) override;
        void mouseExit(const juce::MouseEvent& e) override;

        int dragSourceRow = -1;
        int dropInsertRow = -1;
        bool dragging = false;
        bool pendingDrag = false;
        int pendingDragRow = -1;
        juce::Point<int> dragStartPos;

        static constexpr int kDragThreshold = 6;

    private:
        int getTrackCount() const;
        juce::Rectangle<int> rowBounds(int row) const;
        juce::Rectangle<int> checkboxBounds(int row) const;
        juce::Rectangle<int> playBounds(int row) const;
        int hitTestRow(juce::Point<int> pos) const;
        void paintRow(juce::Graphics& g, int row, juce::Rectangle<int> area) const;
        juce::String rowTooltip(int row) const;
        bool isCheckboxHit(juce::Point<int> pos, int row) const;
        bool isPlayHit(juce::Point<int> pos, int row) const;
        void toggleChecked(int row);
        bool isChecked(int row) const;
        void showContextMenu(int row);

        TrackListEditor& owner;
        int sideIndex = 0;
        int selectedRow = -1;
        int hoverRow = -1;
        std::vector<bool> checkedRows;
        static constexpr int kRowH = 32;
        static constexpr int kCheckboxW = 20;
        static constexpr int kPlayW = 24;
    };

    void ensureAudioReady();
    void layoutChanged();
    void confirmAndDeleteSelected();
    void deleteSelectedTrack();
    void moveSelectedRow(int delta);
    void endDrag();
    bool trashContains(juce::Point<int> pos) const;
    void comboBoxChanged(juce::ComboBox* box) override;
    void timerCallback() override;
    void globalFocusChanged(juce::Component* focusedComponent) override;
    void mouseEnter(const juce::MouseEvent& e) override;
    void mouseExit(const juce::MouseEvent& e) override;
    void updateSideScrollbars();
    void setSelectionMode(bool on);
    void playTrack(int sideIndex, int row);
    void hideMiniPlayer();
    void updateMiniPlayerUi();
    void stopPlaybackIfTracksRemoved(const std::vector<std::pair<int, int>>& removed);
    void validatePlaybackState();
    void resetPlaybackAfterTrackLayoutChange();
    void updateDeleteButtonState();
    int totalCheckedCount() const;
    std::vector<std::pair<int, int>> collectCheckedTracks() const;
    juce::Colour sideAccentColour() const;

    MixtapeEditController* controller = nullptr;
    TapeLengthSpec tape;
    bool dragActive = false;
    int dragSourceSide = -1;
    int dragSourceRow = -1;
    int crossDropSide = -1;
    int crossDropRow = -1;
    bool loading = false;
    bool ownsAudioDevice = false;
    bool selectionMode = false;

    juce::Label loadingLabel;
    juce::Label sideALabel;
    juce::Label sideBLabel;
    juce::ComboBox cassetteSelector;
    SideList sideAList;
    SideList sideBList;
    juce::Viewport sideAViewport;
    juce::Viewport sideBViewport;
    juce::TextButton selectTracksButton { "Select tracks" };
    juce::TextButton deleteSelectedButton { "Remove selected" };
    TrashZone trashZone;
    MiniPlayerBar miniPlayer;
    TrackPreviewPlayback previewPlayback;
    juce::AudioDeviceManager ownedDeviceManager;

    int playingSide = -1;
    int playingRow = -1;
    bool miniPlayerVisible = false;
    bool miniPlayerSeekDragging = false;
    bool resumePlaybackAfterSeek = false;

    static constexpr int kToolbarH = 30;
    static constexpr int kFooterH = 44;
    static constexpr int kMiniPlayerH = 56;
    static constexpr int kTrashH = 48;
    static constexpr int kSideListInset = 4;
    static constexpr int kRowContentInset = 8;
    static constexpr int kPlayIconLeftOffset = 8; // kPlayW / 2 - 4
    static constexpr int kSideHeaderInset = kSideListInset + kRowContentInset + kPlayIconLeftOffset;
};

} // namespace cassette
