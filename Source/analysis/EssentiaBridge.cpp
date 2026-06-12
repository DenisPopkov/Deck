#include "EssentiaBridge.h"

#if defined(CASSETTE_HAS_ESSENTIA)
#include <essentia/algorithmfactory.h>
#include <essentia/essentia.h>
#include <essentia/essentiamath.h>
#include <mutex>
#include <vector>
#endif

namespace cassette
{
namespace
{
#if defined(CASSETTE_HAS_ESSENTIA)
struct EssentiaRuntime
{
    EssentiaRuntime() { essentia::init(); }
    ~EssentiaRuntime() { essentia::shutdown(); }
};

EssentiaRuntime& essentiaRuntime()
{
    static EssentiaRuntime runtime;
    return runtime;
}

juce::CriticalSection essentiaLock;

std::vector<essentia::Real> toMonoVector(const juce::AudioBuffer<float>& buffer)
{
    const int n = buffer.getNumSamples();
    const int ch = buffer.getNumChannels();
    std::vector<essentia::Real> mono(static_cast<size_t>(n), 0.0);

    for (int i = 0; i < n; ++i)
    {
        double sum = 0.0;
        for (int c = 0; c < ch; ++c)
            sum += buffer.getSample(c, i);
        mono[static_cast<size_t>(i)] = sum / static_cast<double>(juce::jmax(1, ch));
    }

    return mono;
}
#endif
}

bool EssentiaBridge::isLinked()
{
#if defined(CASSETTE_HAS_ESSENTIA)
    return true;
#else
    return false;
#endif
}

std::optional<float> EssentiaBridge::integratedLufs(const juce::AudioBuffer<float>& buffer, double sampleRate)
{
#if defined(CASSETTE_HAS_ESSENTIA)
    if (buffer.getNumSamples() == 0 || sampleRate <= 0.0)
        return std::nullopt;

    juce::ignoreUnused(essentiaRuntime());

    const juce::ScopedLock sl(essentiaLock);

    try
    {
        using namespace essentia;
        using namespace essentia::standard;

        AlgorithmFactory& factory = AlgorithmFactory::instance();
        Algorithm* loudness = factory.create("LoudnessEBUR128");

        const auto mono = toMonoVector(buffer);
        loudness->input("signal").set(mono);
        loudness->input("sampleRate").set(static_cast<Real>(sampleRate));

        std::vector<Real> momentary;
        std::vector<Real> shortTerm;
        Real integrated = 0.0;
        Real lra = 0.0;

        loudness->output("momentaryLoudness").set(momentary);
        loudness->output("shortTermLoudness").set(shortTerm);
        loudness->output("integratedLoudness").set(integrated);
        loudness->output("loudnessRange").set(lra);
        loudness->compute();
        delete loudness;

        if (!std::isfinite(integrated))
            return std::nullopt;

        return static_cast<float>(integrated);
    }
    catch (...)
    {
        return std::nullopt;
    }
#else
    juce::ignoreUnused(buffer, sampleRate);
    return std::nullopt;
#endif
}

}
