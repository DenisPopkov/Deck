#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../io/PiTapeSettings.h"

namespace cassette
{

class PiTapeSettingsPanel : public juce::Component
{
public:
    PiTapeSettingsPanel();

    void setSettings(const PiTapeSettings& settings);
    PiTapeSettings getSettings() const;
    int getPreferredHeight() const;
    void refreshLocalisedText();

private:
    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseMove(const juce::MouseEvent& e) override;
    void mouseExit(const juce::MouseEvent& e) override;

    void layoutEnableRow(juce::Rectangle<int>& area);

    juce::ToggleButton enabledToggle;
    juce::Label hostLabel;
    juce::Label portLabel;
    juce::Label userLabel;
    juce::Label passLabel;
    juce::Label dirLabel;
    juce::Label controlPortLabel;
    juce::TextEditor hostEditor;
    juce::TextEditor portEditor;
    juce::TextEditor userEditor;
    juce::TextEditor passEditor;
    juce::TextEditor dirEditor;
    juce::TextEditor controlPortEditor;

    juce::String enableTitleText;
    juce::String enableBodyText;
    juce::Rectangle<int> enableRowArea;
    juce::Rectangle<int> enableTextArea;
};

} // namespace cassette
