#include "TapeSetupPanel.h"
#include "UiTheme.h"
#include "../util/AppLocale.h"

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

    addAndMakeVisible(tapeFitLabel);
    Theme::applyLabel(tapeFitLabel, Theme::captionFont(), Theme::textSecondary());
    tapeFitLabel.setVisible(false);

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
        b->setComponentID("tape-type-segment");
        b->setClickingTogglesState(true);
        b->addListener(this);
    }
    typeBtnI.setToggleState(true, juce::dontSendNotification);
    refreshTypeSegmentStyles();

    addAndMakeVisible(changeTypeButton);
    ui::Theme::styleNeutralButton(changeTypeButton);
    changeTypeButton.addListener(this);
    changeTypeButton.setVisible(false);

    for (auto* b : { &lengthBtnCustom, &lengthBtnC60, &lengthBtnC90, &lengthBtnC120 })
    {
        addAndMakeVisible(*b);
        b->setRadioGroupId(9200);
        b->setClickingTogglesState(true);
        b->addListener(this);
    }
    lengthBtnCustom.setTooltip(tr("tape.tooltip.custom"));
    lengthBtnC60.setTooltip(tr("tape.tooltip.c60"));
    lengthBtnC90.setTooltip(tr("tape.tooltip.c90"));
    lengthBtnC120.setTooltip(tr("tape.tooltip.c120"));
    lengthBtnC90.setToggleState(true, juce::dontSendNotification);
    refreshLengthSegmentStyles();

    addAndMakeVisible(tapeLengthBox);
    tapeLengthBox.addItem("C60 (30 min/side)", 1);
    tapeLengthBox.addItem("C90 (45 min/side)", 2);
    tapeLengthBox.addItem("C120 (60 min/side)", 3);
    tapeLengthBox.addItem("Custom minutes", 4);
    tapeLengthBox.setSelectedId(2, juce::dontSendNotification);
    Theme::styleCombo(tapeLengthBox);
    tapeLengthBox.setVisible(false);
    tapeLengthBox.onChange = [this] {
        updateLengthControlPresentation();
        notifyChanged();
    };

    addAndMakeVisible(customMinutesSlider);
    customMinutesSlider.setRange(20.0, 240.0, 2.0);
    customMinutesSlider.setValue(90.0, juce::dontSendNotification);
    customMinutesSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    customMinutesSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 84, 24);
    customMinutesSlider.textFromValueFunction = [](double value) {
        return juce::String(juce::roundToInt(value)) + " min";
    };
    Theme::styleAccentSlider(customMinutesSlider);
    customMinutesSlider.setVisible(false);
    customMinutesSlider.setEnabled(false);
    customMinutesSlider.onValueChange = [this] { notifyChanged(); };
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

    refreshLocalisedText();
}

