#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>
#include "../audio/PerceptualPlaybackProcessor.h"

namespace cassette
{

class PreviewMonitorPanel : public juce::Component,
                            private juce::Button::Listener,
                            private juce::Slider::Listener
{
public:
    std::function<void(const PerceptualPlaybackSettings&)> onSettingsChanged;
    std::function<void()> onPlay;
    std::function<void()> onPause;
    std::function<void()> onStop;

    PreviewMonitorPanel();

    PerceptualPlaybackSettings currentSettings() const { return settings; }
    void setTransportPlaying(bool playing);
    bool isTransportPlaying() const { return transportPlaying; }
    void setCollapsed(bool collapsed);
    bool isCollapsed() const { return collapsed; }

private:
    bool transportPlaying = false;
    bool collapsed = true;

    void resized() override;
    void paint(juce::Graphics& g) override;
    void buttonClicked(juce::Button* b) override;
    void sliderValueChanged(juce::Slider* s) override;
    void pushSettings();

    PerceptualPlaybackSettings settings;

    juce::TextButton collapseButton { "Preview Effects  v" };
    juce::Label subtitle { {}, "Only for monitoring - not exported" };
    juce::ToggleButton crossfeedToggle { "Crossfeed" };
    juce::ToggleButton virtualSubToggle { "Virtual Sub" };
    juce::ToggleButton widenToggle { "Stereo Width" };
    juce::Slider crossfeedSlider;
    juce::Slider subSlider;
    juce::Slider widenSlider;
    juce::TextButton playButton { "Play" };
    juce::TextButton pauseButton { "Pause" };
    juce::TextButton stopButton { "Stop" };
};

}
