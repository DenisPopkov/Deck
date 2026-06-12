#pragma once

#include <juce_gui_extra/juce_gui_extra.h>
#include <functional>
#include "../dsp/CassetteProfile.h"
#include "../dsp/MasteringOptions.h"
#include "../project/FolderMixBuilder.h"

namespace cassette
{

class TapeSetupPanel : public juce::Component,
                       private juce::Button::Listener,
                       private juce::Slider::Listener
{
public:
    std::function<void()> onPrepareClicked;
    std::function<void()> onSetupChanged;
    std::function<void()> onChangeTapeTypeRequested;

    TapeSetupPanel();

    TapeFormulation getTapeFormulation() const;
    RecordingDeck getRecordingDeck() const;
    CassetteProfile getCassetteProfile() const;
    MasteringOptions getMasteringOptions() const;
    TapeLengthSpec getTapeLengthSpec() const;

    void setMainScreenMode(bool mainScreen);
    void setCompactToolbarMode(bool compact);
    void setMixtapeMode(bool mixtape);
    void setPrepareEnabled(bool enabled);
    void setPrepareVisible(bool visible);
    void setInteractionEnabled(bool enabled);
    void setTapeTypeLocked(bool locked);
    void applyPreset(int presetId);
    void setTapeFitSummary(const juce::String& text, bool ok);

    juce::String tapeSummaryText() const;
    bool isCustomTapeLengthSelected() const;
    bool isPreflightEnabled() const { return preflightToggle.getToggleState(); }
    void triggerPrepare() { if (onPrepareClicked) onPrepareClicked(); }

private:
    void resized() override;
    void paint(juce::Graphics& g) override;
    void buttonClicked(juce::Button* b) override;
    void sliderValueChanged(juce::Slider* s) override;

    void notifyChanged();
    void syncTypeFromButtons();
    void refreshTypeSegmentStyles();
    void applyTapeTypeLockPresentation();
    juce::TextButton* activeTypeButton();
    void refreshLengthSegmentStyles();
    void syncLengthFromButtons();
    void updateLengthControlPresentation();

    bool mainScreenMode = false;
    bool compactToolbarMode = false;
    bool mixtapeMode = false;
    bool interactionEnabled = true;
    bool tapeTypeLocked = false;

    juce::Label tapeTypeLabel { {}, "Cassette type" };
    juce::Label tapeLengthLabel { {}, "Length per side" };
    juce::Label tapeFitLabel;
    juce::ComboBox presetBox;
    juce::ComboBox tapeTypeBox;
    juce::TextButton typeBtnI { "Type I" };
    juce::TextButton typeBtnII { "Type II" };
    juce::TextButton typeBtnIV { "Type IV" };
    juce::TextButton changeTypeButton { "Change" };
    juce::TextButton lengthBtnCustom { "Custom" };
    juce::TextButton lengthBtnC60 { "C60" };
    juce::TextButton lengthBtnC90 { "C90" };
    juce::TextButton lengthBtnC120 { "C120" };
    juce::ComboBox tapeLengthBox;
    juce::Slider customMinutesSlider;
    juce::ToggleButton preflightToggle { "KX-1100G cal tones (export)" };
    juce::TextButton prepareButton { "Prepare for Tape" };
};

}
