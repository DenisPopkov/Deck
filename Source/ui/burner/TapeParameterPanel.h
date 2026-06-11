#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>
#include "../../project/MixtapeProject.h"
#include "../../audio/PerceptualPlaybackProcessor.h"
#include "AnalogVuMeter.h"

namespace cassette
{

class TapeParameterPanel : public juce::Component,
                            private juce::Button::Listener,
                            private juce::Slider::Listener
{
public:
    std::function<void(TapeFormulation)> onTapeTypeChanged;
    std::function<void(float)> onRecLevelChanged;
    std::function<void(float)> onBiasChanged;
    std::function<void(bool)> onMaximumDigitalChanged;
    std::function<void(const PerceptualPlaybackSettings&)> onPreviewSettingsChanged;
    std::function<void()> onRenderSideA;
    std::function<void()> onRenderSideB;
    std::function<void()> onExportMix;

    TapeParameterPanel();
    void bindProject(const MixtapeProject& project);
    void setVuLevels(float leftDb, float rightDb);
    PerceptualPlaybackSettings previewSettingsState() const { return previewSettings; }

private:
    void resized() override;
    void buttonClicked(juce::Button* b) override;
    void sliderValueChanged(juce::Slider* s) override;
    void refreshTapeButtons(TapeFormulation active);
    void pushPreviewSettings();

    PerceptualPlaybackSettings previewSettings;

    juce::TextButton typeIBtn { "TYPE I" };
    juce::TextButton typeIIBtn { "TYPE II" };
    juce::TextButton typeIVBtn { "TYPE IV" };
    juce::Slider biasSlider;
    juce::Slider recLevelSlider;
    juce::Slider inputGainSlider;
    juce::Label biasLabel { {}, "Bias" };
    juce::Label recLabel { {}, "Rec Level" };
    juce::ToggleButton maximumDigital { "Maximum Digital" };
    juce::ToggleButton crossfeedToggle { "Crossfeed" };
    juce::ToggleButton virtualSubToggle { "Virtual sub" };
    juce::ToggleButton widenToggle { "Stereo widen" };
    juce::Slider crossfeedSlider;
    juce::Slider subSlider;
    juce::Slider widenSlider;
    juce::ComboBox dolbyBox;
    juce::ToggleButton dolbyOn { "On" };
    AnalogVuMeter vu;
    juce::TextButton exportBtn { "Export Mix" };
    juce::TextButton renderABtn { "Render Side A" };
    juce::TextButton renderBBtn { "Render Side B" };
};

}
