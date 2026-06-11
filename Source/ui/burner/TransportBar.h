#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>

namespace cassette
{

class TransportBar : public juce::Component,
                       private juce::Button::Listener
{
public:
    std::function<void()> onPlay;
    std::function<void()> onPause;
    std::function<void()> onStop;
    std::function<void(bool)> onSideStopModeChanged;

    TransportBar();
    void setTimeDisplay(const juce::String& text);
    void setSideLabel(const juce::String& text);
    void setPlaying(bool playing);

private:
    void resized() override;
    void buttonClicked(juce::Button* b) override;

    juce::TextButton skipBackBtn { "<<<" };
    juce::TextButton playBtn { "Play" };
    juce::TextButton pauseBtn { "Pause" };
    juce::TextButton recBtn { "REC" };
    juce::Label nowPlaying { {}, "SIDE A" };
    juce::Label timeLabel { {}, "00:00 / 45:00" };
    juce::ToggleButton continuousBtn { "Continuous" };
    juce::ToggleButton sideStopBtn { "Side Stop" };
};

}
