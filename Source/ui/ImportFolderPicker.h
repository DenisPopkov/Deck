#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../io/AudioFileLoader.h"

namespace cassette
{

inline int importFolderChooserFlags()
{
#if JUCE_WINDOWS
    // Native folder picker: choose a directory, not a single file.
    return juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories;
#else
    // Finder / zenity: show audio files while browsing, still allow folder selection.
    return juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles
         | juce::FileBrowserComponent::canSelectDirectories;
#endif
}

inline juce::String importFolderChooserWildcard()
{
#if JUCE_WINDOWS
    return {};
#else
    return AudioFileLoader::importFileWildcard();
#endif
}

inline juce::File folderFromImportPickerResult(const juce::File& picked)
{
    return picked.isDirectory() ? picked : juce::File();
}

}
