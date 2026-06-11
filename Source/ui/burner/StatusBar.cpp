#include "StatusBar.h"
#include "../look/CassetteBurnerLook.h"

namespace cassette
{

StatusBar::StatusBar()
{
    addAndMakeVisible(messageLabel);
    addAndMakeVisible(cpuLabel);
    addAndMakeVisible(srLabel);
    addAndMakeVisible(tapeHissToggle);
    messageLabel.setColour(juce::Label::textColourId, CassetteBurnerLook::textPrimary());
    cpuLabel.setColour(juce::Label::textColourId, CassetteBurnerLook::textPrimary().withAlpha(0.7f));
    srLabel.setColour(juce::Label::textColourId, CassetteBurnerLook::textPrimary().withAlpha(0.7f));
}

void StatusBar::paint(juce::Graphics& g)
{
    g.fillAll(CassetteBurnerLook::panel().darker(0.15f));
    g.setColour(CassetteBurnerLook::gridLine());
    g.drawHorizontalLine(0, 0.0f, static_cast<float>(getWidth()));
}

void StatusBar::resized()
{
    auto r = getLocalBounds().reduced(8, 4);
    tapeHissToggle.setBounds(r.removeFromRight(100));
    srLabel.setBounds(r.removeFromRight(110));
    cpuLabel.setBounds(r.removeFromRight(70));
    messageLabel.setBounds(r);
}

void StatusBar::setMessage(const juce::String& text) { messageLabel.setText(text, juce::dontSendNotification); }

}
