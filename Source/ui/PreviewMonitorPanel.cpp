#include "PreviewMonitorPanel.h"
#include "UiTheme.h"

namespace cassette
{

PreviewMonitorPanel::PreviewMonitorPanel()
{
    addAndMakeVisible(collapseButton);
    collapseButton.setClickingTogglesState(false);
    collapseButton.addListener(this);
    ui::Theme::styleNeutralButton(collapseButton);

    addAndMakeVisible(subtitle);
    subtitle.setColour(juce::Label::textColourId, ui::Theme::textMuted());
    subtitle.setFont(ui::Theme::captionFont());

    for (auto* t : { &crossfeedToggle, &virtualSubToggle, &widenToggle })
    {
        addAndMakeVisible(*t);
        t->setToggleState(t == &crossfeedToggle || t == &virtualSubToggle, juce::dontSendNotification);
        t->addListener(this);
        t->setColour(juce::ToggleButton::textColourId, ui::Theme::textSecondary());
        t->setColour(juce::ToggleButton::tickColourId, ui::Theme::accent());
        t->setVisible(false);
    }

    for (auto* s : { &crossfeedSlider, &subSlider, &widenSlider })
    {
        addAndMakeVisible(*s);
        s->setSliderStyle(juce::Slider::LinearHorizontal);
        s->setTextBoxStyle(juce::Slider::TextBoxRight, false, 44, 18);
        s->setRange(0.0, 1.0, 0.01);
        s->addListener(this);
        s->setVisible(false);
    }
    crossfeedSlider.setValue(settings.crossfeedAmount, juce::dontSendNotification);
    subSlider.setValue(settings.virtualSubAmount, juce::dontSendNotification);
    widenSlider.setValue(settings.stereoWidenAmount, juce::dontSendNotification);

    for (auto* b : { &playButton, &pauseButton, &stopButton })
    {
        addAndMakeVisible(*b);
        b->addListener(this);
        ui::Theme::styleNeutralButton(*b);
        b->setVisible(false);
    }
}

void PreviewMonitorPanel::setCollapsed(bool shouldCollapse)
{
    collapsed = shouldCollapse;
    collapseButton.setButtonText(collapsed ? "Preview Effects  v" : "Preview Effects  ^");

    const bool show = !collapsed;
    subtitle.setVisible(show);
    crossfeedToggle.setVisible(show);
    virtualSubToggle.setVisible(show);
    widenToggle.setVisible(show);
    crossfeedSlider.setVisible(show);
    subSlider.setVisible(show);
    widenSlider.setVisible(show);
    playButton.setVisible(show);
    pauseButton.setVisible(show);
    stopButton.setVisible(show);

    resized();
    repaint();
}

void PreviewMonitorPanel::setTransportPlaying(bool playing)
{
    transportPlaying = playing;
    if (collapsed)
        return;
    playButton.setEnabled(!playing);
    pauseButton.setEnabled(playing);
}

void PreviewMonitorPanel::paint(juce::Graphics& g)
{
    ui::Theme::drawCard(g, getLocalBounds(), juce::String());
}

void PreviewMonitorPanel::pushSettings()
{
    settings.crossfeedEnabled = crossfeedToggle.getToggleState();
    settings.virtualSubEnabled = virtualSubToggle.getToggleState();
    settings.stereoWidenEnabled = widenToggle.getToggleState();
    settings.crossfeedAmount = static_cast<float>(crossfeedSlider.getValue());
    settings.virtualSubAmount = static_cast<float>(subSlider.getValue());
    settings.stereoWidenAmount = static_cast<float>(widenSlider.getValue());

    if (onSettingsChanged)
        onSettingsChanged(settings);
}

void PreviewMonitorPanel::resized()
{
    auto r = getLocalBounds().reduced(8);
    collapseButton.setBounds(r.removeFromTop(28));
    if (collapsed)
        return;

    subtitle.setBounds(r.removeFromTop(18));
    r.removeFromTop(4);

    auto transport = r.removeFromTop(28);
    playButton.setBounds(transport.removeFromLeft(56).reduced(2));
    pauseButton.setBounds(transport.removeFromLeft(56).reduced(2));
    stopButton.setBounds(transport.removeFromLeft(56).reduced(2));
    r.removeFromTop(6);

    auto row1 = r.removeFromTop(24);
    crossfeedToggle.setBounds(row1.removeFromLeft(110));
    crossfeedSlider.setBounds(row1);

    r.removeFromTop(4);
    auto row2 = r.removeFromTop(24);
    virtualSubToggle.setBounds(row2.removeFromLeft(110));
    subSlider.setBounds(row2);

    r.removeFromTop(4);
    auto row3 = r.removeFromTop(24);
    widenToggle.setBounds(row3.removeFromLeft(110));
    widenSlider.setBounds(row3);
}

void PreviewMonitorPanel::buttonClicked(juce::Button* b)
{
    if (b == &collapseButton)
    {
        setCollapsed(!collapsed);
        return;
    }

    if (b == &playButton && onPlay)
        onPlay();
    if (b == &pauseButton && onPause)
        onPause();
    if (b == &stopButton && onStop)
        onStop();

    if (b == &crossfeedToggle || b == &virtualSubToggle || b == &widenToggle)
        pushSettings();
}

void PreviewMonitorPanel::sliderValueChanged(juce::Slider*)
{
    pushSettings();
}

}
