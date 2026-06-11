#include "AudioDropTarget.h"
#include "MainComponent.h"
#include "../io/AudioFileLoader.h"

namespace cassette
{

void AudioDropForwarder::initAudioDropForwarding(juce::Component* self)
{
    owner = self;
}

MainComponent* AudioDropForwarder::findMain() const
{
    return owner != nullptr ? owner->findParentComponentOfClass<MainComponent>() : nullptr;
}

DropPayloadKind AudioDropForwarder::payloadKind(const juce::StringArray& files)
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

bool AudioDropForwarder::isInterestedInFileDrag(const juce::StringArray& files)
{
    if (auto* main = findMain())
        return main->isInterestedInDrop(files);
    return false;
}

void AudioDropForwarder::fileDragEnter(const juce::StringArray& files, int, int)
{
    if (!isInterestedInFileDrag(files))
        return;

    ++dragDepth;
    if (onDragHoverChanged)
        onDragHoverChanged(payloadKind(files));
}

void AudioDropForwarder::fileDragExit(const juce::StringArray&)
{
    if (dragDepth <= 0)
        return;

    --dragDepth;
    if (dragDepth == 0 && onDragHoverChanged)
        onDragHoverChanged(DropPayloadKind::None);
}

void AudioDropForwarder::filesDropped(const juce::StringArray& files, int x, int y)
{
    dragDepth = 0;
    if (onDragHoverChanged)
        onDragHoverChanged(DropPayloadKind::None);

    if (auto* main = findMain())
        main->handleAudioFilesDropped(files, x, y);
}

}
