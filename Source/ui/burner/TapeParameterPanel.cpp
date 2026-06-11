#include "TapeParameterPanel.h"
#include "../look/CassetteBurnerLook.h"

namespace cassette
{

TapeParameterPanel::TapeParameterPanel()
{
    for (auto* b : { &typeIBtn, &typeIIBtn, &typeIVBtn })
    {
        addAndMakeVisible(*b);
        b->setClickingTogglesState(true);
        b->setRadioGroupId(9001);
        b->addListener(this);
    }
    typeIVBtn.setToggleState(true, juce::dontSendNotification);

    for (auto* s : { &biasSlider, &recLevelSlider, &inputGainSlider })
    {
        addAndMakeVisible(*s);
        s->setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        s->setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 18);
        s->addListener(this);
    }
    biasSlider.setRange(-2.0, 2.0, 0.05);
    recLevelSlider.setRange(-12.0, 0.0, 0.1);
    inputGainSlider.setRange(-12.0, 6.0, 0.1);
    recLevelSlider.setValue(-3.2, juce::dontSendNotification);

    addAndMakeVisible(biasLabel);
    addAndMakeVisible(recLabel);
    addAndMakeVisible(maximumDigital);
    maximumDigital.addListener(this);

    for (auto* t : { &crossfeedToggle, &virtualSubToggle, &widenToggle })
    {
        addAndMakeVisible(*t);
        t->setToggleState(t != &widenToggle, juce::dontSendNotification);
        t->addListener(this);
    }
    for (auto* s : { &crossfeedSlider, &subSlider, &widenSlider })
    {
        addAndMakeVisible(*s);
        s->setSliderStyle(juce::Slider::LinearHorizontal);
        s->setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        s->setRange(0.0, 1.0, 0.01);
        s->addListener(this);
    }
    crossfeedSlider.setValue(previewSettings.crossfeedAmount, juce::dontSendNotification);
    subSlider.setValue(previewSettings.virtualSubAmount, juce::dontSendNotification);
    widenSlider.setValue(previewSettings.stereoWidenAmount, juce::dontSendNotification);

    addAndMakeVisible(dolbyBox);
    dolbyBox.addItem("Dolby B", 1);
    dolbyBox.addItem("Dolby C", 2);
    dolbyBox.addItem("Off", 3);
    dolbyBox.setSelectedId(1, juce::dontSendNotification);
    addAndMakeVisible(dolbyOn);
    dolbyOn.setToggleState(true, juce::dontSendNotification);
    addAndMakeVisible(vu);
    addAndMakeVisible(exportBtn);
    addAndMakeVisible(renderABtn);
    addAndMakeVisible(renderBBtn);
    exportBtn.addListener(this);
    renderABtn.addListener(this);
    renderBBtn.addListener(this);
}

void TapeParameterPanel::bindProject(const MixtapeProject& project)
{
    refreshTapeButtons(project.profile.formulation);
    recLevelSlider.setValue(project.recLevelDb, juce::dontSendNotification);
    biasSlider.setValue(project.biasDb, juce::dontSendNotification);
    maximumDigital.setToggleState(project.mastering.maximumDigital, juce::dontSendNotification);
}

void TapeParameterPanel::setVuLevels(float leftDb, float rightDb) { vu.setLevelDb(leftDb, rightDb); }

void TapeParameterPanel::refreshTapeButtons(TapeFormulation active)
{
    typeIBtn.setToggleState(active == TapeFormulation::TypeI, juce::dontSendNotification);
    typeIIBtn.setToggleState(active == TapeFormulation::TypeII, juce::dontSendNotification);
    typeIVBtn.setToggleState(active == TapeFormulation::TypeIV, juce::dontSendNotification);
}

void TapeParameterPanel::resized()
{
    auto r = getLocalBounds().reduced(10);
    biasLabel.setBounds(r.removeFromTop(18));
    biasSlider.setBounds(r.removeFromTop(90));
    r.removeFromTop(8);
    recLabel.setBounds(r.removeFromTop(18));
    recLevelSlider.setBounds(r.removeFromTop(90));
    r.removeFromTop(8);
    auto types = r.removeFromTop(28);
    const int w = types.getWidth() / 3;
    typeIBtn.setBounds(types.removeFromLeft(w).reduced(2));
    typeIIBtn.setBounds(types.removeFromLeft(w).reduced(2));
    typeIVBtn.setBounds(types.reduced(2));
    r.removeFromTop(8);
    maximumDigital.setBounds(r.removeFromTop(22));
    crossfeedToggle.setBounds(r.removeFromTop(20));
    crossfeedSlider.setBounds(r.removeFromTop(18));
    virtualSubToggle.setBounds(r.removeFromTop(20));
    subSlider.setBounds(r.removeFromTop(18));
    widenToggle.setBounds(r.removeFromTop(20));
    widenSlider.setBounds(r.removeFromTop(18));
    auto dolbyRow = r.removeFromTop(26);
    dolbyBox.setBounds(dolbyRow.removeFromLeft(dolbyRow.getWidth() - 50));
    dolbyOn.setBounds(dolbyRow);
    r.removeFromTop(8);
    vu.setBounds(r.removeFromTop(100));
    r.removeFromTop(8);
    renderABtn.setBounds(r.removeFromTop(32));
    r.removeFromTop(4);
    renderBBtn.setBounds(r.removeFromTop(32));
    r.removeFromTop(4);
    exportBtn.setBounds(r.removeFromTop(32));
}

void TapeParameterPanel::pushPreviewSettings()
{
    previewSettings.crossfeedEnabled = crossfeedToggle.getToggleState();
    previewSettings.virtualSubEnabled = virtualSubToggle.getToggleState();
    previewSettings.stereoWidenEnabled = widenToggle.getToggleState();
    previewSettings.crossfeedAmount = static_cast<float>(crossfeedSlider.getValue());
    previewSettings.virtualSubAmount = static_cast<float>(subSlider.getValue());
    previewSettings.stereoWidenAmount = static_cast<float>(widenSlider.getValue());
    if (onPreviewSettingsChanged)
        onPreviewSettingsChanged(previewSettings);
}

void TapeParameterPanel::buttonClicked(juce::Button* b)
{
    if (b == &typeIBtn && onTapeTypeChanged)
        onTapeTypeChanged(TapeFormulation::TypeI);
    if (b == &typeIIBtn && onTapeTypeChanged)
        onTapeTypeChanged(TapeFormulation::TypeII);
    if (b == &typeIVBtn && onTapeTypeChanged)
        onTapeTypeChanged(TapeFormulation::TypeIV);
    if (b == &maximumDigital && onMaximumDigitalChanged)
        onMaximumDigitalChanged(maximumDigital.getToggleState());
    if (b == &crossfeedToggle || b == &virtualSubToggle || b == &widenToggle)
        pushPreviewSettings();
    if (b == &renderABtn && onRenderSideA)
        onRenderSideA();
    if (b == &renderBBtn && onRenderSideB)
        onRenderSideB();
    if (b == &exportBtn && onExportMix)
        onExportMix();
}

void TapeParameterPanel::sliderValueChanged(juce::Slider* s)
{
    if (s == &recLevelSlider && onRecLevelChanged)
        onRecLevelChanged(static_cast<float>(recLevelSlider.getValue()));
    if (s == &biasSlider && onBiasChanged)
        onBiasChanged(static_cast<float>(biasSlider.getValue()));
    if (s == &crossfeedSlider || s == &subSlider || s == &widenSlider)
        pushPreviewSettings();
}

}
