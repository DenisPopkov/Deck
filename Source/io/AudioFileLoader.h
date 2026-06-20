#pragma once

#include <juce_audio_formats/juce_audio_formats.h>

namespace cassette
{

struct LoadedAudio
{
    juce::AudioBuffer<float> buffer;
    double sampleRate = 48000.0;
};

struct LoadResult
{
    juce::Optional<LoadedAudio> audio;
    juce::String error;
};

class AudioFileLoader
{
public:
    static juce::AudioFormatManager& getFormatManager();

    static juce::String importFileWildcard();
    static juce::String supportedFormatsLabel();

    static juce::File normaliseDroppedPath(const juce::String& path);

    static bool isSupportedAudioFile(const juce::File& file);

    static bool isSupportedAudioFileDrop(const juce::StringArray& paths);

    static juce::File pickFirstAudioFile(const juce::StringArray& paths);

    static juce::Optional<LoadedAudio> loadToBuffer(const juce::File& file);
    static LoadResult loadToBufferWithDiagnostics(const juce::File& file);
};

}
