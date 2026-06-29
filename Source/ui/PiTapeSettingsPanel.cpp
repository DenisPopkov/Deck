#include "PiTapeSettingsPanel.h"
#include "UiTheme.h"
#include "../util/AppLocale.h"

namespace cassette
{

namespace
{

constexpr int kEnableRowH = 46;
constexpr int kEnableRowGap = 10;
constexpr int kCheckboxSize = 20;
constexpr int kCheckboxGap = 4;
constexpr int kTextLeftInset = 1;
constexpr float kCheckboxDownOffset = 3.0f;

juce::Font enableTitleFont()
{
#if JUCE_MAC
    return juce::Font(juce::FontOptions("SF Pro Text", 12.0f, juce::Font::bold));
#else
    return ui::Theme::uiFont(12.0f, juce::Font::bold);
#endif
}

int enableCheckboxY(int textY)
{
    const int titleH = juce::roundToInt(enableTitleFont().getHeight());
    const int bodyY = textY + titleH + 2;
    const int bodyAscent = juce::roundToInt(ui::Theme::captionFont().getAscent());
    return juce::roundToInt((float) (bodyY + bodyAscent - kCheckboxSize) + kCheckboxDownOffset);
}

void layoutSingleLineEditor(juce::TextEditor& editor, juce::Rectangle<int> bounds)
{
    editor.setBounds(bounds);
    const int lineH = juce::roundToInt(editor.getFont().getHeight());
    const int topIndent = juce::jmax(0, (bounds.getHeight() - lineH) / 2);
    editor.setIndents(6, topIndent);
}

} // namespace

PiTapeSettingsPanel::PiTapeSettingsPanel()
{
    addAndMakeVisible(enabledToggle);
    addAndMakeVisible(hostLabel);
    addAndMakeVisible(portLabel);
    addAndMakeVisible(controlPortLabel);
    addAndMakeVisible(userLabel);
    addAndMakeVisible(passLabel);
    addAndMakeVisible(dirLabel);
    addAndMakeVisible(hostEditor);
    addAndMakeVisible(portEditor);
    addAndMakeVisible(controlPortEditor);
    addAndMakeVisible(userEditor);
    addAndMakeVisible(passEditor);
    addAndMakeVisible(dirEditor);

    for (auto* label : { &hostLabel, &portLabel, &controlPortLabel, &userLabel, &passLabel, &dirLabel })
        ui::Theme::applyLabel(*label, ui::Theme::captionFont(), ui::Theme::textSecondary());

    enabledToggle.setComponentID("settings-checkbox");
    enabledToggle.setButtonText({});
    enabledToggle.setPaintingIsUnclipped(true);
    enabledToggle.setColour(juce::ToggleButton::textColourId, ui::Theme::textPrimary());
    enabledToggle.setColour(juce::ToggleButton::tickColourId, ui::Theme::accent());

    for (auto* editor : { &hostEditor, &portEditor, &controlPortEditor, &userEditor, &passEditor, &dirEditor })
    {
        editor->setMultiLine(false);
        editor->setScrollbarsShown(false);
        editor->setScrollToShowCursor(false);
        editor->setBorder(juce::BorderSize<int>(0));
        editor->setColour(juce::TextEditor::backgroundColourId, ui::Theme::card());
        editor->setColour(juce::TextEditor::outlineColourId, ui::Theme::borderLight());
        editor->setColour(juce::TextEditor::textColourId, ui::Theme::textPrimary());
        editor->setFont(ui::Theme::bodyFont());
    }

    passEditor.setPasswordCharacter(juce::juce_wchar(0x2022));
    portEditor.setInputRestrictions(5, "0123456789");
    controlPortEditor.setInputRestrictions(5, "0123456789");

    refreshLocalisedText();
}

void PiTapeSettingsPanel::setSettings(const PiTapeSettings& settings)
{
    enabledToggle.setToggleState(settings.enabled, juce::dontSendNotification);
    hostEditor.setText(settings.host, false);
    portEditor.setText(juce::String(settings.port), false);
    controlPortEditor.setText(juce::String(settings.controlPort), false);
    userEditor.setText(settings.username, false);
    passEditor.setText(settings.password, false);
    dirEditor.setText(settings.remoteDir, false);
}

PiTapeSettings PiTapeSettingsPanel::getSettings() const
{
    PiTapeSettings settings;
    settings.enabled = enabledToggle.getToggleState();
    settings.host = hostEditor.getText().trim();
    settings.port = portEditor.getText().getIntValue();
    settings.controlPort = controlPortEditor.getText().getIntValue();
    settings.username = userEditor.getText().trim();
    settings.password = passEditor.getText();
    settings.remoteDir = dirEditor.getText().trim();
    if (settings.port <= 0)
        settings.port = 21;
    if (settings.controlPort <= 0)
        settings.controlPort = 8765;
    if (settings.remoteDir.isEmpty())
        settings.remoteDir = "inbox";
    return settings;
}

int PiTapeSettingsPanel::getPreferredHeight() const
{
    return kEnableRowH + kEnableRowGap + 6 * (28 + 8);
}

void PiTapeSettingsPanel::refreshLocalisedText()
{
    enableTitleText = tr("settings.pi_tape.enable.title");
    enableBodyText = tr("settings.pi_tape.enable.desc");
    hostLabel.setText(tr("settings.pi_tape.host"), juce::dontSendNotification);
    portLabel.setText(tr("settings.pi_tape.port"), juce::dontSendNotification);
    controlPortLabel.setText(tr("settings.pi_tape.control_port"), juce::dontSendNotification);
    userLabel.setText(tr("settings.pi_tape.user"), juce::dontSendNotification);
    passLabel.setText(tr("settings.pi_tape.password"), juce::dontSendNotification);
    dirLabel.setText(tr("settings.pi_tape.remote_dir"), juce::dontSendNotification);
    repaint();
}

void PiTapeSettingsPanel::layoutEnableRow(juce::Rectangle<int>& area)
{
    auto row = area.removeFromTop(kEnableRowH);
    const int titleH = juce::roundToInt(enableTitleFont().getHeight());
    const int bodyH = juce::roundToInt(ui::Theme::captionFont().getHeight());
    const int textY = row.getY();
    const int checkboxY = enableCheckboxY(textY);
    const int textX = row.getX() + kCheckboxSize + kCheckboxGap + kTextLeftInset;
    const int textW = row.getWidth() - kCheckboxSize - kCheckboxGap - kTextLeftInset;

    enabledToggle.setBounds(row.getX(), checkboxY, kCheckboxSize, kCheckboxSize);
    enableTextArea = { textX, textY, textW, titleH + 2 + bodyH };
    enableRowArea = row;
    area.removeFromTop(kEnableRowGap);
}

void PiTapeSettingsPanel::paint(juce::Graphics& g)
{
    if (enableTextArea.isEmpty())
        return;

    const int titleH = juce::roundToInt(enableTitleFont().getHeight());
    const int bodyH = juce::roundToInt(ui::Theme::captionFont().getHeight());

    g.setColour(ui::Theme::textPrimary());
    g.setFont(enableTitleFont());
    g.drawText(enableTitleText,
               enableTextArea.getX(),
               enableTextArea.getY(),
               enableTextArea.getWidth(),
               titleH,
               juce::Justification::topLeft);

    g.setFont(ui::Theme::captionFont());
    g.drawText(enableBodyText,
               enableTextArea.getX(),
               enableTextArea.getY() + titleH + 2,
               enableTextArea.getWidth(),
               bodyH,
               juce::Justification::topLeft);
}

void PiTapeSettingsPanel::resized()
{
    auto r = getLocalBounds();

    layoutEnableRow(r);

    const int labelW = 92;
    const int rowH = 28;
    const int gap = 8;

    auto row = [&](juce::Label& label, juce::TextEditor& editor, int editorW) {
        auto line = r.removeFromTop(rowH);
        label.setBounds(line.removeFromLeft(labelW));
        line.removeFromLeft(6);
        layoutSingleLineEditor(editor, line.removeFromLeft(editorW));
        r.removeFromTop(gap);
    };

    row(hostLabel, hostEditor, getWidth() - labelW - 6);
    row(portLabel, portEditor, 72);
    row(controlPortLabel, controlPortEditor, 72);
    row(userLabel, userEditor, getWidth() - labelW - 6);
    row(passLabel, passEditor, getWidth() - labelW - 6);
    row(dirLabel, dirEditor, getWidth() - labelW - 6);
}

void PiTapeSettingsPanel::mouseDown(const juce::MouseEvent& e)
{
    if (enableRowArea.contains(e.getPosition()) && enabledToggle.isEnabled())
        enabledToggle.setToggleState(!enabledToggle.getToggleState(), juce::sendNotification);
}

void PiTapeSettingsPanel::mouseMove(const juce::MouseEvent& e)
{
    const bool overRow = enableRowArea.contains(e.getPosition()) && enabledToggle.isEnabled();
    setMouseCursor(overRow ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::NormalCursor);
}

void PiTapeSettingsPanel::mouseExit(const juce::MouseEvent&)
{
    setMouseCursor(juce::MouseCursor::NormalCursor);
}

} // namespace cassette
