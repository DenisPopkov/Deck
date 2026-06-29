#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../io/AudioFileLoader.h"

namespace cassette
{

inline int importFolderChooserFlags()
{
    return juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories;
}

inline juce::String importFolderChooserWildcard()
{
    return {};
}

inline juce::File folderFromImportPickerResult(const juce::File& picked)
{
    return picked.isDirectory() ? picked : juce::File();
}

}
