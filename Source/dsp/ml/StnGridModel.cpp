#include "StnGridModel.h"
#include <cmath>

namespace cassette
{

namespace
{

struct StnCell
{
    float alpha;
    float beta;
    float mix;
    float driveScale;
};

constexpr StnCell kGrid[2][4] = {
    {
        { 0.008f, 0.018f, 0.22f, 0.88f },
        { 0.010f, 0.022f, 0.26f, 0.92f },
        { 0.012f, 0.026f, 0.30f, 0.96f },
        { 0.014f, 0.030f, 0.34f, 1.00f },
    },
    {
        { 0.011f, 0.024f, 0.28f, 1.05f },
        { 0.014f, 0.028f, 0.32f, 1.12f },
        { 0.017f, 0.032f, 0.36f, 1.18f },
        { 0.020f, 0.036f, 0.40f, 1.24f },
    },
};

StnCell interpolateGrid(float driveNorm, float biasNorm)
{
    const float d = juce::jlimit(0.0f, 1.0f, driveNorm);
    const float b = juce::jlimit(0.0f, 1.0f, biasNorm);
    const float bi = b * 3.0f;
    const int b0 = juce::jlimit(0, 2, static_cast<int>(std::floor(bi)));
    const int b1 = juce::jmin(3, b0 + 1);
    const float bt = bi - static_cast<float>(b0);

    const int d0 = d < 0.5f ? 0 : 1;
    const int d1 = d0 == 0 ? 1 : 1;
    const float dt = d0 == 0 ? d * 2.0f : (d - 0.5f) * 2.0f;

    auto lerpCell = [](const StnCell& a, const StnCell& b, float t) {
        StnCell c;
        c.alpha = a.alpha + t * (b.alpha - a.alpha);
        c.beta = a.beta + t * (b.beta - a.beta);
        c.mix = a.mix + t * (b.mix - a.mix);
        c.driveScale = a.driveScale + t * (b.driveScale - a.driveScale);
        return c;
    };

    const auto c00 = lerpCell(kGrid[d0][b0], kGrid[d0][b1], bt);
    const auto c10 = lerpCell(kGrid[d1][b0], kGrid[d1][b1], bt);
    return lerpCell(c00, c10, dt);
}

}

void StnGridModel::reset()
{
    stateL = stateR = 0.0f;
}

float StnGridModel::processSample(float x,
                                  float& state,
                                  float alpha,
                                  float beta,
                                  float mix,
                                  float drive) const
{
    const float driven = x * drive;
    state = (1.0f - alpha) * state + alpha * std::tanh(driven - beta * state);
    const float nonlinear = std::tanh(driven + mix * state);
    return (1.0f - mix) * driven + mix * nonlinear;
}

void StnGridModel::process(juce::AudioBuffer<float>& buffer,
                            const CassetteProfile& profile,
                            double sampleRate)
{
    juce::ignoreUnused(sampleRate);

    const float driveNorm = juce::jlimit(0.0f, 1.0f, (profile.saturationDrive - 0.85f) / 0.45f);
    const float biasNorm = juce::jlimit(0.0f, 1.0f, profile.biasReductionOnHf / 0.12f);
    const auto cell = interpolateGrid(driveNorm, biasNorm);
    const float drive = profile.saturationDrive * cell.driveScale;

    const int n = buffer.getNumSamples();
    const int ch = buffer.getNumChannels();

    for (int i = 0; i < n; ++i)
    {
        if (ch >= 1)
        {
            auto* l = buffer.getWritePointer(0);
            l[i] = processSample(l[i], stateL, cell.alpha, cell.beta, cell.mix, drive);
        }
        if (ch >= 2)
        {
            auto* r = buffer.getWritePointer(1);
            r[i] = processSample(r[i], stateR, cell.alpha, cell.beta, cell.mix, drive);
        }
    }
}

}
