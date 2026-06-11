#include "WowFlutterAnalyzer.h"
#include <juce_dsp/juce_dsp.h>
#include <cmath>
#include <numeric>
#include <vector>

namespace cassette
{

namespace
{

constexpr int kFftOrder = 11;
constexpr int kFftSize = 1 << kFftOrder;

void makeMonoMix(const juce::AudioBuffer<float>& buffer, std::vector<float>& mono)
{
    const int n = buffer.getNumSamples();
    mono.resize(static_cast<size_t>(n));
    std::fill(mono.begin(), mono.end(), 0.0f);

    const int ch = buffer.getNumChannels();
    for (int c = 0; c < ch; ++c)
    {
        const auto* d = buffer.getReadPointer(c);
        for (int i = 0; i < n; ++i)
            mono[static_cast<size_t>(i)] += d[i] / static_cast<float>(ch);
    }
}

void computeModulationSpectrum(const std::vector<float>& envelope,
                               double envelopeSampleRate,
                               float& wowEnergy,
                               float& flutterEnergy,
                               float& totalEnergy)
{
    wowEnergy = flutterEnergy = totalEnergy = 0.0f;
    if (envelope.size() < static_cast<size_t>(kFftSize))
        return;

    juce::dsp::FFT fft(kFftOrder);
    std::vector<float> fftData(static_cast<size_t>(kFftSize * 2), 0.0f);

    const int copyN = juce::jmin(kFftSize, static_cast<int>(envelope.size()));
    for (int i = 0; i < copyN; ++i)
        fftData[static_cast<size_t>(i)] = envelope[static_cast<size_t>(i)];

    for (int i = 0; i < copyN; ++i)
    {
        const auto w = 0.5f * (1.0f - std::cos(2.0f * juce::MathConstants<float>::pi * static_cast<float>(i)
                                               / static_cast<float>(juce::jmax(1, copyN - 1))));
        fftData[static_cast<size_t>(i)] *= w;
    }

    fft.performRealOnlyForwardTransform(fftData.data());

    const float binHz = static_cast<float>(envelopeSampleRate) / static_cast<float>(kFftSize);
    const int maxBin = kFftSize / 2;

    for (int bin = 1; bin < maxBin; ++bin)
    {
        const float re = fftData[static_cast<size_t>(bin * 2)];
        const float im = fftData[static_cast<size_t>(bin * 2 + 1)];
        const float mag = re * re + im * im;
        const float hz = bin * binHz;

        totalEnergy += mag;
        if (hz >= 0.4f && hz < 6.5f)
            wowEnergy += mag;
        else if (hz >= 6.5f && hz <= 22.0f)
            flutterEnergy += mag;
    }
}

std::vector<float> buildRmsEnvelope(const std::vector<float>& mono, double sampleRate, double windowSec, int& hopOut)
{
    const int win = juce::jmax(64, static_cast<int>(windowSec * sampleRate));
    const int hop = juce::jmax(32, win / 4);
    hopOut = hop;
    std::vector<float> env;

    for (int start = 0; start + win <= static_cast<int>(mono.size()); start += hop)
    {
        double sum = 0.0;
        for (int i = 0; i < win; ++i)
        {
            const auto s = mono[static_cast<size_t>(start + i)];
            sum += s * s;
        }
        env.push_back(static_cast<float>(std::sqrt(sum / static_cast<double>(win))));
    }

    if (env.size() < 4)
        return env;

    const float mean = std::accumulate(env.begin(), env.end(), 0.0f) / static_cast<float>(env.size());
    for (auto& v : env)
        v = mean > 1.0e-8f ? (v / mean - 1.0f) : 0.0f;

    return env;
}

float bandpass3150EnvelopeModulation(const std::vector<float>& mono, double sampleRate, bool& toneFound)
{
    toneFound = false;
    const int n = static_cast<int>(mono.size());
    if (n < 4096)
        return 0.0f;

    const float omega = 2.0f * juce::MathConstants<float>::pi * 3150.0f / static_cast<float>(sampleRate);
    const float c = std::cos(omega);
    const float coeff = 2.0f * c;

    float y1 = 0.0f, y2 = 0.0f;
    std::vector<float> band(static_cast<size_t>(n), 0.0f);
    float bandEnergy = 0.0f;

    for (int i = 0; i < n; ++i)
    {
        const float x = mono[static_cast<size_t>(i)];
        const float y = coeff * y1 - y2 + x;
        y2 = y1;
        y1 = y;
        band[static_cast<size_t>(i)] = y;
        bandEnergy += y * y;
    }

    bandEnergy = std::sqrt(bandEnergy / static_cast<double>(n));
    if (bandEnergy < 0.008f)
        return 0.0f;

    toneFound = true;

    std::vector<float> env;
    const int win = juce::jmax(128, static_cast<int>(0.02 * sampleRate));
    const int hop = win / 2;
    for (int start = 0; start + win <= n; start += hop)
    {
        float peak = 0.0f;
        for (int i = 0; i < win; ++i)
            peak = juce::jmax(peak, std::abs(band[static_cast<size_t>(start + i)]));
        env.push_back(peak);
    }

    if (env.size() < static_cast<size_t>(kFftSize))
        return 0.0f;

    const float mean = std::accumulate(env.begin(), env.end(), 0.0f) / static_cast<float>(env.size());
    for (auto& v : env)
        v = mean > 1.0e-8f ? (v / mean - 1.0f) : 0.0f;

    const double envSr = sampleRate / static_cast<double>(hop);
    float wowE = 0.0f, flutterE = 0.0f, totalE = 0.0f;
    computeModulationSpectrum(env, envSr, wowE, flutterE, totalE);

    if (totalE < 1.0e-12f)
        return 0.0f;

    return 100.0f * std::sqrt((wowE + flutterE) / totalE);
}

}

WowFlutterMetrics WowFlutterAnalyzer::analyze(const juce::AudioBuffer<float>& buffer, double sampleRate)
{
    WowFlutterMetrics m;
    if (buffer.getNumSamples() == 0 || sampleRate <= 0.0)
        return m;

    std::vector<float> mono;
    makeMonoMix(buffer, mono);

    int hopSamples = 0;
    const auto env = buildRmsEnvelope(mono, sampleRate, 0.08, hopSamples);
    if (env.size() >= static_cast<size_t>(kFftSize))
    {
        const double envSr = sampleRate / static_cast<double>(juce::jmax(1, hopSamples));

        float wowE = 0.0f, flutterE = 0.0f, totalE = 0.0f;
        computeModulationSpectrum(env, envSr, wowE, flutterE, totalE);

        if (totalE > 1.0e-12f)
        {
            m.wowPercent = 100.0f * std::sqrt(wowE / totalE);
            m.flutterPercent = 100.0f * std::sqrt(flutterE / totalE);
        }
    }
    else if (env.size() >= 8)
    {
        const float mean = std::accumulate(env.begin(), env.end(), 0.0f) / static_cast<float>(env.size());
        float var = 0.0f;
        for (float v : env)
        {
            const float d = v - mean;
            var += d * d;
        }
        var /= static_cast<float>(env.size());
        if (mean > 1.0e-8f)
            m.wowPercent = 100.0f * std::sqrt(var) / mean;
    }

    m.testToneWowPercent = bandpass3150EnvelopeModulation(mono, sampleRate, m.hasTestTone3150);
    if (m.hasTestTone3150 && m.testToneWowPercent > m.wowPercent)
        m.wowPercent = m.testToneWowPercent;

    m.wowFlutterIndex = juce::jlimit(0.0f,
                                     1.0f,
                                     m.wowPercent * 0.035f + m.flutterPercent * 0.025f);

    return m;
}

}
