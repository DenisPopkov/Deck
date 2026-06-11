#include "MixtapeBuilderPanel.h"
#include "UiTheme.h"
#include "../io/AudioFileLoader.h"

namespace cassette
{

MixtapeBuilderPanel::MixtapeBuilderPanel()
{
    addAndMakeVisible(folderLabel);
    folderLabel.setColour(juce::Label::textColourId, ui::Theme::textPrimary());
    folderLabel.setFont(ui::Theme::bodyFont());

    addAndMakeVisible(fitLabel);
    fitLabel.setColour(juce::Label::textColourId, ui::Theme::textSecondary());
    fitLabel.setFont(ui::Theme::captionFont());

    addAndMakeVisible(pickFolderButton);
    ui::Theme::styleNeutralButton(pickFolderButton);
    pickFolderButton.addListener(this);

    addAndMakeVisible(buildButton);
    ui::Theme::styleAccentButton(buildButton);
    buildButton.addListener(this);
    buildButton.setEnabled(false);
}

void MixtapeBuilderPanel::showFolderPicker()
{
    pickFolderButton.triggerClick();
}

void MixtapeBuilderPanel::setFolderScan(const std::optional<FolderScanResult>& scan, const juce::File& folder)
{
    folderScan = scan;
    mixFolder = folder;

    if (folder.isDirectory())
        folderLabel.setText("Folder: " + folder.getFileName(), juce::dontSendNotification);
    else
        folderLabel.setText("No folder selected", juce::dontSendNotification);

    if (scan.has_value() && scan->success)
    {
        fitLabel.setText(juce::String(scan->tracks.size()) + " tracks, "
                             + FolderMixBuilder::formatDuration(scan->totalDurationSec),
                         juce::dontSendNotification);
    }
}

void MixtapeBuilderPanel::clearFolder()
{
    folderScan.reset();
    mixFolder = juce::File();
    folderLabel.setText("No folder selected", juce::dontSendNotification);
    fitLabel.setText("Pick a folder with tracks", juce::dontSendNotification);
    fitLabel.setColour(juce::Label::textColourId, ui::Theme::textSecondary());
    buildButton.setEnabled(false);
}

void MixtapeBuilderPanel::setFitReport(const juce::String& text, bool ok)
{
    fitOk = ok;
    fitLabel.setText(text, juce::dontSendNotification);
    fitLabel.setColour(juce::Label::textColourId, ok ? ui::Theme::okGreen() : ui::Theme::warnAmber());
}

void MixtapeBuilderPanel::setBuildEnabled(bool enabled)
{
    buildButton.setEnabled(enabled && fitOk && hasValidScan());
}

void MixtapeBuilderPanel::setBusy(bool busy)
{
    pickFolderButton.setEnabled(!busy);
    buildButton.setEnabled(!busy && fitOk && hasValidScan());
}

void MixtapeBuilderPanel::paint(juce::Graphics& g)
{
    ui::Theme::drawCard(g, getLocalBounds(), "Mixtape Builder");
}

void MixtapeBuilderPanel::resized()
{
    auto r = getLocalBounds().reduced(14).withTrimmedTop(32);
    pickFolderButton.setBounds(r.removeFromTop(32));
    r.removeFromTop(8);
    folderLabel.setBounds(r.removeFromTop(22));
    r.removeFromTop(6);
    fitLabel.setBounds(r.removeFromTop(40));
    r.removeFromTop(10);
    buildButton.setBounds(r.removeFromTop(36));
}

void MixtapeBuilderPanel::buttonClicked(juce::Button* b)
{
    if (b == &pickFolderButton)
    {
        auto chooser = std::make_shared<juce::FileChooser>(
            "Import audio",
            mixFolder.isDirectory() ? mixFolder
                                    : juce::File::getSpecialLocation(juce::File::userMusicDirectory),
            AudioFileLoader::importFileWildcard());

        chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles
                                 | juce::FileBrowserComponent::canSelectDirectories,
                             [this, chooser](const juce::FileChooser& fc) {
                                 const auto picked = fc.getResult();
                                 if (picked.isDirectory() && onFolderSelected)
                                     onFolderSelected(picked);
                             });
    }
    else if (b == &buildButton && onBuildClicked)
    {
        onBuildClicked();
    }
}

}
