#include "CassetteLook.h"
#include "../UiTheme.h"

namespace cassette
{

CassetteLook::CassetteLook() : juce::LookAndFeel_V4(getLightColourScheme())
{
    setColour(juce::ResizableWindow::backgroundColourId, ui::Theme::background());
    setColour(juce::TextButton::buttonColourId, ui::Theme::card());
    setColour(juce::TextButton::textColourOffId, ui::Theme::textPrimary());
    setColour(juce::ComboBox::backgroundColourId, ui::Theme::card());
    setColour(juce::ComboBox::outlineColourId, ui::Theme::border());
    setColour(juce::PopupMenu::backgroundColourId, ui::Theme::card());
    setColour(juce::PopupMenu::highlightedBackgroundColourId, ui::Theme::accent().withAlpha(0.14f));
    setColour(juce::PopupMenu::highlightedTextColourId, ui::Theme::accent());
    setColour(juce::Label::textColourId, ui::Theme::textPrimary());
}

juce::Font CassetteLook::buttonFontForHeight(int buttonHeight, bool compact)
{
    if (compact || buttonHeight < 30)
        return ui::Theme::buttonFontSmall();
    return ui::Theme::buttonFont();
}

juce::Font CassetteLook::getTextButtonFont(juce::TextButton& button, int buttonHeight)
{
    const auto text = button.getButtonText();
    const bool compactTapeSegment = text.startsWithIgnoreCase("Type I")
                                    || text.startsWithIgnoreCase("Type II")
                                    || text.startsWithIgnoreCase("Type IV");
    return buttonFontForHeight(buttonHeight, compactTapeSegment);
}

juce::Font CassetteLook::getLabelFont(juce::Label&)
{
    return ui::Theme::bodyFont();
}

juce::Font CassetteLook::getComboBoxFont(juce::ComboBox&)
{
    return ui::Theme::bodyFont();
}

juce::Font CassetteLook::getPopupMenuFont()
{
    return ui::Theme::bodyFont();
}

void CassetteLook::drawButtonBackground(juce::Graphics& g,
                                        juce::Button& button,
                                        const juce::Colour&,
                                        bool,
                                        bool down)
{
    auto bounds = button.getLocalBounds().toFloat();
    const auto base = button.findColour(juce::TextButton::buttonColourId);
    juce::Colour fill = base;

    if (!button.isEnabled())
        fill = ui::Theme::buttonDisabledFill();
    else if (down)
        fill = base.darker(0.08f);

    g.setColour(fill);
    g.fillRect(bounds);

    g.setColour(button.isEnabled() ? ui::Theme::border() : ui::Theme::buttonDisabledBorder());
    g.drawRect(bounds.reduced(0.5f), 1.0f);
}

void CassetteLook::drawButtonText(juce::Graphics& g,
                                  juce::TextButton& button,
                                  bool,
                                  bool)
{
    const auto font = getTextButtonFont(button, button.getHeight());
    g.setFont(font);

    const auto textColour = button.isEnabled()
                                ? button.findColour(button.getToggleState() ? juce::TextButton::textColourOnId
                                                                             : juce::TextButton::textColourOffId)
                                : ui::Theme::buttonDisabledText();

    g.setColour(textColour);

    auto textArea = button.getLocalBounds().reduced(4, 2);
    g.drawText(button.getButtonText(), textArea, juce::Justification::centred);
}

}
