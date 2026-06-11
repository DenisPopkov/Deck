#pragma once

#include <juce_gui_extra/juce_gui_extra.h>
#include <juce_audio_devices/juce_audio_devices.h>
#include <optional>
#include "CompareWaveformDisplay.h"
#include "WizardStepStrip.h"
#include "DropHeroPanel.h"
#include "TapeSetupPanel.h"
#include "MixtapeBuilderPanel.h"
#include "AudioDropTarget.h"
#include "../analysis/AudioFeatures.h"
#include "../io/AudioFileLoader.h"
#include "../threading/AnalysisWorker.h"
#include "../dsp/CassetteProfile.h"
#include "../project/FolderMixBuilder.h"
#include "../audio/PreviewEngine.h"
#include "../analysis/PerceptualQualityGuard.h"

namespace cassette
{

class MainComponent : public juce::Component,
                      public juce::FileDragAndDropTarget,
                      private juce::Button::Listener,
                      private juce::Timer
{
public:
    MainComponent();
    ~MainComponent() override;

    void paint(juce::Graphics& g) override;
    void resized() override;
    bool keyPressed(const juce::KeyPress& key) override;

    bool isInterestedInAudioFileDrag(const juce::StringArray& files);
    bool isInterestedInDrop(const juce::StringArray& files) const;
    void handleAudioFilesDropped(const juce::StringArray& files, int x, int y);

private:
    bool isInterestedInFileDrag(const juce::StringArray& files) override;
    void fileDragEnter(const juce::StringArray& files, int x, int y) override;
    void fileDragExit(const juce::StringArray& files) override;
    void filesDropped(const juce::StringArray& files, int x, int y) override;
    void buttonClicked(juce::Button* button) override;
    void timerCallback() override;

    TapeSetupPanel& tapeSetup();
    const TapeSetupPanel& tapeSetup() const;

    void loadAudioFile(const juce::File& file);
    void startProcessing();
    void startFolderMixBuild();
    void finishProcessing(bool success, const juce::String& message);
    void syncPreviewBuffer(const std::optional<LoadedAudio>& audio);
    void scanMixFolder(const juce::File& folder);
    void refreshFolderFitLabel();
    void exportWav();
    void pickFolder();
    void stopPreview();
    void resetSession();
    void invalidatePreparedOutput();
    void promptChangeTapeType();

    MasteringOptions currentMasteringOptions() const;
    TapeFormulation getSelectedProfile() const;

    void setUiProcessing(bool processing);
    void setProgress(double value);
    void setStatus(const juce::String& text, juce::Colour colour = juce::Colours::white);
    void paintProgressBar(juce::Graphics& g, juce::Rectangle<int> area) const;
    void updateWizardState();
    void syncTransportButtonStyles();
    void updateReadySummary();
    void updateWaveformInfo(const AudioFeatures& source, const AudioFeatures* processed = nullptr);

    void showMixtapeSide(bool sideB);
    void updateDropHighlight(const juce::StringArray& files, bool active);
    DropPayloadKind dropPayloadKind(const juce::StringArray& files) const;

    juce::Label sessionLabel;

    TapeSetupPanel tapeSetupPanel;
    DropHeroPanel dropHero;
    WizardStepStrip wizardSteps;
    juce::Label readySummary { {}, "" };

    juce::TextButton newButton { "New" };
    juce::TextButton importButton { "Import audio" };
    juce::TextButton startButton { "Prepare" };
    juce::TextButton exportButton { "Export WAV" };
    juce::TextButton playBeforeButton { "Play Before" };
    juce::TextButton playAfterButton { "Play After" };

    CompareWaveformDisplay compareWaveform;
    MixtapeBuilderPanel mixtapePanel;

    juce::Label status;
    double progress = 0.0;
    juce::Rectangle<int> progressBounds;
    juce::Rectangle<int> leftSidebarBounds;
    juce::Rectangle<int> rightSidebarBounds;

    static constexpr int kLeftSidebarW = 208;
    static constexpr int kRightSidebarW = 0;

    bool mixtapeModeActive = false;
    bool hasSource = false;
    bool hasProcessed = false;

    juce::File loadedFile;
    std::optional<LoadedAudio> loadedAudio;
    std::optional<LoadedAudio> sourceAudio;
    std::optional<FolderScanResult> folderScan;
    std::optional<PerceptualQualityReport> lastQuality;
    std::optional<AudioFeatures> lastProcessedFeatures;
    std::optional<LoadedAudio> sideAAudio;
    std::optional<LoadedAudio> sideBAudio;
    std::optional<LoadedAudio> mixtapeReferenceAudio;
    juce::File sideAPath;
    juce::File sideBPath;
    double sideADurationSec = 0.0;
    double sideBDurationSec = 0.0;
    bool hasSideB = false;
    int mixtapeCassetteCount = 1;
    juce::String preparedTapeLabel;

    AnalysisWorker worker;
    std::atomic<bool> isProcessing { false };
    int windowDragDepth = 0;
    DropPayloadKind activeDropKind = DropPayloadKind::None;

    juce::AudioDeviceManager previewDeviceManager;
    juce::AudioSourcePlayer previewPlayer;
    PreviewEngine previewEngine;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};

}
