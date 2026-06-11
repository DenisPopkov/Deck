#include "CassetteBurnerLook.h"

namespace cassette
{

CassetteBurnerLook::CassetteBurnerLook() : juce::LookAndFeel_V4(getDarkColourScheme())
{
    setColour(juce::ResizableWindow::backgroundColourId, background());
    setColour(juce::TextButton::buttonColourId, panel());
    setColour(juce::TextButton::textColourOffId, textPrimary());
    setColour(juce::ComboBox::backgroundColourId, panel());
    setColour(juce::Slider::thumbColourId, accent());
    setColour(juce::Slider::trackColourId, gridLine());
}

void CassetteBurnerLook::drawButtonBackground(juce::Graphics& g,
                                              juce::Button& button,
                                              const juce::Colour&,
                                              bool highlighted,
                                              bool down)
{
    auto bounds = button.getLocalBounds().toFloat().reduced(1.0f);
    const auto base = button.getToggleState() ? accent() : panel().brighter(highlighted ? 0.12f : 0.0f);
    g.setColour(down ? base.darker(0.2f) : base);
    g.fillRoundedRectangle(bounds, 4.0f);
}

juce::Font CassetteBurnerLook::getTextButtonFont(juce::TextButton&, int buttonHeight)
{
    return juce::Font(juce::FontOptions(juce::jmax(12.0f, buttonHeight * 0.55f))).withExtraKerningFactor(0.02f);
}

}
