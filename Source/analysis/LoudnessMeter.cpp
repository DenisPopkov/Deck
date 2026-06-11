#include "LoudnessMeter.h"
#include <cmath>
#include <vector>
#include <algorithm>

namespace cassette
{

namespace
{

constexpr double kEps = 1.0e-20;
constexpr float kAbsoluteGateLufs = -70.0f;
constexpr float kRelativeGateLu = 10.0f;
constexpr float kLraRelativeGateLu = 20.0f;

struct BiquadCoef
{
    float b0 = 1.0f, b1 = 0.0f, b2 = 0.0f, a1 = 0.0f, a2 = 0.0f;
};

struct BiquadState
{
    float z1 = 0.0f, z2 = 0.0f;

    float process(float x, const BiquadCoef& c)
    {
        const float y = c.b0 * x + z1;
        z1 = c.b1 * x - c.a1 * y + z2;
        z2 = c.b2 * x - c.a2 * y;
        return y;
    }

    void reset() { z1 = z2 = 0.0f; }
};

void designKWeighting(double sampleRate, BiquadCoef& shelf, BiquadCoef& hp)
{
    if (std::abs(sampleRate - 48000.0) < 1.0)
    {
        shelf = { 1.53512485958697f, -2.69169618940638f, 1.19839281285285f,
                  -1.69065929318241f, 0.73248077421585f };
        hp = { 1.0f, -2.0f, 1.0f, -1.99004745483398f, 0.99007225036621f };
        return;
    }

    if (std::abs(sampleRate - 44100.0) < 1.0)
    {
        shelf = { 1.53084106703496f, -2.69016469436159f, 1.19828036409584f,
                  -1.68669643436124f, 0.72999491541099f };
        hp = { 1.0f, -2.0f, 1.0f, -1.989168704092f, 0.989168704092f };
        return;
    }

    {
        const double f0 = 1681.974450955533;
        const double G = 3.99984385397;
        const double Q = 0.70717514692;
        const double K = std::tan(juce::MathConstants<double>::pi * f0 / sampleRate);
        const double Vh = std::pow(10.0, G / 20.0);
        const double Vb = std::pow(Vh, 0.5);
        const double a0 = 1.0 + K / Q + K * K;

        shelf.b0 = static_cast<float>((Vh + Vb * K / Q + K * K) / a0);
        shelf.b1 = static_cast<float>(2.0 * (K * K - Vh) / a0);
        shelf.b2 = static_cast<float>((Vh - Vb * K / Q + K * K) / a0);
        shelf.a1 = static_cast<float>(2.0 * (K * K - 1.0) / a0);
        shelf.a2 = static_cast<float>((1.0 - K / Q + K * K) / a0);
    }

    {
        const double f0 = 38.135470876;
        const double Q = 0.500327011;
        const double K = std::tan(juce::MathConstants<double>::pi * f0 / sampleRate);
        const double a0 = 1.0 + K / Q + K * K;

        hp.b0 = static_cast<float>(1.0 / a0);
        hp.b1 = static_cast<float>(-2.0 / a0);
        hp.b2 = static_cast<float>(1.0 / a0);
        hp.a1 = static_cast<float>(2.0 * (K * K - 1.0) / a0);
        hp.a2 = static_cast<float>((1.0 - K / Q + K * K) / a0);
    }
}

float filterSample(float x, BiquadState& s1, BiquadState& s2, const BiquadCoef& shelf, const BiquadCoef& hp)
{
    return s2.process(s1.process(x, shelf), hp);
}

void applyKWeighting(juce::AudioBuffer<float>& filtered,
                     const juce::AudioBuffer<float>& buffer,
                     const BiquadCoef& shelf,
                     const BiquadCoef& hp)
{
    const int channels = buffer.getNumChannels();
    const int n = buffer.getNumSamples();
    filtered.setSize(channels, n, false, false, true);

    for (int ch = 0; ch < channels; ++ch)
    {
        BiquadState s1, s2;
        const auto* in = buffer.getReadPointer(ch);
        auto* out = filtered.getWritePointer(ch);
        for (int i = 0; i < n; ++i)
            out[i] = filterSample(in[i], s1, s2, shelf, hp);
    }
}

double windowMeanSquare(const juce::AudioBuffer<float>& filtered, int start, int length)
{
    const int channels = filtered.getNumChannels();
    if (length <= 0 || channels == 0)
        return 0.0;

    double sum = 0.0;
    for (int i = 0; i < length; ++i)
    {
        double frame = 0.0;
        for (int ch = 0; ch < channels; ++ch)
        {
            const float y = filtered.getSample(ch, start + i);
            frame += static_cast<double>(y) * y;
        }
        sum += frame / static_cast<double>(channels);
    }

    return sum / static_cast<double>(length);
}

void collectBlockEnergies(const juce::AudioBuffer<float>& filtered,
                          double sampleRate,
                          double blockSeconds,
                          double hopSeconds,
                          std::vector<double>& meanSquares)
{
    const int block = juce::jmax(1, static_cast<int>(blockSeconds * sampleRate));
    const int hop = juce::jmax(1, static_cast<int>(hopSeconds * sampleRate));
    const int n = filtered.getNumSamples();

    meanSquares.clear();
    for (int start = 0; start < n; start += hop)
    {
        const int len = juce::jmin(block, n - start);
        if (len < block / 4)
            break;
        meanSquares.push_back(windowMeanSquare(filtered, start, len));
    }
}

double gatedIntegratedMeanSquare(const std::vector<double>& meanSquares)
{
    if (meanSquares.empty())
        return 0.0;

    const double absThreshold = std::pow(10.0, (static_cast<double>(kAbsoluteGateLufs) + 0.691) / 10.0);

    double sum = 0.0;
    int count = 0;
    for (const auto z : meanSquares)
    {
        if (z >= absThreshold)
        {
            sum += z;
            ++count;
        }
    }

    if (count == 0)
        return 0.0;

    const double ungatedLufs = LoudnessMeter::lufsFromMeanSquare(sum / static_cast<double>(count));
    const double relThreshold = std::pow(10.0, (static_cast<double>(ungatedLufs - kRelativeGateLu) + 0.691) / 10.0);

    sum = 0.0;
    count = 0;
    for (const auto z : meanSquares)
    {
        if (z >= absThreshold && z >= relThreshold)
        {
            sum += z;
            ++count;
        }
    }

    return count > 0 ? sum / static_cast<double>(count) : 0.0;
}

float computeLra(const std::vector<double>& shortTermMeanSquares, float integratedLufs)
{
    if (shortTermMeanSquares.size() < 2)
        return 0.0f;

    const double absThreshold = std::pow(10.0, (static_cast<double>(kAbsoluteGateLufs) + 0.691) / 10.0);
    const double relThreshold = std::pow(10.0, (static_cast<double>(integratedLufs - kLraRelativeGateLu) + 0.691) / 10.0);

    std::vector<float> gatedLufs;
    gatedLufs.reserve(shortTermMeanSquares.size());

    for (const auto z : shortTermMeanSquares)
    {
        if (z >= absThreshold && z >= relThreshold)
            gatedLufs.push_back(LoudnessMeter::lufsFromMeanSquare(z));
    }

    if (gatedLufs.size() < 2)
        return 0.0f;

    std::sort(gatedLufs.begin(), gatedLufs.end());
    const auto p10 = gatedLufs[gatedLufs.size() * 10 / 100];
    const auto p95 = gatedLufs[gatedLufs.size() * 95 / 100];
    return juce::jmax(0.0f, p95 - p10);
}

}

float LoudnessMeter::lufsFromMeanSquare(double meanSquare)
{
    return -0.691f + 10.0f * static_cast<float>(std::log10(meanSquare + kEps));
}

LoudnessMeter::Result LoudnessMeter::analyze(const juce::AudioBuffer<float>& buffer, double sampleRate)
{
    Result result;
    if (buffer.getNumSamples() == 0 || buffer.getNumChannels() == 0 || sampleRate <= 0.0)
        return result;

    BiquadCoef shelf, hp;
    designKWeighting(sampleRate, shelf, hp);

    juce::AudioBuffer<float> filtered;
    applyKWeighting(filtered, buffer, shelf, hp);

    std::vector<double> integratedBlocks;
    collectBlockEnergies(filtered, sampleRate, 0.4, 0.1, integratedBlocks);

    const double integratedZ = gatedIntegratedMeanSquare(integratedBlocks);
    if (integratedZ > 0.0)
        result.integratedLufs = lufsFromMeanSquare(integratedZ);

    std::vector<double> shortTermBlocks;
    collectBlockEnergies(filtered, sampleRate, 3.0, 0.75, shortTermBlocks);

    for (const auto z : shortTermBlocks)
    {
        const auto lufs = lufsFromMeanSquare(z);
        result.shortTermMaxLufs = juce::jmax(result.shortTermMaxLufs, lufs);
        result.shortTermMinLufs = juce::jmin(result.shortTermMinLufs, lufs);
    }

    if (shortTermBlocks.empty())
        result.shortTermMinLufs = result.shortTermMaxLufs;

    result.loudnessRangeLu = computeLra(shortTermBlocks, result.integratedLufs);
    return result;
}

}
