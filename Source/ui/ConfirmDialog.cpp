#include "ConfirmDialog.h"
#include "UiTheme.h"

namespace cassette::ui
{

namespace
{

constexpr int kDialogWidth = 400;
constexpr int kPadH = 24;
constexpr int kPadV = 20;
constexpr int kTitleH = 20;
constexpr int kGapAfterTitle = 12;
constexpr int kGapBeforeButtons = 16;
constexpr int kButtonRowH = 36;
constexpr int kMinMessageH = 44;

int measureMessageHeight(const juce::String& message, int contentWidth)
{
    juce::AttributedString text;
    text.setText(message);
    text.setWordWrap(juce::AttributedString::WordWrap::byWord);
    text.setFont(Theme::bodyFont());
    text.setColour(Theme::textSecondary());

    juce::TextLayout layout;
    layout.createLayoutWithBalancedLineLengths(text, static_cast<float>(contentWidth));
    return juce::jmax(kMinMessageH, static_cast<int>(std::ceil(layout.getHeight())) + 4);
}

class ConfirmDialogPanel : public juce::Component
{
public:
    std::function<void(bool)> onResult;

    ConfirmDialogPanel(const ConfirmDialogOptions& options)
    {
        const int contentWidth = kDialogWidth - kPadH * 2;
        const int messageH = measureMessageHeight(options.message, contentWidth);
        dialogHeight = kPadV + kTitleH + kGapAfterTitle + messageH + kGapBeforeButtons + kButtonRowH + kPadV;

        addAndMakeVisible(titleLabel);
        Theme::applyLabel(titleLabel, Theme::sectionFont(), Theme::textPrimary(), juce::Justification::centred);
        titleLabel.setText(options.title, juce::dontSendNotification);

        addAndMakeVisible(messageLabel);
        Theme::applyLabel(messageLabel, Theme::bodyFont(), Theme::textSecondary(), juce::Justification::centred);
        messageLabel.setText(options.message, juce::dontSendNotification);

        addAndMakeVisible(confirmButton);
        addAndMakeVisible(cancelButton);
        confirmButton.setButtonText(options.confirmLabel);
        cancelButton.setButtonText(options.cancelLabel);
        Theme::styleNeutralButton(confirmButton);
        Theme::styleRecButton(cancelButton);

        confirmButton.onClick = [this] { finish(true); };
        cancelButton.onClick = [this] { finish(false); };

        setSize(kDialogWidth, dialogHeight);
    }

    int getDialogHeight() const { return dialogHeight; }

    void paint(juce::Graphics& g) override
    {
        Theme::drawCard(g, getLocalBounds(), juce::String());
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced(kPadH, kPadV);

        titleLabel.setBounds(r.removeFromTop(kTitleH));
        r.removeFromTop(kGapAfterTitle);
        messageLabel.setBounds(r.removeFromTop(r.getHeight() - kGapBeforeButtons - kButtonRowH));
        r.removeFromTop(kGapBeforeButtons);

        auto row = r.removeFromTop(kButtonRowH);
        const int gap = 10;
        const int buttonW = (row.getWidth() - gap) / 2;
        confirmButton.setBounds(row.removeFromLeft(buttonW));
        row.removeFromLeft(gap);
        cancelButton.setBounds(row);
    }

    bool keyPressed(const juce::KeyPress& key) override
    {
        if (key == juce::KeyPress::escapeKey)
        {
            finish(false);
            return true;
        }
        if (key == juce::KeyPress::returnKey)
        {
            finish(false);
            return true;
        }
        return juce::Component::keyPressed(key);
    }

private:
    juce::Label titleLabel;
    juce::Label messageLabel;
    juce::TextButton confirmButton;
    juce::TextButton cancelButton;
    int dialogHeight = 168;

    void finish(bool confirmed)
    {
        if (onResult)
            onResult(confirmed);
    }
};

class ConfirmDialogModal : public juce::Component
{
public:
    std::function<void(bool)> onResult;

    ConfirmDialogModal(const ConfirmDialogOptions& options)
        : dialog(options)
    {
        dialog.onResult = [this](bool confirmed) {
            if (onResult)
                onResult(confirmed);
            exitModalState(confirmed ? 1 : 0);
        };

        addAndMakeVisible(dialog);
        setWantsKeyboardFocus(true);
    }

    void paint(juce::Graphics& g) override
    {
        g.fillAll(juce::Colours::black.withAlpha(0.55f));
    }

    void resized() override
    {
        dialog.setBounds(getLocalBounds().withSizeKeepingCentre(dialog.getWidth(), dialog.getDialogHeight()));
    }

    bool keyPressed(const juce::KeyPress& key) override
    {
        return dialog.keyPressed(key) || juce::Component::keyPressed(key);
    }

private:
    ConfirmDialogPanel dialog;
};

} // namespace

void showConfirmDialog(juce::Component* host,
                       const ConfirmDialogOptions& options,
                       std::function<void(bool confirmed)> onResult)
{
    if (host == nullptr)
        return;

    auto* top = host->getTopLevelComponent();
    if (top == nullptr)
        return;

    auto* modal = new ConfirmDialogModal(options);
    modal->onResult = std::move(onResult);

    top->addAndMakeVisible(modal);
    modal->setBounds(top->getLocalBounds());
    modal->toFront(true);
    modal->grabKeyboardFocus();
    modal->enterModalState(true,
                            juce::ModalCallbackFunction::create([modal](int) { delete modal; }),
                            true);
}

} // namespace cassette::ui
