#include "RoughnessDeEsser.h"
#include <juce_dsp/juce_dsp.h>
#include <cmath>

namespace cassette
{

namespace
{

using Filter = juce::dsp::ProcessorDuplicator<juce::dsp::IIR::Filter<float>,
                                              juce::dsp::IIR::Coefficients<float>>;

void filterBand(juce::AudioBuffer<float>& band, double sampleRate, float loHz, float hiHz)
{
    Filter hpf, lpf;
    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = static_cast<juce::uint32>(band.getNumSamples());
    spec.numChannels = static_cast<juce::uint32>(band.getNumChannels());

    *hpf.state = *juce::dsp::IIR::Coefficients<float>::makeHighPass(sampleRate, loHz, 0.707f);
    *lpf.state = *juce::dsp::IIR::Coefficients<float>::makeLowPass(sampleRate, hiHz, 0.707f);
    hpf.prepare(spec);
    lpf.prepare(spec);

    juce::dsp::AudioBlock<float> block(band);
    juce::dsp::ProcessContextReplacing<float> ctx(block);
    hpf.process(ctx);
    lpf.process(ctx);
}

}

void RoughnessDeEsser::processBand(juce::AudioBuffer<float>& buffer,
                                   float centerHz,
                                   float modulationIndex,
                                   double sampleRate,
                                   float intensity)
{
    const float t = juce::jlimit(0.0f, 1.0f, (modulationIndex - 0.18f) / 0.55f) * intensity;
    if (t < 0.05f)
        return;

    const float bw = centerHz / 2.8f;
    const float lo = juce::jmax(80.0f, centerHz - bw);
    const float hi = juce::jmin(static_cast<float>(sampleRate * 0.45), centerHz + bw);

    juce::AudioBuffer<float> band;
    band.makeCopyOf(buffer);
    filterBand(band, sampleRate, lo, hi);

    const float cutDb = -juce::jmin(4.5f, t * 4.2f * modulationIndex);
    const float bandGain = juce::Decibels::decibelsToGain(cutDb);

    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
    {
        const auto* b = band.getReadPointer(ch);
        auto* out = buffer.getWritePointer(ch);
        for (int i = 0; i < buffer.getNumSamples(); ++i)
            out[i] += b[i] * (bandGain - 1.0f);
    }
}

void RoughnessDeEsser::process(juce::AudioBuffer<float>& buffer,
                               const PsychoacousticMetrics& metrics,
                               double sampleRate,
                               float intensity)
{
    if (intensity < 0.02f || buffer.getNumSamples() == 0)
        return;

    const float global = juce::jlimit(0.0f, 1.0f, metrics.streamingRingingIndex * intensity);
    if (global < 0.08f && metrics.roughnessAsper < 0.15f)
        return;

    for (int b = 0; b < PsychoacousticMetrics::kModulationBands; ++b)
    {
        const auto mi = metrics.bandModulationIndex[static_cast<size_t>(b)];
        if (mi < 0.22f)
            continue;

        const auto bark = (static_cast<float>(b) + 0.5f) * 24.0f
                          / static_cast<float>(PsychoacousticMetrics::kModulationBands);
        const auto centerHz = PsychoacousticMetrics::barkToHz(bark);
        if (centerHz < 1800.0f || centerHz > 14000.0f)
            continue;

        processBand(buffer, centerHz, mi * global, sampleRate, intensity);
    }
}

}
