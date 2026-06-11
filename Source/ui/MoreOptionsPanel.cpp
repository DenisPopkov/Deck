#include "MoreOptionsPanel.h"
#include "UiTheme.h"

namespace cassette
{

MoreOptionsPanel::MoreOptionsPanel()
{
    setVisible(false);
    addChildComponent(closeButton);
    addChildComponent(title);
    addChildComponent(tapePanel);
    addChildComponent(readinessPanel);
    addChildComponent(previewPanel);

    title.setColour(juce::Label::textColourId, ui::Theme::textPrimary());
    title.setFont(ui::Theme::titleFont());
    ui::Theme::styleAccentButton(closeButton);
    closeButton.addListener(this);

    previewPanel.setCollapsed(false);
}

TapeSetupPanel& MoreOptionsPanel::tapeSetup() { return tapePanel; }
const TapeSetupPanel& MoreOptionsPanel::tapeSetup() const { return tapePanel; }

void MoreOptionsPanel::present()
{
    showing = true;
    setVisible(true);
    closeButton.setVisible(true);
    title.setVisible(true);
    tapePanel.setVisible(true);
    readinessPanel.setVisible(true);
    previewPanel.setVisible(true);
    toFront(true);
    resized();
    repaint();
}

void MoreOptionsPanel::dismiss()
{
    showing = false;
    setVisible(false);
    if (onDismiss)
        onDismiss();
}

void MoreOptionsPanel::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colours::black.withAlpha(0.55f));

    auto card = getLocalBounds().withSizeKeepingCentre(juce::jmin(getWidth() - 40, 720),
                                                       juce::jmin(getHeight() - 40, 680));
    ui::Theme::drawCard(g, card, juce::String());
}

void MoreOptionsPanel::resized()
{
    if (!showing)
        return;

    auto card = getLocalBounds().withSizeKeepingCentre(juce::jmin(getWidth() - 40, 720),
                                                       juce::jmin(getHeight() - 40, 680));
    auto r = card.reduced(20);

    auto header = r.removeFromTop(36);
    title.setBounds(header.removeFromLeft(header.getWidth() - 90));
    closeButton.setBounds(header.removeFromRight(80).reduced(0, 4));

    r.removeFromTop(8);
    auto left = r.removeFromLeft(r.getWidth() / 2 - 6);
    tapePanel.setBounds(left.removeFromTop(340));
    left.removeFromTop(8);
    previewPanel.setBounds(left);

    r.removeFromLeft(12);
    readinessPanel.setBounds(r);
}

void MoreOptionsPanel::buttonClicked(juce::Button* b)
{
    if (b == &closeButton)
        dismiss();
}

}
