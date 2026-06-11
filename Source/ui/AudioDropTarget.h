#pragma once

#include <juce_gui_extra/juce_gui_extra.h>
#include <functional>

namespace cassette
{

class MainComponent;

enum class DropPayloadKind
{
    None,
    AudioFile,
    Folder
};

class AudioDropForwarder : public juce::FileDragAndDropTarget
{
public:
    AudioDropForwarder() = default;

    std::function<void(DropPayloadKind)> onDragHoverChanged;

protected:
    void initAudioDropForwarding(juce::Component* self);

private:
    juce::Component* owner = nullptr;
    int dragDepth = 0;

    bool isInterestedInFileDrag(const juce::StringArray& files) override;
    void fileDragEnter(const juce::StringArray& files, int x, int y) override;
    void fileDragExit(const juce::StringArray& files) override;
    void filesDropped(const juce::StringArray& files, int x, int y) override;

    MainComponent* findMain() const;
    static DropPayloadKind payloadKind(const juce::StringArray& files);
};

}
