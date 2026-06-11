#include "WavExporter.h"

namespace cassette
{

bool WavExporter::writeWav32Float(const juce::File& file, const juce::AudioBuffer<float>& buffer, double sampleRate)
{
    juce::WavAudioFormat wav;

    std::unique_ptr<juce::FileOutputStream> out(file.createOutputStream());
    if (out == nullptr)
        return false;

    std::unique_ptr<juce::AudioFormatWriter> writer(
        wav.createWriterFor(out.release(), sampleRate, (unsigned int)buffer.getNumChannels(), 32, {}, 0));

    if (writer == nullptr)
        return false;

    return writer->writeFromAudioSampleBuffer(buffer, 0, buffer.getNumSamples());
}

}
