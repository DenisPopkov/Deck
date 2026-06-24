#include "ProcessingOptionsPanel.h"
#include "UiTheme.h"

namespace cassette
{

ProcessingOptionsPanel::ProcessingOptionsPanel()
{
    addAndMakeVisible(sectionLabel);
    ui::Theme::applyLabel(sectionLabel, ui::Theme::captionFont(), ui::Theme::textSecondary());

    for (auto* t : { &truePeakLimiterToggle, &stereoTighteningToggle })
    {
        addAndMakeVisible(*t);
        t->setToggleState(true, juce::dontSendNotification);
        t->setColour(juce::ToggleButton::textColourId, ui::Theme::textSecondary());
        t->setColour(juce::ToggleButton::tickColourId, ui::Theme::accent());
        t->addListener(this);
    }

    truePeakLimiterToggle.setTooltip(
        "Caps inter-sample peaks in the exported file. Turn off if you plan to drive saturation on the deck input.");
    stereoTighteningToggle.setTooltip(
        "Keeps bass centered and tames wide highs for tape. Turn off for 3D effects, wide stereo, and spinning pans.");
}

void ProcessingOptionsPanel::setOptions(const MasteringOptions& newOptions)
{
    options = newOptions;
    truePeakLimiterToggle.setToggleState(options.enableTruePeakLimiter, juce::dontSendNotification);
    stereoTighteningToggle.setToggleState(options.enableStereoTightening, juce::dontSendNotification);
}

MasteringOptions ProcessingOptionsPanel::getOptions() const
{
    auto out = options;
    out.enableTruePeakLimiter = truePeakLimiterToggle.getToggleState();
    out.enableStereoTightening = stereoTighteningToggle.getToggleState();
    return out;
}

void ProcessingOptionsPanel::setInteractionEnabled(bool enabled)
{
    truePeakLimiterToggle.setEnabled(enabled);
    stereoTighteningToggle.setEnabled(enabled);
}

void ProcessingOptionsPanel::paint(juce::Graphics& g)
{
    juce::ignoreUnused(g);
}

void ProcessingOptionsPanel::resized()
{
    auto r = getLocalBounds();
    sectionLabel.setBounds(r.removeFromTop(18));
    r.removeFromTop(4);
    truePeakLimiterToggle.setBounds(r.removeFromTop(22));
    r.removeFromTop(2);
    stereoTighteningToggle.setBounds(r.removeFromTop(22));
}

void ProcessingOptionsPanel::buttonClicked(juce::Button* b)
{
    if (b == &truePeakLimiterToggle || b == &stereoTighteningToggle)
        notifyChanged();
}

void ProcessingOptionsPanel::notifyChanged()
{
    options = getOptions();
    if (onChanged)
        onChanged();
}

}
