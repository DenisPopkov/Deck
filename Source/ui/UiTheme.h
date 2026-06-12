#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace cassette::ui
{

struct Theme
{
    static inline juce::Colour background() { return juce::Colour(0xfff5f0e6); }
    static inline juce::Colour panel() { return juce::Colour(0xfffaf7f0); }
    static inline juce::Colour card() { return juce::Colour(0xffffffff); }
    static inline juce::Colour border() { return juce::Colour(0xff111111); }
    static inline juce::Colour borderLight() { return juce::Colour(0xffcccccc); }
    static inline juce::Colour accent() { return juce::Colour(0xffe85500); }
    static inline juce::Colour accentMuted() { return juce::Colour(0xffff7a33); }
    static inline juce::Colour prepare() { return accent(); }
    static inline juce::Colour exportGreen() { return juce::Colour(0xff2d6a4f); }
    static inline juce::Colour okGreen() { return juce::Colour(0xff3d8b5f); }
    static inline juce::Colour warnAmber() { return juce::Colour(0xffc45c00); }
    static inline juce::Colour failRed() { return juce::Colour(0xffc0392b); }
    static inline juce::Colour textPrimary() { return juce::Colour(0xff111111); }
    static inline juce::Colour textSecondary() { return juce::Colour(0xff444444); }
    static inline juce::Colour textMuted() { return juce::Colour(0xff666666); }
    static inline juce::Colour trackBlue() { return juce::Colour(0xff4a90d9); }
    static inline juce::Colour trackGreen() { return juce::Colour(0xff5cb88a); }
    static inline juce::Colour trackGrey() { return juce::Colour(0xff9aa0a6); }
    static inline juce::Colour sidebarHighlight() { return juce::Colour(0xffebe4d8); }
    static inline juce::Colour buttonDisabledFill() { return juce::Colour(0xffe8e3d9); }
    static inline juce::Colour buttonDisabledText() { return juce::Colour(0xff4a4a4a); }
    static inline juce::Colour buttonDisabledBorder() { return juce::Colour(0xffb8b2a8); }

    static inline juce::Colour cardBorder() { return borderLight(); }

    static juce::Font uiFont(float height, juce::Font::FontStyleFlags style = juce::Font::plain)
    {
#if JUCE_MAC
        juce::ignoreUnused(style);
        return juce::Font(juce::FontOptions("SF Pro Text", height, juce::Font::plain));
#else
        return juce::Font(juce::FontOptions(height, style));
#endif
    }

    static juce::Font monoFont(float height)
    {
#if JUCE_MAC
        return juce::Font(juce::FontOptions("SF Mono", height, juce::Font::plain));
#else
        return juce::Font(juce::FontOptions(juce::Font::getDefaultMonospacedFontName(), height, juce::Font::plain));
#endif
    }

    static juce::Font titleFont() { return uiFont(16.0f, juce::Font::bold).boldened(); }
    static juce::Font sectionFont() { return uiFont(11.5f, juce::Font::bold).boldened(); }
    static juce::Font bodyFont() { return uiFont(13.0f); }
    static juce::Font buttonFont() { return uiFont(13.0f); }
    static juce::Font buttonFontSmall() { return uiFont(11.0f); }
    static juce::Font captionFont() { return uiFont(11.0f); }
    static juce::Font metricFont() { return monoFont(12.0f); }
    static juce::Font scoreFont() { return uiFont(22.0f, juce::Font::bold).boldened(); }

    static void applyLabel(juce::Label& label,
                           const juce::Font& font,
                           juce::Colour colour,
                           juce::Justification justification = juce::Justification::centredLeft)
    {
        label.setFont(font);
        label.setColour(juce::Label::textColourId, colour);
        label.setJustificationType(justification);
        label.setMinimumHorizontalScale(1.0f);
    }

    static void styleBlackButton(juce::TextButton& b)
    {
        b.setColour(juce::TextButton::buttonColourId, border());
        b.setColour(juce::TextButton::buttonOnColourId, border().brighter(0.15f));
        b.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
        b.setColour(juce::TextButton::textColourOnId, juce::Colours::white);
    }

    static void styleNeutralButton(juce::TextButton& b)
    {
        b.setColour(juce::TextButton::buttonColourId, card());
        b.setColour(juce::TextButton::buttonOnColourId, sidebarHighlight());
        b.setColour(juce::TextButton::textColourOffId, textPrimary());
        b.setColour(juce::TextButton::textColourOnId, textPrimary());
    }

    static void styleAccentButton(juce::TextButton& b) { styleRecButton(b); }

    static void styleRecButton(juce::TextButton& b)
    {
        b.setColour(juce::TextButton::buttonColourId, accent());
        b.setColour(juce::TextButton::buttonOnColourId, accent().darker(0.12f));
        b.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
        b.setColour(juce::TextButton::textColourOnId, juce::Colours::white);
    }

    static void styleExportButton(juce::TextButton& b)
    {
        styleRecButton(b);
    }

    enum class TransportButtonStyle { Neutral, Black, Rec, Export };

    static void applyTransportButtonStyle(juce::TextButton& b, TransportButtonStyle style, bool enabled)
    {
        if (!enabled)
        {
            b.setColour(juce::TextButton::buttonColourId, buttonDisabledFill());
            b.setColour(juce::TextButton::buttonOnColourId, buttonDisabledFill());
            b.setColour(juce::TextButton::textColourOffId, buttonDisabledText());
            b.setColour(juce::TextButton::textColourOnId, buttonDisabledText());
            return;
        }

        switch (style)
        {
            case TransportButtonStyle::Black: styleBlackButton(b); break;
            case TransportButtonStyle::Rec: styleRecButton(b); break;
            case TransportButtonStyle::Export: styleExportButton(b); break;
            case TransportButtonStyle::Neutral:
            default: styleNeutralButton(b); break;
        }
    }

    static void styleTapeTypeSegment(juce::TextButton& b, bool selected, bool enabled = true)
    {
        if (!enabled)
        {
            b.setColour(juce::TextButton::buttonColourId, buttonDisabledFill());
            b.setColour(juce::TextButton::buttonOnColourId, buttonDisabledFill());
            b.setColour(juce::TextButton::textColourOffId, buttonDisabledText());
            b.setColour(juce::TextButton::textColourOnId, buttonDisabledText());
            return;
        }

        b.setColour(juce::TextButton::buttonColourId, selected ? accent() : card());
        b.setColour(juce::TextButton::buttonOnColourId, accent());
        b.setColour(juce::TextButton::textColourOffId, selected ? juce::Colours::white : textPrimary());
        b.setColour(juce::TextButton::textColourOnId, juce::Colours::white);
    }

    static void styleSegmentButton(juce::TextButton& b, bool selected)
    {
        styleTapeTypeSegment(b, selected);
    }

    static void styleCombo(juce::ComboBox& box)
    {
        box.setColour(juce::ComboBox::backgroundColourId, card());
        box.setColour(juce::ComboBox::textColourId, textPrimary());
        box.setColour(juce::ComboBox::outlineColourId, border());
        box.setColour(juce::ComboBox::arrowColourId, textSecondary());
        box.setColour(juce::PopupMenu::backgroundColourId, card());
        box.setColour(juce::PopupMenu::textColourId, textPrimary());
    }

    static void styleAccentSlider(juce::Slider& slider)
    {
        slider.setColour(juce::Slider::backgroundColourId, sidebarHighlight());
        slider.setColour(juce::Slider::trackColourId, accent());
        slider.setColour(juce::Slider::thumbColourId, accent().darker(0.15f));
        slider.setColour(juce::Slider::textBoxTextColourId, textPrimary());
        slider.setColour(juce::Slider::textBoxBackgroundColourId, card());
        slider.setColour(juce::Slider::textBoxOutlineColourId, border());
        slider.setColour(juce::Slider::textBoxHighlightColourId, card());
    }

    static void drawPanel(juce::Graphics& g, juce::Rectangle<int> bounds, bool fill = true)
    {
        if (fill)
        {
            g.setColour(card());
            g.fillRect(bounds);
        }
        g.setColour(border());
        g.drawRect(bounds, 1);
    }

    static void drawCard(juce::Graphics& g, juce::Rectangle<int> bounds, const juce::String& title)
    {
        drawPanel(g, bounds);

        if (title.isNotEmpty())
        {
            g.setColour(textPrimary());
            g.setFont(sectionFont());
            g.drawText(title, bounds.reduced(12, 10).removeFromTop(20), juce::Justification::centredLeft);
        }
    }

    static void drawSectionLabel(juce::Graphics& g, juce::Rectangle<int> area, const juce::String& text)
    {
        g.setColour(textSecondary());
        g.setFont(sectionFont());
        g.drawText(text, area, juce::Justification::centredLeft);
    }

    static void drawCentredText(juce::Graphics& g,
                                const juce::String& text,
                                juce::Rectangle<int> area,
                                const juce::Font& font,
                                juce::Colour colour)
    {
        g.setColour(colour);
        g.setFont(font);
        g.drawText(text, area, juce::Justification::centred);
    }
};

}
