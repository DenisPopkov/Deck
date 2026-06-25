#include "DropHeroPanel.h"
#include "UiTheme.h"
#include "../util/AppLocale.h"

namespace cassette
{

DropHeroPanel::DropHeroPanel()
{
    initAudioDropForwarding(this);
    addAndMakeVisible(chooseFolderButton);
    ui::Theme::styleBlackButton(chooseFolderButton);
    chooseFolderButton.setButtonText(tr("btn.import_folder"));
    chooseFolderButton.onClick = [this] {
        if (onChooseFolder)
            onChooseFolder();
    };

    onDragHoverChanged = [this](DropPayloadKind kind) {
        setDragHighlight(kind != DropPayloadKind::None, kind);
    };
}

void DropHeroPanel::setDragHighlight(bool active, DropPayloadKind kind)
{
    dragHighlight = active;
    dropKind = kind;
    repaint();
}

void DropHeroPanel::refreshLocalisedText()
{
    chooseFolderButton.setButtonText(tr("btn.import_folder"));
    repaint();
}

void DropHeroPanel::setInteractionEnabled(bool enabled)
{
    chooseFolderButton.setEnabled(enabled);
}

namespace
{

void drawDashedRect(juce::Graphics& g, juce::Rectangle<int> bounds, juce::Colour colour, float dash = 7.0f)
{
    const float dashes[] = { dash, dash };
    const auto r = bounds.toFloat().reduced(0.5f);
    g.setColour(colour);
    g.drawDashedLine({ r.getX(), r.getY(), r.getRight(), r.getY() }, dashes, 2, 1.0f);
    g.drawDashedLine({ r.getRight(), r.getY(), r.getRight(), r.getBottom() }, dashes, 2, 1.0f);
    g.drawDashedLine({ r.getRight(), r.getBottom(), r.getX(), r.getBottom() }, dashes, 2, 1.0f);
    g.drawDashedLine({ r.getX(), r.getBottom(), r.getX(), r.getY() }, dashes, 2, 1.0f);
}

}

void DropHeroPanel::paint(juce::Graphics& g)
{
    using namespace ui;

    auto r = getLocalBounds();

    g.setColour(Theme::card());
    g.fillRect(r);

    if (dragHighlight)
    {
        g.setColour(Theme::accent().withAlpha(0.10f));
        g.fillRect(r.reduced(2));
        g.setColour(Theme::accent());
        g.drawRect(r.reduced(2), 2);
    }
    else
    {
        drawDashedRect(g, r.reduced(1), Theme::borderLight());
    }

    auto textArea = getLocalBounds().reduced(24).withTrimmedBottom(56);
    g.setColour(Theme::textPrimary());
    g.setFont(Theme::bodyFont().boldened());
    g.drawFittedText(dragHighlight ? (dropKind == DropPayloadKind::Folder ? tr("drop.folder")
                                                                        : tr("drop.audio"))
                                 : tr("drop.music"),
                     textArea,
                     juce::Justification::centred,
                     1);
}

void DropHeroPanel::resized()
{
    auto r = getLocalBounds().reduced(24);
    chooseFolderButton.setBounds(r.removeFromBottom(36).withSizeKeepingCentre(172, 36));
}

}
