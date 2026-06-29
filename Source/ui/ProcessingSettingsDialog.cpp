#include "ProcessingSettingsDialog.h"
#include "ProcessingOptionsPanel.h"
#include "UiTheme.h"
#include "../util/AppLocale.h"

#if CASSETTE_ENABLE_PI_TAPE
#include "PiTapeSettingsPanel.h"
#endif

namespace cassette::ui
{

namespace
{

constexpr int kDialogWidth = 420;
constexpr int kPadH = 24;
constexpr int kPadV = 20;
constexpr int kTitleH = 28;

#if CASSETTE_ENABLE_PI_TAPE

class AppSettingsDialogPanel : public juce::Component
{
public:
    std::function<void(AppSettingsState)> onDone;

    explicit AppSettingsDialogPanel(const AppSettingsState& current)
    {
        addAndMakeVisible(titleLabel);
        titleLabel.setComponentID("settings-dialog-title");
        titleLabel.setPaintingIsUnclipped(true);
        Theme::applyLabel(titleLabel, Theme::titleFont(), Theme::textPrimary(), juce::Justification::centred);

        addAndMakeVisible(optionsPanel);
        optionsPanel.setDialogMode(true);
        optionsPanel.setOptions(current.mastering);

        addAndMakeVisible(piTapePanel);
        piTapePanel.setSettings(current.piTape);

        addAndMakeVisible(doneButton);
        Theme::styleAccentButton(doneButton);
        doneButton.onClick = [this] { finish(); };

        refreshLocalisedText();
        const int bodyH = optionsPanel.getPreferredHeight() + 12 + piTapePanel.getPreferredHeight();
        setSize(kDialogWidth, kPadV * 2 + kTitleH + 12 + bodyH + 14 + 36);
    }

    void paint(juce::Graphics& g) override
    {
        Theme::drawCard(g, getLocalBounds(), juce::String());
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced(kPadH, kPadV);

        titleLabel.setBounds(r.removeFromTop(kTitleH));
        r.removeFromTop(12);

        const int optionsH = optionsPanel.getPreferredHeight();
        optionsPanel.setBounds(r.removeFromTop(optionsH));
        r.removeFromTop(12);

        piTapePanel.setBounds(r.removeFromTop(piTapePanel.getPreferredHeight()));
        r.removeFromTop(14);
        doneButton.setBounds(r.removeFromTop(36));
    }

    bool keyPressed(const juce::KeyPress& key) override
    {
        if (key == juce::KeyPress::escapeKey || key == juce::KeyPress::returnKey)
        {
            finish();
            return true;
        }
        return juce::Component::keyPressed(key);
    }

private:
    juce::Label titleLabel;
    cassette::ProcessingOptionsPanel optionsPanel;
    cassette::PiTapeSettingsPanel piTapePanel;
    juce::TextButton doneButton;

    void refreshLocalisedText()
    {
        titleLabel.setText(tr("settings.title"), juce::dontSendNotification);
        doneButton.setButtonText(tr("btn.done"));
        optionsPanel.refreshLocalisedText();
        piTapePanel.refreshLocalisedText();
    }

    void finish()
    {
        if (onDone)
        {
            AppSettingsState state;
            state.mastering = optionsPanel.getOptions();
            state.piTape = piTapePanel.getSettings();
            onDone(std::move(state));
        }
    }
};

class AppSettingsDialogModal : public juce::Component
{
public:
    explicit AppSettingsDialogModal(const AppSettingsState& current)
        : panel(current)
    {
        panel.onDone = [this](AppSettingsState state) {
            if (onDone)
                onDone(std::move(state));
            exitModalState(1);
        };
        addAndMakeVisible(panel);
        setWantsKeyboardFocus(true);
    }

    std::function<void(AppSettingsState)> onDone;

    void paint(juce::Graphics& g) override
    {
        g.fillAll(juce::Colours::black.withAlpha(0.55f));
    }

    void resized() override
    {
        panel.setBounds(getLocalBounds().withSizeKeepingCentre(panel.getWidth(), panel.getHeight()));
    }

    bool keyPressed(const juce::KeyPress& key) override
    {
        return panel.keyPressed(key) || juce::Component::keyPressed(key);
    }

private:
    AppSettingsDialogPanel panel;
};

#else

class AppSettingsDialogPanel : public juce::Component
{
public:
    std::function<void(MasteringOptions)> onDone;

