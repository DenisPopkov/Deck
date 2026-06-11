#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace cassette
{

class CassetteBurnerLook : public juce::LookAndFeel_V4
{
public:
    CassetteBurnerLook();

    static juce::Colour background() { return juce::Colour(0xff1a1a1e); }
    static juce::Colour panel() { return juce::Colour(0xff25252b); }
    static juce::Colour accent() { return juce::Colour(0xffff6b35); }
    static juce::Colour textPrimary() { return juce::Colour(0xffe5e5e5); }
    static juce::Colour gridLine() { return juce::Colour(0xff333338); }

    void drawButtonBackground(juce::Graphics& g,
                            juce::Button& button,
                            const juce::Colour& backgroundColour,
                            bool shouldDrawButtonAsHighlighted,
                            bool shouldDrawButtonAsDown) override;

    juce::Font getTextButtonFont(juce::TextButton&, int buttonHeight) override;
};

}
