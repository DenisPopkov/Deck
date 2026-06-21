#pragma once

#include "AudioFileLoader.h"
#include <juce_core/juce_core.h>

namespace cassette
{

enum class DropPayloadKind
{
    None,
    AudioFile,
    Folder
};

inline DropPayloadKind classifyDropPayload(const juce::StringArray& files)
{
    for (const auto& path : files)
    {
        if (AudioFileLoader::normaliseDroppedPath(path).isDirectory())
            return DropPayloadKind::Folder;
    }

    if (AudioFileLoader::isSupportedAudioFileDrop(files))
        return DropPayloadKind::AudioFile;

    return DropPayloadKind::None;
}

inline bool isDropPayloadInterested(const juce::StringArray& files)
{
    return classifyDropPayload(files) != DropPayloadKind::None;
}

}
