#include "SpectrumAnalyzer.h"
#include <juce_dsp/juce_dsp.h>
#include <cmath>

namespace cassette
{

SpectrumAnalyzer::BandRatios SpectrumAnalyzer::bandEnergyRatios(const juce::AudioBuffer<float>& buffer,
                                                                double sampleRate)
{
    BandRatios ratios;
    const int n = buffer.getNumSamples();
    if (n < 64 || buffer.getNumChannels() == 0 || sampleRate <= 0.0)
        return ratios;

    const int fftOrder = juce::jlimit(6, 14, static_cast<int>(std::ceil(std::log2(static_cast<double>(n)))));
    const int fftSize = 1 << fftOrder;
    juce::dsp::FFT fft(fftOrder);

    std::vector<float> mono(static_cast<size_t>(fftSize), 0.0f);
    const int copyN = juce::jmin(n, fftSize);
    const int ch = buffer.getNumChannels();

    for (int i = 0; i < copyN; ++i)
    {
        float sum = 0.0f;
        for (int c = 0; c < ch; ++c)
            sum += buffer.getSample(c, i);
        mono[static_cast<size_t>(i)] = sum / static_cast<float>(ch);
    }

    for (int i = 0; i < fftSize; ++i)
    {
        const float w = 0.5f * (1.0f - std::cos(juce::MathConstants<float>::twoPi * static_cast<float>(i)
                                                / static_cast<float>(juce::jmax(1, fftSize - 1))));
        mono[static_cast<size_t>(i)] *= w;
    }

    std::vector<float> fftData(static_cast<size_t>(fftSize * 2), 0.0f);
    std::copy(mono.begin(), mono.end(), fftData.begin());
    fft.performFrequencyOnlyForwardTransform(fftData.data());

    const float binHz = static_cast<float>(sampleRate) / static_cast<float>(fftSize);
    const float lfMax = 150.0f;
    const float mfMax = 4000.0f;

    double eLf = 0.0, eMf = 0.0, eHf = 0.0, eTotal = 0.0;
    const int halfBins = fftSize / 2;

    for (int bin = 1; bin < halfBins; ++bin)
    {
        const float hz = static_cast<float>(bin) * binHz;
        const float mag = fftData[static_cast<size_t>(bin)];
        const double e = static_cast<double>(mag) * mag;

        eTotal += e;
        if (hz < lfMax)
            eLf += e;
        else if (hz < mfMax)
            eMf += e;
        else
            eHf += e;
    }

    if (eTotal < 1.0e-20)
        return ratios;

    ratios.lf = static_cast<float>(eLf / eTotal);
    ratios.mf = static_cast<float>(eMf / eTotal);
    ratios.hf = static_cast<float>(eHf / eTotal);
    return ratios;
}

}
