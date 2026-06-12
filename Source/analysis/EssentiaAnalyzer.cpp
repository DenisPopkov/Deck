#include "EssentiaAnalyzer.h"
#include "EssentiaBridge.h"
#include "LoudnessMeter.h"
#include "SpectrumAnalyzer.h"
#include "TruePeakMeter.h"
#include "PsychoacousticMetrics.h"
#include "WowFlutterAnalyzer.h"
#include <cmath>
#include <vector>
#include <algorithm>

namespace cassette
{

namespace
{
constexpr float kEps = 1.0e-12f;
constexpr float kClipLinear = 0.988548f;

float blockRms(const float* data, int numSamples)
{
    double sum = 0.0;
    for (int i = 0; i < numSamples; ++i)
        sum += static_cast<double>(data[i]) * data[i];
    return static_cast<float>(std::sqrt(sum / static_cast<double>(juce::jmax(1, numSamples)) + kEps));
}

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
}

float EssentiaAnalyzer::lufsFromMeanSquare(double meanSquare)
{
    return -0.691f + 10.0f * static_cast<float>(std::log10(meanSquare + static_cast<double>(kEps)));
}

AudioFeatures EssentiaAnalyzer::extractFeatures(const juce::AudioBuffer<float>& buffer, double sampleRate)
{
    AudioFeatures f;
    if (buffer.getNumSamples() == 0 || buffer.getNumChannels() == 0)
        return f;

    f.integratedLUFS = estimateIntegratedLufs(buffer, sampleRate);
    estimateShortTermLoudness(buffer, sampleRate, f.shortTermMaxLUFS, f.shortTermMinLUFS, f.loudnessRangeDb);

    f.truePeakDbfs = estimateTruePeakDbfs(buffer, sampleRate);
    f.samplePeakDbfs = estimateSamplePeakDbfs(buffer);

    const auto rms = blockRms(buffer.getReadPointer(0), buffer.getNumSamples());
    if (buffer.getNumChannels() > 1)
    {
        std::vector<float> mono;
        makeMonoMix(buffer, mono);
        const auto monoRms = blockRms(mono.data(), buffer.getNumSamples());
        f.rmsLevelDb = juce::Decibels::gainToDecibels(monoRms, -100.0f);
    }
    else
    {
        f.rmsLevelDb = juce::Decibels::gainToDecibels(rms, -100.0f);
    }

    f.crestFactorDb = f.truePeakDbfs - f.rmsLevelDb;
    estimateBandEnergyRatios(buffer, sampleRate, f.lfEnergyRatio, f.mfEnergyRatio, f.hfEnergyRatio);

    f.stereoCorrelation = stereoCorrelation(buffer, sampleRate, 20000.0f);
    f.lfStereoCorrelation = stereoCorrelation(buffer, sampleRate, 150.0f);
    f.stereoWidthPercent = estimateStereoWidthPercent(buffer);

    f.noiseFloorDbfs = estimateNoiseFloorDbfs(buffer, sampleRate);
    f.clippingPercent = estimateClippingPercent(buffer);
    f.dcOffsetDbfs = estimateDcOffsetDbfs(buffer);
    f.dynamicRangeDb = estimateDynamicRangeDb(buffer, sampleRate);
    f.psycho = PsychoacousticMetrics::analyze(buffer, sampleRate);
    f.wowFlutter = WowFlutterAnalyzer::analyze(buffer, sampleRate);

    return f;
}

juce::AudioBuffer<float> EssentiaAnalyzer::excerpt(const juce::AudioBuffer<float>& buffer,
                                                    double sampleRate,
                                                    double maxSec)
{
    const int maxSamples = static_cast<int>(maxSec * sampleRate);
    if (buffer.getNumSamples() <= maxSamples || buffer.getNumChannels() == 0)
        return buffer;

    juce::AudioBuffer<float> out(buffer.getNumChannels(), maxSamples);
    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        out.copyFrom(ch, 0, buffer, ch, 0, maxSamples);
    return out;
}

AudioFeatures EssentiaAnalyzer::extractFeaturesForMastering(const juce::AudioBuffer<float>& buffer,
                                                            double sampleRate)
{
    AudioFeatures f;
    if (buffer.getNumSamples() == 0 || buffer.getNumChannels() == 0)
        return f;

    // Full-track peak is cheap; heavy analysis uses a short excerpt only.
    constexpr double kMasteringAnalysisSec = 30.0;
    const auto analysis = excerpt(buffer, sampleRate, kMasteringAnalysisSec);

    f.integratedLUFS = estimateIntegratedLufs(analysis, sampleRate);
    f.samplePeakDbfs = estimateSamplePeakDbfs(buffer);
    f.truePeakDbfs = estimateTruePeakDbfs(analysis, sampleRate);

    std::vector<float> mono;
    makeMonoMix(analysis, mono);
    f.rmsLevelDb = juce::Decibels::gainToDecibels(blockRms(mono.data(), analysis.getNumSamples()), -100.0f);
    f.crestFactorDb = f.truePeakDbfs - f.rmsLevelDb;

    estimateBandEnergyRatios(analysis, sampleRate, f.lfEnergyRatio, f.mfEnergyRatio, f.hfEnergyRatio);
    f.psycho = PsychoacousticMetrics::analyze(analysis, sampleRate);
    f.noiseFloorDbfs = -90.0f;

    return f;
}

float EssentiaAnalyzer::estimateIntegratedLufs(const juce::AudioBuffer<float>& buffer, double sampleRate)
{
    if (const auto essentiaLufs = EssentiaBridge::integratedLufs(buffer, sampleRate))
        return *essentiaLufs;

    return LoudnessMeter::analyze(buffer, sampleRate).integratedLufs;
}

void EssentiaAnalyzer::estimateShortTermLoudness(const juce::AudioBuffer<float>& buffer,
                                                 double sampleRate,
                                                 float& maxLufs,
                                                 float& minLufs,
                                                 float& loudnessRangeLu)
{
    const auto r = LoudnessMeter::analyze(buffer, sampleRate);
    maxLufs = r.shortTermMaxLufs;
    minLufs = r.shortTermMinLufs;
    loudnessRangeLu = r.loudnessRangeLu;
}

float EssentiaAnalyzer::estimateTruePeakDbfs(const juce::AudioBuffer<float>& buffer, double sampleRate)
{
    return TruePeakMeter::measurePeakDbfs(buffer, sampleRate);
}

float EssentiaAnalyzer::estimateSamplePeakDbfs(const juce::AudioBuffer<float>& buffer)
{
    float peak = 0.0f;
    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        peak = juce::jmax(peak, buffer.getMagnitude(ch, 0, buffer.getNumSamples()));
    return juce::Decibels::gainToDecibels(peak, -100.0f);
}

void EssentiaAnalyzer::estimateBandEnergyRatios(const juce::AudioBuffer<float>& buffer,
                                                double sampleRate,
                                                float& lf,
                                                float& mf,
                                                float& hf)
{
    const auto bands = SpectrumAnalyzer::bandEnergyRatios(buffer, sampleRate);
    lf = bands.lf;
    mf = bands.mf;
    hf = bands.hf;
}

float EssentiaAnalyzer::stereoCorrelation(const juce::AudioBuffer<float>& buffer,
                                          double sampleRate,
                                          double lpCutoffHz)
{
    if (buffer.getNumChannels() < 2)
        return 1.0f;

    const auto* l = buffer.getReadPointer(0);
    const auto* r = buffer.getReadPointer(1);
    const int numSamples = buffer.getNumSamples();
    const float c = std::exp(-2.0f * juce::MathConstants<float>::pi * static_cast<float>(lpCutoffHz)
                             / static_cast<float>(sampleRate));

    float yL = 0, yR = 0;
    double sumLR = 0, sumL2 = 0, sumR2 = 0;

    for (int i = 0; i < numSamples; ++i)
    {
        yL = (1.0f - c) * l[i] + c * yL;
        yR = (1.0f - c) * r[i] + c * yR;
        sumLR += yL * yR;
        sumL2 += yL * yL;
        sumR2 += yR * yR;
    }

    return static_cast<float>(sumLR / (std::sqrt(sumL2 * sumR2) + kEps));
}

float EssentiaAnalyzer::estimateStereoWidthPercent(const juce::AudioBuffer<float>& buffer)
{
    if (buffer.getNumChannels() < 2)
        return 0.0f;

    double midE = 0, sideE = 0;
    const auto* l = buffer.getReadPointer(0);
    const auto* r = buffer.getReadPointer(1);
    const int n = buffer.getNumSamples();

    for (int i = 0; i < n; ++i)
    {
        const auto mid = 0.5 * (l[i] + r[i]);
        const auto side = 0.5 * (l[i] - r[i]);
        midE += mid * mid;
        sideE += side * side;
    }

    const auto total = midE + sideE;
    if (total < kEps)
        return 0.0f;
    return static_cast<float>(100.0 * std::sqrt(sideE / total));
}

float EssentiaAnalyzer::estimateNoiseFloorDbfs(const juce::AudioBuffer<float>& buffer, double sampleRate)
{
    const int win = juce::jmax(1, static_cast<int>(0.1 * sampleRate));
    const int n = buffer.getNumSamples();
    std::vector<float> rmsBlocks;

    for (int start = 0; start < n; start += win)
    {
        const int len = juce::jmin(win, n - start);
        double sum = 0;
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        {
            const auto* d = buffer.getReadPointer(ch);
            for (int i = 0; i < len; ++i)
                sum += d[i] * d[i];
        }
        rmsBlocks.push_back(static_cast<float>(std::sqrt(sum / static_cast<double>(len * buffer.getNumChannels()))));
    }

    rmsBlocks.erase(std::remove_if(rmsBlocks.begin(), rmsBlocks.end(), [](float v) { return v < 1.0e-5f; }),
                    rmsBlocks.end());

    if (rmsBlocks.empty())
        return -90.0f;

    const size_t idx = juce::jmin(rmsBlocks.size() - 1, static_cast<size_t>(rmsBlocks.size() * 0.1));
    std::nth_element(rmsBlocks.begin(), rmsBlocks.begin() + static_cast<std::ptrdiff_t>(idx), rmsBlocks.end());
    return juce::Decibels::gainToDecibels(rmsBlocks[idx], -90.0f);
}

float EssentiaAnalyzer::estimateClippingPercent(const juce::AudioBuffer<float>& buffer)
{
    int clipped = 0;
    int total = 0;
    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
    {
        const auto* d = buffer.getReadPointer(ch);
        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            ++total;
            if (std::abs(d[i]) >= kClipLinear)
                ++clipped;
        }
    }
    return total > 0 ? (100.0f * static_cast<float>(clipped) / static_cast<float>(total)) : 0.0f;
}