    explicit AppSettingsDialogPanel(const MasteringOptions& current)
    {
        addAndMakeVisible(titleLabel);
        titleLabel.setComponentID("settings-dialog-title");
        titleLabel.setPaintingIsUnclipped(true);
        Theme::applyLabel(titleLabel, Theme::titleFont(), Theme::textPrimary(), juce::Justification::centred);

        addAndMakeVisible(optionsPanel);
        optionsPanel.setDialogMode(true);
        optionsPanel.setOptions(current);

        addAndMakeVisible(doneButton);
        Theme::styleAccentButton(doneButton);
        doneButton.onClick = [this] { finish(); };

        refreshLocalisedText();
        const int optionsH = optionsPanel.getPreferredHeight();
        setSize(kDialogWidth, kPadV * 2 + kTitleH + 12 + optionsH + 14 + 36);
    }

    void paint(juce::Graphics& g) override
    {
        Theme::drawCard(g, getLocalBounds(), juce::String());
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced(kPadH, kPadV);

        titleLabel.setBounds(r.removeFromTop(kTitleH));
        r.removeFromTop(12);

        const int optionsH = optionsPanel.getPreferredHeight();
        optionsPanel.setBounds(r.removeFromTop(optionsH));
        r.removeFromTop(14);
        doneButton.setBounds(r.removeFromTop(36));
    }

    bool keyPressed(const juce::KeyPress& key) override
    {
        if (key == juce::KeyPress::escapeKey || key == juce::KeyPress::returnKey)
        {
            finish();
            return true;
        }
        return juce::Component::keyPressed(key);
    }

private:
    juce::Label titleLabel;
    cassette::ProcessingOptionsPanel optionsPanel;
    juce::TextButton doneButton;

    void refreshLocalisedText()
    {
        titleLabel.setText(tr("settings.title"), juce::dontSendNotification);
        doneButton.setButtonText(tr("btn.done"));
        optionsPanel.refreshLocalisedText();
    }

    void finish()
    {
        if (onDone)
            onDone(optionsPanel.getOptions());
    }
};

class AppSettingsDialogModal : public juce::Component
{
public:
    explicit AppSettingsDialogModal(const MasteringOptions& current)
        : panel(current)
    {
        panel.onDone = [this](MasteringOptions options) {
            if (onDone)
                onDone(std::move(options));
            exitModalState(1);
        };
        addAndMakeVisible(panel);
        setWantsKeyboardFocus(true);
    }

    std::function<void(MasteringOptions)> onDone;

    void paint(juce::Graphics& g) override
    {
        g.fillAll(juce::Colours::black.withAlpha(0.55f));
    }

    void resized() override
    {
        panel.setBounds(getLocalBounds().withSizeKeepingCentre(panel.getWidth(), panel.getHeight()));
    }

    bool keyPressed(const juce::KeyPress& key) override
    {
        return panel.keyPressed(key) || juce::Component::keyPressed(key);
    }

private:
    AppSettingsDialogPanel panel;
};

#endif

}

#if CASSETTE_ENABLE_PI_TAPE

void showAppSettingsDialog(juce::Component* host,
                           const AppSettingsState& current,
                           std::function<void(AppSettingsState)> onDone)
{
    if (host == nullptr)
        return;

    auto* top = host->getTopLevelComponent();
    if (top == nullptr)
        return;

    auto* modal = new AppSettingsDialogModal(current);
    modal->onDone = std::move(onDone);

    top->addAndMakeVisible(modal);
    modal->setBounds(top->getLocalBounds());
    modal->toFront(true);
    modal->grabKeyboardFocus();
    modal->enterModalState(true,
                            juce::ModalCallbackFunction::create([modal](int) { delete modal; }),
                            true);
}

#else

void showAppSettingsDialog(juce::Component* host,
                           const MasteringOptions& current,
                           std::function<void(MasteringOptions)> onDone)
{
    if (host == nullptr)
        return;

    auto* top = host->getTopLevelComponent();
    if (top == nullptr)
        return;

    auto* modal = new AppSettingsDialogModal(current);
    modal->onDone = std::move(onDone);

    top->addAndMakeVisible(modal);
    modal->setBounds(top->getLocalBounds());
    modal->toFront(true);
    modal->grabKeyboardFocus();
    modal->enterModalState(true,
                            juce::ModalCallbackFunction::create([modal](int) { delete modal; }),
                            true);
}

#endif

}
