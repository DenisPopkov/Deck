#include "TransportBar.h"
#include "../look/CassetteBurnerLook.h"

namespace cassette
{

TransportBar::TransportBar()
{
    for (auto* b : { &skipBackBtn, &playBtn, &pauseBtn })
        addAndMakeVisible(*b);
    addAndMakeVisible(recBtn);
    addAndMakeVisible(nowPlaying);
    addAndMakeVisible(timeLabel);
    addAndMakeVisible(continuousBtn);
    addAndMakeVisible(sideStopBtn);

    recBtn.setColour(juce::TextButton::buttonColourId, CassetteBurnerLook::accent());
    continuousBtn.setToggleState(true, juce::dontSendNotification);
    sideStopBtn.setToggleState(false, juce::dontSendNotification);

    skipBackBtn.addListener(this);
    playBtn.addListener(this);
    pauseBtn.addListener(this);

    nowPlaying.setColour(juce::Label::textColourId, CassetteBurnerLook::textPrimary());
    timeLabel.setColour(juce::Label::textColourId, CassetteBurnerLook::textPrimary().withAlpha(0.85f));
    timeLabel.setJustificationType(juce::Justification::centredRight);
}

void TransportBar::setTimeDisplay(const juce::String& text) { timeLabel.setText(text, juce::dontSendNotification); }

void TransportBar::setSideLabel(const juce::String& text) { nowPlaying.setText(text, juce::dontSendNotification); }

void TransportBar::setPlaying(bool playing)
{
    playBtn.setEnabled(!playing);
    pauseBtn.setEnabled(playing);
}

void TransportBar::resized()
{
    auto r = getLocalBounds().reduced(10, 6);
    skipBackBtn.setBounds(r.removeFromLeft(40));
    r.removeFromLeft(4);
    playBtn.setBounds(r.removeFromLeft(56));
    r.removeFromLeft(4);
    pauseBtn.setBounds(r.removeFromLeft(56));
    r.removeFromLeft(8);
    recBtn.setBounds(r.removeFromLeft(64));
    r.removeFromLeft(16);
    nowPlaying.setBounds(r.removeFromLeft(200));
    timeLabel.setBounds(r.removeFromRight(120));
    r.removeFromRight(12);
    sideStopBtn.setBounds(r.removeFromRight(90));
    continuousBtn.setBounds(r.removeFromRight(100));
}

void TransportBar::buttonClicked(juce::Button* b)
{
    if (b == &playBtn && onPlay)
        onPlay();
    if (b == &pauseBtn && onPause)
        onPause();
    if (b == &skipBackBtn && onStop)
        onStop();
}

}
