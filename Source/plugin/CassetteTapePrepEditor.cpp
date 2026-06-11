#include "CassetteTapePrepEditor.h"
#include "../ui/UiTheme.h"

namespace cassette
{

CassetteTapePrepEditor::CassetteTapePrepEditor(CassetteTapePrepProcessor& processor)
    : juce::AudioProcessorEditor(processor),
      proc(processor)
{
    setSize(360, 220);

    addAndMakeVisible(title);
    addAndMakeVisible(hint);
    addAndMakeVisible(profileBox);
    addAndMakeVisible(hfSlider);
    addAndMakeVisible(cleanTransfer);
    addAndMakeVisible(bypass);

    title.setFont(ui::Theme::titleFont());
    title.setColour(juce::Label::textColourId, ui::Theme::textPrimary());
    hint.setFont(ui::Theme::captionFont());
    hint.setColour(juce::Label::textColourId, ui::Theme::textMuted());

    profileBox.addItemList({ "Type I", "Type II", "Type IV", "High-end Deck", "Walkman", "Cheap Portable" }, 1);
    ui::Theme::styleCombo(profileBox);

    hfSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    hfSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 18);
    hfSlider.setRange(0.0, 1.0, 0.01);
    hfSlider.setTextValueSuffix(" HF");

    profileAttachment = std::make_unique<ComboAttachment>(proc.getApvts(), "profile", profileBox);
    hfAttachment = std::make_unique<SliderAttachment>(proc.getApvts(), "hfSafety", hfSlider);
    cleanAttachment = std::make_unique<ButtonAttachment>(proc.getApvts(), "cleanTransfer", cleanTransfer);
    bypassAttachment = std::make_unique<ButtonAttachment>(proc.getApvts(), "bypass", bypass);
}

void CassetteTapePrepEditor::paint(juce::Graphics& g)
{
    g.fillAll(ui::Theme::background());
    ui::Theme::drawCard(g, getLocalBounds().reduced(8), {});
}

void CassetteTapePrepEditor::resized()
{
    auto r = getLocalBounds().reduced(16);
    title.setBounds(r.removeFromTop(24));
    r.removeFromTop(4);
    hint.setBounds(r.removeFromTop(32));
    r.removeFromTop(8);
    profileBox.setBounds(r.removeFromTop(28));
    r.removeFromTop(10);
    hfSlider.setBounds(r.removeFromTop(48));
    r.removeFromTop(8);
    cleanTransfer.setBounds(r.removeFromTop(24));
    bypass.setBounds(r.removeFromTop(24));
}

}