void TapeSetupPanel::refreshLocalisedText()
{
    typeBtnI.setButtonText(tr("tape.type_i"));
    typeBtnII.setButtonText(tr("tape.type_ii"));
    typeBtnIV.setButtonText(tr("tape.type_iv"));
    lengthBtnCustom.setButtonText(tr("tape.custom"));
    changeTypeButton.setButtonText(tr("btn.change"));
    lengthBtnCustom.setTooltip(tr("tape.tooltip.custom"));
    lengthBtnC60.setTooltip(tr("tape.tooltip.c60"));
    lengthBtnC90.setTooltip(tr("tape.tooltip.c90"));
    lengthBtnC120.setTooltip(tr("tape.tooltip.c120"));
    if (!mainScreenMode)
        prepareButton.setButtonText(mixtapeMode ? tr("tape.build_sides") : tr("tape.prepare_for_tape"));
    refreshTypeSegmentStyles();
    repaint();
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
    if (mainScreenMode)
    {
        if (lengthBtnCustom.getToggleState())
            return tapeLengthSpecForPreset(TapeLengthPreset::Custom, customMinutesSlider.getValue());
        if (lengthBtnC60.getToggleState())
            return tapeLengthSpecForPreset(TapeLengthPreset::C60, 30.0);
        if (lengthBtnC120.getToggleState())
            return tapeLengthSpecForPreset(TapeLengthPreset::C120, 60.0);
        return tapeLengthSpecForPreset(TapeLengthPreset::C90, 45.0);
    }

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

bool TapeSetupPanel::isCustomTapeLengthSelected() const
{
    return mainScreenMode ? lengthBtnCustom.getToggleState()
                          : tapeLengthBox.getSelectedId() == 4;
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
    const bool showLengthControls = mixtape || !mainScreenMode;
    lengthBtnCustom.setVisible(mainScreenMode && mixtape);
    lengthBtnC60.setVisible(mainScreenMode && mixtape);
    lengthBtnC90.setVisible(mainScreenMode && mixtape);
    lengthBtnC120.setVisible(mainScreenMode && mixtape);
    tapeLengthBox.setVisible(!mainScreenMode && showLengthControls);
    tapeLengthLabel.setVisible(!mainScreenMode && mixtape);
    tapeFitLabel.setVisible(false);
    updateLengthControlPresentation();
    if (!mainScreenMode)
        prepareButton.setButtonText(mixtape ? tr("tape.build_sides") : tr("tape.prepare_for_tape"));
    if (!mixtape)
        tapeFitLabel.setText({}, juce::dontSendNotification);
    resized();
    repaint();
}

void TapeSetupPanel::setTapeFitSummary(const juce::String& text, bool ok)
{
    if (mainScreenMode)
    {
        tapeFitLabel.setText({}, juce::dontSendNotification);
        tapeFitLabel.setVisible(false);
        return;
    }

    tapeFitLabel.setText(text, juce::dontSendNotification);
    tapeFitLabel.setColour(juce::Label::textColourId,
                           ok ? ui::Theme::textPrimary() : ui::Theme::warnAmber());
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
    lengthBtnCustom.setEnabled(enabled);
    lengthBtnC60.setEnabled(enabled);
    lengthBtnC90.setEnabled(enabled);
    lengthBtnC120.setEnabled(enabled);
    updateLengthControlPresentation();
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

void TapeSetupPanel::refreshLengthSegmentStyles()
{
    const bool enabled = interactionEnabled && isEnabled();
    ui::Theme::styleTapeTypeSegment(lengthBtnCustom, lengthBtnCustom.getToggleState(), enabled);
    ui::Theme::styleTapeTypeSegment(lengthBtnC60, lengthBtnC60.getToggleState(), enabled);
    ui::Theme::styleTapeTypeSegment(lengthBtnC90, lengthBtnC90.getToggleState(), enabled);
    ui::Theme::styleTapeTypeSegment(lengthBtnC120, lengthBtnC120.getToggleState(), enabled);
}

void TapeSetupPanel::syncLengthFromButtons()
{
    if (lengthBtnCustom.getToggleState())
        tapeLengthBox.setSelectedId(4, juce::dontSendNotification);
    else if (lengthBtnC60.getToggleState())
        tapeLengthBox.setSelectedId(1, juce::dontSendNotification);
    else if (lengthBtnC120.getToggleState())
        tapeLengthBox.setSelectedId(3, juce::dontSendNotification);
    else
        tapeLengthBox.setSelectedId(2, juce::dontSendNotification);
    refreshLengthSegmentStyles();
}

void TapeSetupPanel::updateLengthControlPresentation()
{
    const bool customSelected = isCustomTapeLengthSelected();
    customMinutesSlider.setVisible(mixtapeMode && customSelected);
    customMinutesSlider.setEnabled(interactionEnabled && customSelected);
    ui::Theme::styleAccentSlider(customMinutesSlider);
    resized();
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

    auto header = getLocalBounds().reduced(14, 12);
    ui::Theme::drawSectionLabel(g, header.removeFromTop(18), tr("tape.parameters"));

    if (mixtapeMode)
    {
        header.removeFromTop(32 + 10);
        ui::Theme::drawSectionLabel(g, header.removeFromTop(18), tr("tape.length"));
    }
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
            r.removeFromTop(10);
            r.removeFromTop(18);

            auto lengthRow = r.removeFromTop(32);
            const int lenGap = 4;
            const int lenSegW = (lengthRow.getWidth() - lenGap * 3) / 4;
            lengthBtnC60.setBounds(lengthRow.removeFromLeft(lenSegW));
            lengthRow.removeFromLeft(lenGap);
            lengthBtnC90.setBounds(lengthRow.removeFromLeft(lenSegW));
            lengthRow.removeFromLeft(lenGap);
            lengthBtnC120.setBounds(lengthRow.removeFromLeft(lenSegW));
            lengthRow.removeFromLeft(lenGap);
            lengthBtnCustom.setBounds(lengthRow);

            if (lengthBtnCustom.getToggleState())
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
    else if (b == &lengthBtnCustom || b == &lengthBtnC60 || b == &lengthBtnC90 || b == &lengthBtnC120)
    {
        if (b == &lengthBtnC60)
            customMinutesSlider.setValue(30.0, juce::dontSendNotification);
        else if (b == &lengthBtnC90)
            customMinutesSlider.setValue(45.0, juce::dontSendNotification);
        else if (b == &lengthBtnC120)
            customMinutesSlider.setValue(60.0, juce::dontSendNotification);

        syncLengthFromButtons();
        updateLengthControlPresentation();
        notifyChanged();
    }
}

void TapeSetupPanel::sliderValueChanged(juce::Slider*)
{
    notifyChanged();
}

}
