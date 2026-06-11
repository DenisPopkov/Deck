#pragma once

#include <juce_gui_extra/juce_gui_extra.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include "AudioDropTarget.h"
#include "UiTheme.h"

namespace cassette
{

struct WaveformCardInfo
{
    juce::String title { "Before" };
    juce::String subtitle;
    bool hasAudio = false;
};

class CompareWaveformDisplay : public juce::Component,
                               public AudioDropForwarder
{
public:
    CompareWaveformDisplay();

    void setBeforeBuffer(const juce::AudioBuffer<float>& buffer, double sampleRate);
    void setAfterBuffer(const juce::AudioBuffer<float>& buffer, double sampleRate);
    void clearAfter();
    void clearAll();

    void setBeforeInfo(const WaveformCardInfo& info);
    void setAfterInfo(const WaveformCardInfo& info);
    void setShowEmptyDropZone(bool show);
    void setDragHighlight(bool active, DropPayloadKind kind = DropPayloadKind::None);

    std::function<void(bool afterSide)> onPreviewSideClicked;

    void paint(juce::Graphics& g) override;
    void mouseDown(const juce::MouseEvent& e) override;

private:
    juce::AudioBuffer<float> before;
    juce::AudioBuffer<float> after;
    double sampleRate = 48000.0;
    bool hasAfter = false;
    bool hasBefore = false;
    bool showEmptyDropZone = true;
    bool dragHighlight = false;
    DropPayloadKind dropKind = DropPayloadKind::None;

    WaveformCardInfo beforeInfo;
    WaveformCardInfo afterInfo;

    void drawHalfWaveform(juce::Graphics& g,
                          juce::Rectangle<int> area,
                          const juce::AudioBuffer<float>& buffer,
                          juce::Colour colour,
                          const WaveformCardInfo& info,
                          bool processed) const;

    void drawEmptyDropZone(juce::Graphics& g, juce::Rectangle<int> area) const;
    void drawDragOverlay(juce::Graphics& g) const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CompareWaveformDisplay)
};

}
