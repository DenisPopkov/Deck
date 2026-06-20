#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../io/AudioFileLoader.h"

namespace cassette
{

inline int importFolderChooserFlags()
{
#if JUCE_WINDOWS
    // Windows folder-picker mode hides files; use a normal open dialog instead.
    return juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles;
#else
    return juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles
         | juce::FileBrowserComponent::canSelectDirectories;
#endif
}

inline juce::File folderFromImportPickerResult(const juce::File& picked)
{
    if (picked.isDirectory())
        return picked;

    if (picked.existsAsFile())
        return picked.getParentDirectory();

    return {};
}

}
