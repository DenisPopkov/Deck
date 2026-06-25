#include "ProcessingOptionsPanel.h"
#include "UiTheme.h"

namespace cassette
{

namespace
{

constexpr int kDialogRowH = 46;
constexpr int kDialogRowGap = 10;
constexpr int kDialogCheckboxSize = 20;
constexpr int kDialogCheckboxGap = 4;
constexpr int kDialogTextLeftInset = 1;
constexpr int kDialogTextTopInset = 0;
constexpr int kDialogLineGap = 2;
constexpr float kDialogCheckboxDownOffset = 3.0f;

juce::Font settingsOptionTitleFont()
{
#if JUCE_MAC
    return juce::Font(juce::FontOptions("SF Pro Text", 12.0f, juce::Font::bold));
#else
    return ui::Theme::uiFont(12.0f, juce::Font::bold);
#endif
}

struct DialogRowMetrics
{
    int titleH = 0;
    int bodyH = 0;
    int blockH = 0;
};

DialogRowMetrics dialogRowMetrics()
{
    DialogRowMetrics m;
    m.titleH = juce::roundToInt(settingsOptionTitleFont().getHeight());
    m.bodyH = juce::roundToInt(ui::Theme::captionFont().getHeight());
    m.blockH = m.titleH + kDialogLineGap + m.bodyH;
    return m;
}

int dialogCheckboxY(int textY, const DialogRowMetrics& m)
{
    const int bodyY = textY + m.titleH + kDialogLineGap;
    const int bodyAscent = juce::roundToInt(ui::Theme::captionFont().getAscent());
    return juce::roundToInt((float) (bodyY + bodyAscent - kDialogCheckboxSize) + kDialogCheckboxDownOffset);
}

}

ProcessingOptionsPanel::ProcessingOptionsPanel()
{
    addAndMakeVisible(sectionLabel);
    ui::Theme::applyLabel(sectionLabel, ui::Theme::captionFont(), ui::Theme::textSecondary());

    for (auto* t : { &truePeakLimiterToggle, &stereoTighteningToggle })
    {
        addAndMakeVisible(*t);
        t->setToggleState(true, juce::dontSendNotification);
        t->setColour(juce::ToggleButton::textColourId, ui::Theme::textPrimary());
        t->setColour(juce::ToggleButton::tickColourId, ui::Theme::accent());
        t->addListener(this);
    }

    refreshLocalisedText();
}

void ProcessingOptionsPanel::refreshLocalisedText()
{
    sectionLabel.setText(tr("settings.processing"), juce::dontSendNotification);
    truePeakTitleText = tr("processing.true_peak.title");
    truePeakBodyText = tr("processing.true_peak.desc");
    stereoTitleText = tr("processing.stereo.title");
    stereoBodyText = tr("processing.stereo.desc");

    if (dialogMode)
    {
        truePeakLimiterToggle.setButtonText({});
        stereoTighteningToggle.setButtonText({});
    }
    else
    {
        truePeakLimiterToggle.setButtonText(tr("processing.true_peak"));
        stereoTighteningToggle.setButtonText(tr("processing.stereo"));
    }

    repaint();
}

void ProcessingOptionsPanel::setOptions(const MasteringOptions& newOptions)
{
    options = newOptions;
    truePeakLimiterToggle.setToggleState(options.enableTruePeakLimiter, juce::dontSendNotification);
    stereoTighteningToggle.setToggleState(options.enableStereoTightening, juce::dontSendNotification);
}

MasteringOptions ProcessingOptionsPanel::getOptions() const
{
    auto out = options;
    out.enableTruePeakLimiter = truePeakLimiterToggle.getToggleState();
    out.enableStereoTightening = stereoTighteningToggle.getToggleState();
    return out;
}

void ProcessingOptionsPanel::setDialogMode(bool isDialogMode)
{
    dialogMode = isDialogMode;
    sectionLabel.setVisible(!dialogMode);

    const auto checkboxId = isDialogMode ? juce::String("settings-checkbox") : juce::String();
    for (auto* toggle : { &truePeakLimiterToggle, &stereoTighteningToggle })
    {
        toggle->setComponentID(checkboxId);
        toggle->setPaintingIsUnclipped(isDialogMode);
    }

    refreshLocalisedText();
    resized();
}

int ProcessingOptionsPanel::getPreferredHeight() const
{
    if (dialogMode)
        return kDialogRowH + kDialogRowGap + kDialogRowH;

    return sectionLabel.isVisible() ? 18 + 4 + 22 + 2 + 22 : 22 + 2 + 22;
}

void ProcessingOptionsPanel::setInteractionEnabled(bool enabled)
{
    truePeakLimiterToggle.setEnabled(enabled);
    stereoTighteningToggle.setEnabled(enabled);
}

void ProcessingOptionsPanel::layoutDialogRow(juce::Rectangle<int>& area, juce::ToggleButton& toggle)
{
    auto row = area.removeFromTop(kDialogRowH);
    const auto metrics = dialogRowMetrics();
    const int textY = row.getY() + kDialogTextTopInset;
    const int checkboxY = dialogCheckboxY(textY, metrics);
    const int textX = row.getX() + kDialogCheckboxSize + kDialogCheckboxGap + kDialogTextLeftInset;
    const int textW = row.getWidth() - kDialogCheckboxSize - kDialogCheckboxGap - kDialogTextLeftInset;

    toggle.setBounds(row.getX(), checkboxY, kDialogCheckboxSize, kDialogCheckboxSize);

    juce::Rectangle<int> textArea(textX, textY, textW, metrics.blockH);
    if (&toggle == &truePeakLimiterToggle)
    {
        truePeakTextArea = textArea;
        truePeakRowArea = row;
    }
    else
    {
        stereoTextArea = textArea;
        stereoRowArea = row;
    }

    area.removeFromTop(kDialogRowGap);
}

void ProcessingOptionsPanel::paint(juce::Graphics& g)
{
    if (!dialogMode)
        return;

    const auto drawRow = [&](const juce::Rectangle<int>& area,
                             const juce::String& title,
                             const juce::String& body) {
        if (area.isEmpty())
            return;

        const auto metrics = dialogRowMetrics();
        g.setColour(ui::Theme::textPrimary());
        g.setFont(settingsOptionTitleFont());
        g.drawText(title, area.getX(), area.getY(), area.getWidth(), metrics.titleH, juce::Justification::topLeft);

        g.setFont(ui::Theme::captionFont());
        g.drawText(body,
                   area.getX(),
                   area.getY() + metrics.titleH + kDialogLineGap,
                   area.getWidth(),
                   metrics.bodyH,
                   juce::Justification::topLeft);
    };

    drawRow(truePeakTextArea, truePeakTitleText, truePeakBodyText);
    drawRow(stereoTextArea, stereoTitleText, stereoBodyText);
}

void ProcessingOptionsPanel::resized()
{
    auto r = getLocalBounds();
    if (!dialogMode)
    {
        sectionLabel.setBounds(r.removeFromTop(18));
        r.removeFromTop(4);
        truePeakLimiterToggle.setBounds(r.removeFromTop(22));
        r.removeFromTop(2);
        stereoTighteningToggle.setBounds(r.removeFromTop(22));
        return;
    }

    layoutDialogRow(r, truePeakLimiterToggle);
    layoutDialogRow(r, stereoTighteningToggle);
}

void ProcessingOptionsPanel::toggleDialogRow(juce::ToggleButton& toggle)
{
    if (!dialogMode || !toggle.isEnabled())
        return;

    toggle.setToggleState(!toggle.getToggleState(), juce::sendNotificationSync);
}

void ProcessingOptionsPanel::updateDialogRowCursor(juce::Point<int> position)
{
    if (!dialogMode)
        return;

    const bool overRow = (truePeakRowArea.contains(position) && truePeakLimiterToggle.isEnabled())
                      || (stereoRowArea.contains(position) && stereoTighteningToggle.isEnabled());
    setMouseCursor(overRow ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::NormalCursor);
}

void ProcessingOptionsPanel::mouseDown(const juce::MouseEvent& e)
{
    if (!dialogMode)
        return;

    if (truePeakRowArea.contains(e.getPosition()))
        toggleDialogRow(truePeakLimiterToggle);
    else if (stereoRowArea.contains(e.getPosition()))
        toggleDialogRow(stereoTighteningToggle);
}

void ProcessingOptionsPanel::mouseMove(const juce::MouseEvent& e)
{
    updateDialogRowCursor(e.getPosition());
}

void ProcessingOptionsPanel::mouseExit(const juce::MouseEvent&)
{
    setMouseCursor(juce::MouseCursor::NormalCursor);
}

void ProcessingOptionsPanel::buttonClicked(juce::Button* b)
{
    if (b == &truePeakLimiterToggle || b == &stereoTighteningToggle)
        notifyChanged();
}

void ProcessingOptionsPanel::notifyChanged()
{
    options = getOptions();
    if (onChanged)
        onChanged();
}

}
