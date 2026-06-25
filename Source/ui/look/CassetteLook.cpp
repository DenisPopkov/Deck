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
    setColour(juce::TooltipWindow::backgroundColourId, ui::Theme::card());
    setColour(juce::TooltipWindow::textColourId, ui::Theme::textSecondary());
    setColour(juce::TooltipWindow::outlineColourId, ui::Theme::border());
}

juce::Font CassetteLook::buttonFontForHeight(int buttonHeight, bool compact)
{
    if (compact || buttonHeight < 30)
        return ui::Theme::buttonFontSmall();
    return ui::Theme::buttonFont();
}

juce::Font CassetteLook::getTextButtonFont(juce::TextButton& button, int buttonHeight)
{
    const bool compactTapeSegment = button.getComponentID() == "tape-type-segment";
    return buttonFontForHeight(buttonHeight, compactTapeSegment);
}

juce::Font CassetteLook::getLabelFont(juce::Label& label)
{
    const auto id = label.getComponentID();
    if (id.startsWith("settings-"))
        return label.getFont();
    return ui::Theme::bodyFont();
}

void CassetteLook::drawLabel(juce::Graphics& g, juce::Label& label)
{
    const auto id = label.getComponentID();
    if (id.startsWith("settings-"))
    {
        g.fillAll(label.findColour(juce::Label::backgroundColourId));

        if (!label.isBeingEdited())
        {
            const float alpha = label.isEnabled() ? 1.0f : 0.5f;
            const auto font = label.getFont();
            g.setColour(label.findColour(juce::Label::textColourId).withMultipliedAlpha(alpha));
            g.setFont(font);

            auto textArea = label.getBorderSize().subtractedFrom(label.getLocalBounds());
            g.drawFittedText(label.getText(),
                             textArea,
                             label.getJustificationType(),
                             juce::jmax(1, (int) ((float) textArea.getHeight() / font.getHeight())),
                             label.getMinimumHorizontalScale());
        }

        return;
    }

    juce::LookAndFeel_V4::drawLabel(g, label);
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

void CassetteLook::drawToggleButton(juce::Graphics& g,
                                    juce::ToggleButton& button,
                                    bool shouldDrawButtonAsHighlighted,
                                    bool shouldDrawButtonAsDown)
{
    if (button.getComponentID() == "settings-checkbox")
    {
        const float tickWidth = juce::jmin(16.0f, (float) button.getHeight() - 4.0f);
        const float x = ((float) button.getWidth() - tickWidth) * 0.5f;
        const float y = ((float) button.getHeight() - tickWidth) * 0.5f;
        drawTickBox(g,
                    button,
                    x,
                    y,
                    tickWidth,
                    tickWidth,
                    button.getToggleState(),
                    button.isEnabled(),
                    shouldDrawButtonAsHighlighted,
                    shouldDrawButtonAsDown);
        return;
    }

    juce::LookAndFeel_V4::drawToggleButton(g, button, shouldDrawButtonAsHighlighted, shouldDrawButtonAsDown);
}

void CassetteLook::drawTooltip(juce::Graphics& g, const juce::String& text, int width, int height)
{
    const auto bounds = juce::Rectangle<int>(width, height);
    ui::Theme::drawPanel(g, bounds, true);

    g.setColour(ui::Theme::textSecondary());
    g.setFont(ui::Theme::captionFont());
    g.drawFittedText(text,
                     bounds.reduced(10, 8),
                     juce::Justification::topLeft,
                     8);
}

}
