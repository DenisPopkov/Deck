#include "AudioFileLoader.h"

namespace cassette
{

namespace
{

const juce::StringArray& supportedAudioExtensions()
{
    static const juce::StringArray extensions { "wav", "flac", "aiff", "aif", "ogg", "mp3" };
    return extensions;
}

bool isHiddenFileName(const juce::String& fileName)
{
    return fileName.isNotEmpty() && fileName.startsWithChar('.');
}

bool isKnownNonAudioExtension(const juce::String& extension)
{
    if (extension.isEmpty())
        return false;

    const auto bare = extension.startsWithChar('.') ? extension.substring(1) : extension;
    static const juce::StringArray blocked {
        "heic", "heif", "jpg", "jpeg", "png", "gif", "webp", "tif", "tiff", "bmp",
        "zip", "rar", "7z", "gz", "tar", "dmg", "pkg", "app", "exe", "msi", "deb",
        "pdf", "doc", "docx", "xls", "xlsx", "ppt", "pptx", "txt", "md", "json",
        "xml", "html", "css", "js", "ts", "mov", "mp4", "m4v", "avi", "mkv"
    };
    return blocked.contains(bare, true);
}

bool isSupportedAudioExtension(const juce::String& extension)
{
    if (extension.isEmpty())
        return false;

    const auto bare = extension.startsWithChar('.') ? extension.substring(1) : extension;
    if (isKnownNonAudioExtension(bare))
        return false;

    return supportedAudioExtensions().contains(bare, true);
}

} // namespace

juce::AudioFormatManager& AudioFileLoader::getFormatManager()
{
    static juce::AudioFormatManager fm;
    static bool registered = false;
    if (!registered)
    {
        fm.registerBasicFormats();
        registered = true;
    }
    return fm;
}

juce::String AudioFileLoader::importFileWildcard()
{
    return "*.wav;*.flac;*.aiff;*.aif;*.ogg;*.mp3";
}

juce::String AudioFileLoader::supportedFormatsLabel()
{
    return "WAV, FLAC, AIFF, OGG, MP3";
}

juce::File AudioFileLoader::normaliseDroppedPath(const juce::String& path)
{
    auto trimmed = path.trim();
    if (trimmed.isEmpty())
        return {};

    if (trimmed.startsWithIgnoreCase("file://"))
        return juce::URL(trimmed).getLocalFile();

    return juce::File(trimmed);
}

bool AudioFileLoader::isSupportedAudioFile(const juce::File& file)
{
    if (!file.existsAsFile())
        return false;

    if (isHiddenFileName(file.getFileName()))
        return false;

    const auto ext = file.getFileExtension();
    if (!isSupportedAudioExtension(ext))
        return false;

    return getFormatManager().findFormatForFileExtension(ext) != nullptr;
}

bool AudioFileLoader::isSupportedAudioFileDrop(const juce::StringArray& paths)
{
    for (const auto& p : paths)
        if (isSupportedAudioFile(normaliseDroppedPath(p)))
            return true;

    return false;
}

juce::File AudioFileLoader::pickFirstAudioFile(const juce::StringArray& paths)
{
    for (const auto& p : paths)
    {
        const auto f = normaliseDroppedPath(p);
        if (isSupportedAudioFile(f))
            return f;
    }
    return {};
}

double AudioFileLoader::probeDurationSec(const juce::File& file)
{
    if (!isSupportedAudioFile(file))
        return 0.0;

    auto in = file.createInputStream();
    if (in == nullptr)
        return 0.0;

    auto& fm = getFormatManager();
    std::unique_ptr<juce::AudioFormatReader> reader(
        fm.createReaderFor(std::unique_ptr<juce::InputStream>(in.release())));
    if (reader == nullptr)
        return 0.0;

    if (reader->sampleRate <= 0.0 || reader->numChannels <= 0 || reader->lengthInSamples <= 0)
        return 0.0;

    return static_cast<double>(reader->lengthInSamples) / reader->sampleRate;
}

juce::Optional<LoadedAudio> AudioFileLoader::loadToBuffer(const juce::File& file)
{
    return loadToBufferWithDiagnostics(file).audio;
}

LoadResult AudioFileLoader::loadToBufferWithDiagnostics(const juce::File& file)
{
    LoadResult result;

    if (!file.existsAsFile())
    {
        result.error = "File not found: " + file.getFullPathName();
        return result;
    }

    if (!isSupportedAudioFile(file))
    {
        result.error = "Unsupported format (use " + supportedFormatsLabel() + ")";
        return result;
    }

    auto& fm = getFormatManager();
    std::unique_ptr<juce::AudioFormatReader> reader;

    if (auto in = file.createInputStream(); in != nullptr)
        reader.reset(fm.createReaderFor(std::move(in)));

    if (reader == nullptr)
        reader.reset(fm.createReaderFor(file));

    if (reader == nullptr)
    {
        result.error = "Could not open decoder for: " + file.getFileName();
        return result;
    }

    if (reader->lengthInSamples <= 0)
    {
        result.error = "File is empty or unreadable";
        return result;
    }

    LoadedAudio out;
    out.sampleRate = reader->sampleRate;
    const auto numCh = static_cast<int>(reader->numChannels);
    const auto numSamples = static_cast<int>(reader->lengthInSamples);

    out.buffer.setSize(numCh, numSamples);
    if (!reader->read(&out.buffer, 0, numSamples, 0, true, true))
    {
        result.error = "Failed while reading audio samples";
        return result;
    }

    result.audio = std::move(out);
    return result;
}

}
