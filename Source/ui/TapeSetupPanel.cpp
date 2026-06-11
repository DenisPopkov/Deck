#include "TapeSetupPanel.h"
#include "UiTheme.h"

namespace cassette
{

TapeSetupPanel::TapeSetupPanel()
{
    using namespace ui;

    addAndMakeVisible(tapeTypeLabel);
    Theme::applyLabel(tapeTypeLabel, Theme::captionFont(), Theme::textSecondary());

    addAndMakeVisible(tapeLengthLabel);
    Theme::applyLabel(tapeLengthLabel, Theme::captionFont(), Theme::textSecondary());
    tapeLengthLabel.setVisible(false);

    addAndMakeVisible(presetBox);
    presetBox.addItem("Preset: Custom", 1);
    presetBox.addItem("Type IV deck", 2);
    presetBox.addItem("Type II chrome", 3);
    presetBox.addItem("Type I normal", 4);
    presetBox.setSelectedId(1, juce::dontSendNotification);
    Theme::styleCombo(presetBox);
    presetBox.onChange = [this] {
        if (presetBox.getSelectedId() > 1)
            applyPreset(presetBox.getSelectedId());
        notifyChanged();
    };

    addAndMakeVisible(tapeTypeBox);
    tapeTypeBox.addItem("Type I (Normal)", 1);
    tapeTypeBox.addItem("Type II (Chrome)", 2);
    tapeTypeBox.addItem("Type IV (Metal)", 3);
    tapeTypeBox.setSelectedId(1, juce::dontSendNotification);
    Theme::styleCombo(tapeTypeBox);
    tapeTypeBox.onChange = [this] { notifyChanged(); };

    for (auto* b : { &typeBtnI, &typeBtnII, &typeBtnIV })
    {
        addAndMakeVisible(*b);
        b->setRadioGroupId(9100);
        b->setClickingTogglesState(true);
        b->addListener(this);
    }
    typeBtnI.setToggleState(true, juce::dontSendNotification);
    refreshTypeSegmentStyles();

    addAndMakeVisible(changeTypeButton);
    ui::Theme::styleNeutralButton(changeTypeButton);
    changeTypeButton.addListener(this);
    changeTypeButton.setVisible(false);

    addAndMakeVisible(tapeLengthBox);
    tapeLengthBox.addItem("C60 (30 min/side)", 1);
    tapeLengthBox.addItem("C90 (45 min/side)", 2);
    tapeLengthBox.addItem("C120 (60 min/side)", 3);
    tapeLengthBox.addItem("Custom minutes", 4);
    tapeLengthBox.setSelectedId(2, juce::dontSendNotification);
    Theme::styleCombo(tapeLengthBox);
    tapeLengthBox.onChange = [this] {
        customMinutesSlider.setEnabled(tapeLengthBox.getSelectedId() == 4);
        notifyChanged();
    };

    addAndMakeVisible(customMinutesSlider);
    customMinutesSlider.setRange(5.0, 120.0, 1.0);
    customMinutesSlider.setValue(45.0, juce::dontSendNotification);
    customMinutesSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    customMinutesSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 44, 22);
    customMinutesSlider.setEnabled(false);
    customMinutesSlider.addListener(this);

    addAndMakeVisible(preflightToggle);
    preflightToggle.setToggleState(false, juce::dontSendNotification);
    preflightToggle.setColour(juce::ToggleButton::textColourId, Theme::textSecondary());
    preflightToggle.setColour(juce::ToggleButton::tickColourId, Theme::accent());
    preflightToggle.onClick = [this] { notifyChanged(); };

    addAndMakeVisible(prepareButton);
    Theme::styleAccentButton(prepareButton);
    prepareButton.addListener(this);
    prepareButton.setEnabled(false);
}

TapeFormulation TapeSetupPanel::getTapeFormulation() const
{
    if (mainScreenMode)
    {
        if (typeBtnII.getToggleState())
            return TapeFormulation::TypeII;
        if (typeBtnIV.getToggleState())
            return TapeFormulation::TypeIV;
        return TapeFormulation::TypeI;
    }

    switch (tapeTypeBox.getSelectedId())
    {
        case 2: return TapeFormulation::TypeII;
        case 3: return TapeFormulation::TypeIV;
        case 1:
        default: return TapeFormulation::TypeI;
    }
}

RecordingDeck TapeSetupPanel::getRecordingDeck() const
{
    return RecordingDeck::KenwoodKX1100G;
}

CassetteProfile TapeSetupPanel::getCassetteProfile() const
{
    return CassetteProfile::forRecording(getRecordingDeck(), getTapeFormulation());
}

MasteringOptions TapeSetupPanel::getMasteringOptions() const
{
    MasteringOptions options;
    options.perceptualAutoFallback = true;
    return options;
}

