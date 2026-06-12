#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../UiTheme.h"

namespace cassette
{

class CassetteBurnerLook : public juce::LookAndFeel_V4
{
public:
    CassetteBurnerLook();

    static juce::Colour background() { return ui::Theme::background(); }
    static juce::Colour panel() { return ui::Theme::panel(); }
    static juce::Colour accent() { return ui::Theme::accent(); }
    static juce::Colour textPrimary() { return ui::Theme::textPrimary(); }
    static juce::Colour gridLine() { return ui::Theme::borderLight(); }

    void drawButtonBackground(juce::Graphics& g,
                            juce::Button& button,
                            const juce::Colour& backgroundColour,
                            bool shouldDrawButtonAsHighlighted,
                            bool shouldDrawButtonAsDown) override;

    juce::Font getTextButtonFont(juce::TextButton&, int buttonHeight) override;
};

}
