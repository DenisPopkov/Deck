#include "PsychoacousticMetrics.h"
#include <juce_dsp/juce_dsp.h>
#include <cmath>

namespace cassette
{

namespace
{

constexpr float kPi = juce::MathConstants<float>::pi;

float terhardtAthDb(float hz)
{
    const auto fKhz = hz / 1000.0f;
    return 3.64f * std::pow(fKhz, -0.8f)
           - 6.5f * std::exp(-0.6f * std::pow(fKhz - 3.3f, 2.0f))
           + 0.001f * std::pow(fKhz, 4.0f);
}

float sharpnessWeight(float bark)
{
    if (bark <= 16.0f)
        return 1.0f;
    return 0.066f * std::exp(0.171f * bark);
}

float maskingSpreadDb(float deltaBark)
{
    if (deltaBark <= 0.0f)
        return 25.0f * deltaBark;
    return -17.0f * deltaBark;
}

int barkBandIndex(float bark)
{
    return juce::jlimit(0, PsychoacousticMetrics::kBarkBands - 1, static_cast<int>(std::floor(bark)));
}

void accumulateBandEnergy(const std::vector<float>& magnitudes,
                          const std::vector<float>& binHz,
                          std::array<double, PsychoacousticMetrics::kBarkBands>& bandPower)
{
    bandPower.fill(0.0);
    for (size_t i = 0; i < magnitudes.size(); ++i)
    {
        if (binHz[i] < 20.0f)
            continue;

        const auto bark = PsychoacousticMetrics::hzToBark(binHz[i]);
        const auto idx = barkBandIndex(bark);
        const auto p = static_cast<double>(magnitudes[i]) * magnitudes[i];
        bandPower[static_cast<size_t>(idx)] += p;
    }
}

float bandPowerToSoneLike(double power)
{
    if (power <= 1.0e-20)
        return 0.0f;

    const auto levelDb = 10.0f * std::log10(static_cast<float>(power));
    const auto phon = levelDb + 70.0f;
    if (phon < 30.0f)
        return 0.0f;
    return std::pow(2.0f, (phon - 40.0f) / 10.0f);
}

float estimateTonality(const std::vector<float>& magnitudes, const std::vector<float>& binHz)
{
    if (magnitudes.size() < 8)
        return 0.0f;

    double tonal = 0.0;
    double total = 0.0;

    for (size_t i = 2; i + 2 < magnitudes.size(); ++i)
    {
        if (binHz[i] < 200.0f || binHz[i] > 12000.0f)
            continue;

        const auto local = (magnitudes[i - 1] + magnitudes[i] + magnitudes[i + 1]) / 3.0f;
        const auto peakness = magnitudes[i] / juce::jmax(1.0e-9f, local);
        const auto p = static_cast<double>(magnitudes[i]) * magnitudes[i];
        total += p;
        if (peakness > 1.8f)
            tonal += p * static_cast<double>(peakness - 1.0f);
    }

    if (total < 1.0e-20)
        return 0.0f;
    return juce::jlimit(0.0f, 1.0f, static_cast<float>(tonal / total));
}

void computeGlobalMaskingThreshold(const std::array<float, PsychoacousticMetrics::kBarkBands>& bandLevelDb,
                                   std::array<float, PsychoacousticMetrics::kBarkBands>& gmtDb)
{
    std::array<float, PsychoacousticMetrics::kBarkBands> athDb {};
    for (int i = 0; i < PsychoacousticMetrics::kBarkBands; ++i)
    {
        const auto centerHz = PsychoacousticMetrics::barkToHz(static_cast<float>(i) + 0.5f);
        athDb[static_cast<size_t>(i)] = terhardtAthDb(centerHz);
    }

    for (int i = 0; i < PsychoacousticMetrics::kBarkBands; ++i)
    {
        double linearSum = std::pow(10.0, athDb[static_cast<size_t>(i)] / 10.0);

        for (int j = 0; j < PsychoacousticMetrics::kBarkBands; ++j)
        {
            const auto delta = static_cast<float>(i - j);
            const auto spread = maskingSpreadDb(delta);
            const auto maskDb = bandLevelDb[static_cast<size_t>(j)] + spread;
            linearSum += std::pow(10.0, maskDb / 10.0);
        }

        gmtDb[static_cast<size_t>(i)] = 10.0f * std::log10(static_cast<float>(linearSum));
    }
}

float roughnessCarrierWeight(float barkBand47)
{
    const auto z = barkBand47 * 24.0f / 47.0f;
    return sharpnessWeight(z);
}

void computeTemporalModulationMetrics(const juce::AudioBuffer<float>& mono,
                                      double sampleRate,
                                      PsychoacousticMetrics& metrics)
{
    const int n = mono.getNumSamples();
    if (n < static_cast<int>(sampleRate * 0.25))
        return;

    const int frame = juce::jmax(64, static_cast<int>(0.020 * sampleRate));
    const int hop = juce::jmax(32, frame / 2);
    const int numFrames = 1 + (n - frame) / hop;
    if (numFrames < 8)
        return;

    std::array<std::vector<float>, PsychoacousticMetrics::kModulationBands> bandRmsSeries;
    for (auto& series : bandRmsSeries)
        series.resize(static_cast<size_t>(numFrames));

    const auto* data = mono.getReadPointer(0);

    for (int f = 0; f < numFrames; ++f)
    {
        const int start = f * hop;
        double totalE = 0.0;
        for (int i = 0; i < frame; ++i)
        {
            const auto x = data[start + i];
            totalE += x * x;
        }
        const auto totalRms = std::sqrt(totalE / frame);

        for (int b = 0; b < PsychoacousticMetrics::kModulationBands; ++b)
        {
            const auto bark = (static_cast<float>(b) + 0.5f) * 24.0f / static_cast<float>(PsychoacousticMetrics::kModulationBands);
            const auto centerHz = PsychoacousticMetrics::barkToHz(bark);
            const auto omega = 2.0 * juce::MathConstants<double>::pi * centerHz / sampleRate;
            const auto coeff = std::exp(-omega);

            double bandE = 0.0;
            float yLp = 0.0f;
            for (int i = 0; i < frame; ++i)
            {
                const auto x = data[start + i];
                yLp = static_cast<float>((1.0 - coeff) * x + coeff * yLp);
                bandE += yLp * yLp;
            }
            const auto bandRms = std::sqrt(bandE / frame);
            bandRmsSeries[static_cast<size_t>(b)][static_cast<size_t>(f)] =
                totalRms > 1.0e-9f ? bandRms / static_cast<float>(totalRms) : 0.0f;
        }
    }

    auto modulationDepthInBand = [&](const std::vector<float>& series, float modLoHz, float modHiHz) {
        const int m = static_cast<int>(series.size());
        if (m < 4)
            return 0.0f;

        double mean = 0.0;
        for (auto v : series)
            mean += v;
        mean /= m;

        double modEnergy = 0.0;
        double totalVar = 0.0;
        const int maxK = juce::jmin(m / 2, 512);
        for (int k = 1; k < maxK; ++k)
        {
            double re = 0.0;
            double im = 0.0;
            for (int t = 0; t < m; ++t)
            {
                const auto phase = 2.0 * juce::MathConstants<double>::pi * k * t / m;
                const auto v = series[static_cast<size_t>(t)] - mean;
                re += v * std::cos(phase);
                im += v * std::sin(phase);
            }
            const auto freq = k * static_cast<float>(sampleRate) / static_cast<float>(hop * m);
            const auto amp = static_cast<float>(std::sqrt(re * re + im * im) / m);
            totalVar += amp * amp;
            if (freq >= modLoHz && freq <= modHiHz)
                modEnergy += amp * amp;
        }
        if (totalVar < 1.0e-12f)
            return 0.0f;
        return static_cast<float>(std::sqrt(modEnergy / totalVar));
    };

    float roughnessSum = 0.0f;
    float fluctuationSum = 0.0f;
    float weightSum = 0.0f;

    for (int b = 0; b < PsychoacousticMetrics::kModulationBands; ++b)
    {
        const auto& series = bandRmsSeries[static_cast<size_t>(b)];
        const auto miRough = modulationDepthInBand(series, 20.0f, 300.0f);
        const auto miFluct = modulationDepthInBand(series, 0.5f, 20.0f);
        metrics.bandModulationIndex[static_cast<size_t>(b)] = miRough;

        const auto w = roughnessCarrierWeight(static_cast<float>(b));
        roughnessSum += w * miRough;
        fluctuationSum += w * miFluct * (b > 8 ? 1.0f : 0.35f);
        weightSum += w;
    }

    if (weightSum > 1.0e-6f)
    {
        metrics.roughnessAsper = roughnessSum / weightSum;
        metrics.fluctuationVacil = fluctuationSum / weightSum;
    }
}

}

float PsychoacousticMetrics::hzToBark(float hz)
{
    const auto f = juce::jmax(1.0f, hz);
    return 13.0f * std::atan(0.00076f * f)
           + 3.5f * std::atan(std::pow(f / 7500.0f, 2.0f));
}

float PsychoacousticMetrics::barkToHz(float bark)
{
    float hz = 100.0f * std::pow(10.0f, bark / 6.0f);
    for (int i = 0; i < 8; ++i)
    {
        const auto z = hzToBark(hz);
        const auto dz = bark - z;
        hz *= std::pow(10.0f, dz / 6.0f);
    }
    return juce::jlimit(20.0f, 20000.0f, hz);
}

float PsychoacousticMetrics::erbBandwidthHz(float centerHz)
{
    return 24.7f * (4.37f * centerHz / 1000.0f + 1.0f);
}

PsychoacousticMetrics PsychoacousticMetrics::analyze(const juce::AudioBuffer<float>& buffer, double sampleRate)
{
    PsychoacousticMetrics metrics;

    if (buffer.getNumSamples() < 64 || buffer.getNumChannels() == 0)
        return metrics;

    constexpr double kMaxAnalysisSec = 45.0;
    const int maxSamples = static_cast<int>(kMaxAnalysisSec * sampleRate);
    const juce::AudioBuffer<float>* source = &buffer;
    juce::AudioBuffer<float> capped;

    if (buffer.getNumSamples() > maxSamples)
    {
        const int start = (buffer.getNumSamples() - maxSamples) / 2;
        capped.setSize(buffer.getNumChannels(), maxSamples);
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
            capped.copyFrom(ch, 0, buffer, ch, start, maxSamples);
        source = &capped;
    }

    const int fftOrder = juce::jlimit(10, 15, static_cast<int>(std::ceil(std::log2(source->getNumSamples()))));
    const int fftSize = 1 << fftOrder;

    juce::AudioBuffer<float> mono(1, source->getNumSamples());
    mono.clear();
    for (int ch = 0; ch < source->getNumChannels(); ++ch)
        mono.addFrom(0, 0, *source, ch, 0, source->getNumSamples(), 1.0f / source->getNumChannels());

    juce::dsp::FFT fft(fftOrder);
    std::vector<float> fftData(static_cast<size_t>(fftSize * 2), 0.0f);
    const int copyLen = juce::jmin(source->getNumSamples(), fftSize);
    std::copy(mono.getReadPointer(0), mono.getReadPointer(0) + copyLen, fftData.begin());

    for (int i = 0; i < fftSize; ++i)
    {
        const auto w = 0.5f * (1.0f - std::cos(2.0f * kPi * static_cast<float>(i) / static_cast<float>(fftSize - 1)));
        fftData[static_cast<size_t>(i)] *= w;
    }

    fft.performFrequencyOnlyForwardTransform(fftData.data());

    const int numBins = fftSize / 2;
    std::vector<float> magnitudes(static_cast<size_t>(numBins));
    std::vector<float> binHz(static_cast<size_t>(numBins));
    for (int i = 0; i < numBins; ++i)
    {
        magnitudes[static_cast<size_t>(i)] = fftData[static_cast<size_t>(i)];
        binHz[static_cast<size_t>(i)] = static_cast<float>(i) * static_cast<float>(sampleRate) / static_cast<float>(fftSize);
    }

    std::array<double, kBarkBands> bandPower {};
    accumulateBandEnergy(magnitudes, binHz, bandPower);

    std::array<float, kBarkBands> bandLevelDb {};
    for (int i = 0; i < kBarkBands; ++i)
    {
        const auto power = bandPower[static_cast<size_t>(i)];
        bandLevelDb[static_cast<size_t>(i)] = power > 1.0e-20
            ? 10.0f * std::log10(static_cast<float>(power))
            : -120.0f;
        metrics.barkSpecificLoudness[static_cast<size_t>(i)] = bandPowerToSoneLike(power);
    }

    float loudnessSum = 0.0f;
    float weightedSharp = 0.0f;
    for (int i = 0; i < kBarkBands; ++i)
    {
        const auto n = metrics.barkSpecificLoudness[static_cast<size_t>(i)];
        loudnessSum += n;
        weightedSharp += n * sharpnessWeight(static_cast<float>(i) + 0.5f);
    }
    metrics.sharpnessAcum = loudnessSum > 1.0e-6f ? weightedSharp / loudnessSum : 0.0f;
    metrics.tonalityIndex = estimateTonality(magnitudes, binHz);

    std::array<float, kBarkBands> gmtDb {};
    computeGlobalMaskingThreshold(bandLevelDb, gmtDb);

    float hfExcess = 0.0f;
    int hfBands = 0;
    for (int i = 17; i < kBarkBands; ++i)
    {
        const auto excess = bandLevelDb[static_cast<size_t>(i)] - gmtDb[static_cast<size_t>(i)];
        hfExcess += juce::jmax(0.0f, excess);
        ++hfBands;
    }
    metrics.hfAboveMaskingDb = hfBands > 0 ? hfExcess / static_cast<float>(hfBands) : 0.0f;

    computeTemporalModulationMetrics(mono, sampleRate, metrics);

    const float sharpNorm = juce::jlimit(0.0f, 1.0f, (metrics.sharpnessAcum - 1.0f) / 1.2f);
    const float maskNorm = juce::jlimit(0.0f, 1.0f, metrics.hfAboveMaskingDb / 12.0f);
    const float toneNorm = juce::jlimit(0.0f, 1.0f, metrics.tonalityIndex * 1.5f);
    const float roughNorm = juce::jlimit(0.0f, 1.0f, metrics.roughnessAsper / 0.45f);
    metrics.hfTamerStrength = juce::jlimit(0.0f, 1.0f,
        0.32f * maskNorm + 0.22f * sharpNorm + 0.16f * toneNorm + 0.30f * roughNorm);
    metrics.streamingRingingIndex = juce::jlimit(0.0f, 1.0f,
        0.35f * juce::jlimit(0.0f, 1.0f, metrics.sharpnessAcum / 2.5f)
        + 0.30f * roughNorm + 0.20f * toneNorm + 0.15f * maskNorm);

    return metrics;
}

}
