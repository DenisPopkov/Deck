#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "AudioDropTarget.h"

namespace cassette
{

class DropHeroPanel : public juce::Component,
                      public AudioDropForwarder
{
public:
    std::function<void()> onChooseFolder;

    DropHeroPanel();

    void setDragHighlight(bool active, DropPayloadKind kind);
    void setInteractionEnabled(bool enabled);
    void refreshLocalisedText();
    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    juce::TextButton chooseFolderButton { "Choose Folder" };
    bool dragHighlight = false;
    DropPayloadKind dropKind = DropPayloadKind::None;
};

}
