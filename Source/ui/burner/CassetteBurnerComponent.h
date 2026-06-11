#pragma once

#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "../../project/MixtapeProject.h"
#include "../../audio/PreviewEngine.h"
#include "../../threading/AnalysisWorker.h"
#include "ProjectSidebar.h"
#include "TransportBar.h"
#include "TapeTimelineView.h"
#include "TapeParameterPanel.h"
#include "StatusBar.h"

namespace cassette
{

class CassetteBurnerComponent : public juce::Component,
                                 private juce::AudioIODeviceCallback,
                                 private juce::Timer,
                                 private TapeTimelineView::Listener
{
public:
    CassetteBurnerComponent();
    ~CassetteBurnerComponent() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    void timelineProjectChanged() override;
    void timelineSelectionChanged(int sideIndex, int clipIndex) override;

    void audioDeviceIOCallbackWithContext(const float* const* inputChannelData,
                                          int numInputChannels,
                                          float* const* outputChannelData,
                                          int numOutputChannels,
                                          int numSamples,
                                          const juce::AudioIODeviceCallbackContext& context) override;
    void audioDeviceAboutToStart(juce::AudioIODevice* device) override;
    void audioDeviceStopped() override;

    void timerCallback() override;
    void refreshUiFromProject();
    void saveProject();
    void rebuildPreviewForActiveSide();
    void renderSide(bool sideB);

    MixtapeProject project;
    juce::File projectFile;

    ProjectSidebar sidebar;
    TransportBar transportBar;
    TapeTimelineView timeline;
    TapeParameterPanel params;
    StatusBar statusBar;

    juce::AudioDeviceManager deviceManager;
    juce::AudioSourcePlayer audioPlayer;
    PreviewEngine preview;

    AnalysisWorker worker;
    int previewSide = 0;
    bool previewReady = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CassetteBurnerComponent)
};

}