float EssentiaAnalyzer::estimateDcOffsetDbfs(const juce::AudioBuffer<float>& buffer)
{
    double sum = 0;
    int total = 0;
    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
    {
        const auto* d = buffer.getReadPointer(ch);
        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            sum += d[i];
            ++total;
        }
    }
    const auto mean = static_cast<float>(sum / static_cast<double>(juce::jmax(1, total)));
    return juce::Decibels::gainToDecibels(std::abs(mean), -100.0f);
}

float EssentiaAnalyzer::estimateDynamicRangeDb(const juce::AudioBuffer<float>& buffer, double sampleRate)
{
    const int block = juce::jmax(1, static_cast<int>(0.4 * sampleRate));
    const int n = buffer.getNumSamples();
    const auto gate = juce::Decibels::decibelsToGain(-50.0f);
    float maxRms = 0;
    float minRms = 1.0f;

    for (int start = 0; start < n; start += block)
    {
        const int len = juce::jmin(block, n - start);
        const auto r = blockRms(buffer.getReadPointer(0) + start, len);
        if (r < gate)
            continue;
        maxRms = juce::jmax(maxRms, r);
        minRms = juce::jmin(minRms, r);
    }

    if (minRms >= 1.0f || maxRms < kEps)
        return 0.0f;
    return juce::Decibels::gainToDecibels(maxRms / minRms, -100.0f);
}

}