TapeLengthSpec TapeSetupPanel::getTapeLengthSpec() const
{
    TapeLengthPreset preset = TapeLengthPreset::C90;
    switch (tapeLengthBox.getSelectedId())
    {
        case 1: preset = TapeLengthPreset::C60; break;
        case 3: preset = TapeLengthPreset::C120; break;
        case 4: preset = TapeLengthPreset::Custom; break;
        case 2:
        default: preset = TapeLengthPreset::C90; break;
    }
    return tapeLengthSpecForPreset(preset, customMinutesSlider.getValue());
}

void TapeSetupPanel::setMainScreenMode(bool mainScreen)
{
    mainScreenMode = mainScreen;
    presetBox.setVisible(!mainScreen);
    preflightToggle.setVisible(!mainScreen);
    prepareButton.setVisible(!mainScreen);
    tapeTypeLabel.setVisible(!mainScreen);
    tapeTypeBox.setVisible(!mainScreen);
    applyTapeTypeLockPresentation();
    resized();
    repaint();
}

void TapeSetupPanel::setCompactToolbarMode(bool compact)
{
    compactToolbarMode = compact;
    resized();
    repaint();
}

void TapeSetupPanel::setMixtapeMode(bool mixtape)
{
    mixtapeMode = mixtape;
    tapeLengthBox.setVisible(mixtape || !mainScreenMode);
    tapeLengthLabel.setVisible(mainScreenMode && mixtape);
    customMinutesSlider.setVisible(mixtape && tapeLengthBox.getSelectedId() == 4);
    if (!mainScreenMode)
        prepareButton.setButtonText(mixtape ? "Build Side A/B WAV" : "Prepare for Tape");
    resized();
    repaint();
}

void TapeSetupPanel::setPrepareEnabled(bool enabled)
{
    prepareButton.setEnabled(enabled);
}

void TapeSetupPanel::setPrepareVisible(bool visible)
{
    prepareButton.setVisible(visible);
    resized();
}

void TapeSetupPanel::setInteractionEnabled(bool enabled)
{
    interactionEnabled = enabled;

    presetBox.setEnabled(enabled);
    tapeTypeBox.setEnabled(enabled);
    tapeLengthBox.setEnabled(enabled);
    customMinutesSlider.setEnabled(enabled && tapeLengthBox.getSelectedId() == 4);
    preflightToggle.setEnabled(enabled);

    applyTapeTypeLockPresentation();
}

void TapeSetupPanel::setTapeTypeLocked(bool locked)
{
    if (tapeTypeLocked == locked)
        return;

    tapeTypeLocked = locked;
    applyTapeTypeLockPresentation();
    resized();
    repaint();
}

juce::TextButton* TapeSetupPanel::activeTypeButton()
{
    if (typeBtnII.getToggleState())
        return &typeBtnII;
    if (typeBtnIV.getToggleState())
        return &typeBtnIV;
    return &typeBtnI;
}

void TapeSetupPanel::applyTapeTypeLockPresentation()
{
    const bool showAllTypes = mainScreenMode && !tapeTypeLocked;
    typeBtnI.setVisible(mainScreenMode && (showAllTypes || typeBtnI.getToggleState()));
    typeBtnII.setVisible(mainScreenMode && (showAllTypes || typeBtnII.getToggleState()));
    typeBtnIV.setVisible(mainScreenMode && (showAllTypes || typeBtnIV.getToggleState()));

    changeTypeButton.setVisible(mainScreenMode && tapeTypeLocked);
    changeTypeButton.setEnabled(interactionEnabled);

    if (tapeTypeLocked)
    {
        typeBtnI.setEnabled(false);
        typeBtnII.setEnabled(false);
        typeBtnIV.setEnabled(false);
    }
    else
    {
        typeBtnI.setEnabled(interactionEnabled);
        typeBtnII.setEnabled(interactionEnabled);
        typeBtnIV.setEnabled(interactionEnabled);
    }

    refreshTypeSegmentStyles();
}

juce::String TapeSetupPanel::tapeSummaryText() const
{
    return getCassetteProfile().displayName + " / Auto mastering";
}

void TapeSetupPanel::applyPreset(int presetId)
{
    switch (presetId)
    {
        case 2:
            tapeTypeBox.setSelectedId(3, juce::dontSendNotification);
            typeBtnIV.setToggleState(true, juce::dontSendNotification);
            break;
        case 3:
            tapeTypeBox.setSelectedId(2, juce::dontSendNotification);
            typeBtnII.setToggleState(true, juce::dontSendNotification);
            break;
        case 4:
            tapeTypeBox.setSelectedId(1, juce::dontSendNotification);
            typeBtnI.setToggleState(true, juce::dontSendNotification);
            break;
        default:
            break;
    }
    refreshTypeSegmentStyles();
}

