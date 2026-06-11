#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "TapeSetupPanel.h"
#include "ReadinessPanel.h"
#include "PreviewMonitorPanel.h"

namespace cassette
{

class MoreOptionsPanel : public juce::Component,
                         private juce::Button::Listener
{
public:
    std::function<void()> onDismiss;

    MoreOptionsPanel();

    TapeSetupPanel& tapeSetup();
    const TapeSetupPanel& tapeSetup() const;
    ReadinessPanel& readiness() { return readinessPanel; }
    PreviewMonitorPanel& preview() { return previewPanel; }

    void present();
    void dismiss();
    bool isShowingPanel() const { return showing; }

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    void buttonClicked(juce::Button* b) override;

    bool showing = false;
    juce::TextButton closeButton { "Done" };
    juce::Label title { {}, "More Options" };

    TapeSetupPanel tapePanel;
    ReadinessPanel readinessPanel;
    PreviewMonitorPanel previewPanel;
};

}
