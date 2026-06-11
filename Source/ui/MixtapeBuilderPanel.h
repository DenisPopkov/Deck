#pragma once

#include <juce_gui_extra/juce_gui_extra.h>
#include <functional>
#include <optional>
#include "../project/FolderMixBuilder.h"

namespace cassette
{

class MixtapeBuilderPanel : public juce::Component,
                            private juce::Button::Listener
{
public:
    std::function<void(const juce::File&)> onFolderSelected;
    std::function<void()> onBuildClicked;
    std::function<void()> onFolderClear;

    MixtapeBuilderPanel();

    void setFolderScan(const std::optional<FolderScanResult>& scan, const juce::File& folder);
    void clearFolder();
    void setFitReport(const juce::String& text, bool ok);
    void setBuildEnabled(bool enabled);
    void setBusy(bool busy);
    void showFolderPicker();

    juce::File currentFolder() const { return mixFolder; }
    bool hasValidScan() const { return folderScan.has_value() && folderScan->success; }

private:
    void resized() override;
    void paint(juce::Graphics& g) override;
    void buttonClicked(juce::Button* b) override;

    juce::File mixFolder;
    std::optional<FolderScanResult> folderScan;
    bool fitOk = false;

    juce::Label folderLabel { {}, "No folder selected" };
    juce::Label fitLabel { {}, "Pick a folder with tracks" };
    juce::TextButton pickFolderButton { "Open Folder..." };
    juce::TextButton buildButton { "Build Side A/B WAV" };
};

}