void TapeSetupPanel::syncTypeFromButtons()
{
    if (typeBtnII.getToggleState())
        tapeTypeBox.setSelectedId(2, juce::dontSendNotification);
    else if (typeBtnIV.getToggleState())
        tapeTypeBox.setSelectedId(3, juce::dontSendNotification);
    else
        tapeTypeBox.setSelectedId(1, juce::dontSendNotification);
    refreshTypeSegmentStyles();
}

void TapeSetupPanel::refreshTypeSegmentStyles()
{
    const bool enabled = interactionEnabled && isEnabled();
    ui::Theme::styleTapeTypeSegment(typeBtnI, typeBtnI.getToggleState(), enabled);
    ui::Theme::styleTapeTypeSegment(typeBtnII, typeBtnII.getToggleState(), enabled);
    ui::Theme::styleTapeTypeSegment(typeBtnIV, typeBtnIV.getToggleState(), enabled);
}

void TapeSetupPanel::notifyChanged()
{
    if (onSetupChanged)
        onSetupChanged();
}

void TapeSetupPanel::paint(juce::Graphics& g)
{
    if (!mainScreenMode)
    {
        ui::Theme::drawCard(g, getLocalBounds(), "Tape Setup");
        return;
    }

    if (compactToolbarMode)
        return;

    ui::Theme::drawSectionLabel(g, getLocalBounds().reduced(14, 12).removeFromTop(18), "Tape parameters");
}

void TapeSetupPanel::resized()
{
    auto r = getLocalBounds();

    if (mainScreenMode)
    {
        if (compactToolbarMode)
        {
            auto typeRow = r.reduced(0, 2);
            if (tapeTypeLocked)
            {
                const int changeW = 68;
                changeTypeButton.setBounds(typeRow.removeFromRight(changeW));
                typeRow.removeFromRight(4);
                if (auto* active = activeTypeButton())
                    active->setBounds(typeRow);
            }
            else
            {
                const int segW = juce::jmax(52, (typeRow.getWidth() - 8) / 3);
                typeBtnI.setBounds(typeRow.removeFromLeft(segW));
                typeRow.removeFromLeft(4);
                typeBtnII.setBounds(typeRow.removeFromLeft(segW));
                typeRow.removeFromLeft(4);
                typeBtnIV.setBounds(typeRow.removeFromLeft(segW));
            }
            return;
        }

        r = r.reduced(14, 12);
        r.removeFromTop(24);

        auto typeRow = r.removeFromTop(32);
        if (tapeTypeLocked)
        {
            const int changeW = 68;
            changeTypeButton.setBounds(typeRow.removeFromRight(changeW));
            typeRow.removeFromRight(4);
            if (auto* active = activeTypeButton())
                active->setBounds(typeRow);
        }
        else
        {
            const int segW = (typeRow.getWidth() - 8) / 3;
            typeBtnI.setBounds(typeRow.removeFromLeft(segW));
            typeRow.removeFromLeft(4);
            typeBtnII.setBounds(typeRow.removeFromLeft(segW));
            typeRow.removeFromLeft(4);
            typeBtnIV.setBounds(typeRow);
        }

        if (mixtapeMode)
        {
            r.removeFromTop(16);
            tapeLengthLabel.setBounds(r.removeFromTop(18));
            r.removeFromTop(4);
            tapeLengthBox.setBounds(r.removeFromTop(28));
            if (tapeLengthBox.getSelectedId() == 4)
            {
                r.removeFromTop(6);
                customMinutesSlider.setBounds(r.removeFromTop(28));
            }
        }
        return;
    }

    r = r.reduced(14).withTrimmedTop(32);

    presetBox.setBounds(r.removeFromTop(28));
    r.removeFromTop(8);
    tapeTypeBox.setBounds(r.removeFromTop(28));
    r.removeFromTop(8);

    if (mixtapeMode)
    {
        tapeLengthBox.setBounds(r.removeFromTop(28));
        r.removeFromTop(6);
        if (tapeLengthBox.getSelectedId() == 4)
        {
            customMinutesSlider.setBounds(r.removeFromTop(28));
            r.removeFromTop(8);
        }
    }

    preflightToggle.setBounds(r.removeFromTop(24));
    if (prepareButton.isVisible())
    {
        r.removeFromTop(12);
        prepareButton.setBounds(r.removeFromTop(36));
    }
}

void TapeSetupPanel::buttonClicked(juce::Button* b)
{
    if (b == &changeTypeButton)
    {
        if (onChangeTapeTypeRequested)
            onChangeTapeTypeRequested();
        return;
    }
    if (b == &prepareButton && onPrepareClicked)
        onPrepareClicked();
    else if (b == &typeBtnI || b == &typeBtnII || b == &typeBtnIV)
    {
        syncTypeFromButtons();
        notifyChanged();
    }
}

void TapeSetupPanel::sliderValueChanged(juce::Slider*)
{
    notifyChanged();
}

}
