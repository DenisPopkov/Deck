#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>
#include "../dsp/MasteringOptions.h"
#include "../util/AppLocale.h"

namespace cassette
{

class ProcessingOptionsPanel : public juce::Component,
                               private juce::Button::Listener
{
public:
    std::function<void()> onChanged;

    ProcessingOptionsPanel();

    void setOptions(const MasteringOptions& options);
    MasteringOptions getOptions() const;

    void setDialogMode(bool dialogMode);
    void setInteractionEnabled(bool enabled);
    void refreshLocalisedText();

    int getPreferredHeight() const;

    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseMove(const juce::MouseEvent& e) override;
    void mouseExit(const juce::MouseEvent& e) override;

private:
    void buttonClicked(juce::Button* b) override;
    void notifyChanged();
    void layoutDialogRow(juce::Rectangle<int>& area, juce::ToggleButton& toggle);
    void toggleDialogRow(juce::ToggleButton& toggle);
    void updateDialogRowCursor(juce::Point<int> position);

    MasteringOptions options;
    bool dialogMode = false;
    juce::Label sectionLabel { {}, "Processing" };
    juce::String truePeakTitleText;
    juce::String truePeakBodyText;
    juce::String stereoTitleText;
    juce::String stereoBodyText;
    juce::Rectangle<int> truePeakTextArea;
    juce::Rectangle<int> stereoTextArea;
    juce::Rectangle<int> truePeakRowArea;
    juce::Rectangle<int> stereoRowArea;
    juce::ToggleButton truePeakLimiterToggle { "True peak limiter" };
    juce::ToggleButton stereoTighteningToggle { "Stereo tightening" };
};

}
