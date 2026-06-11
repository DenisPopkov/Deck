#pragma once

#include <juce_dsp/juce_dsp.h>

namespace cassette
{

struct PerceptualPlaybackSettings
{
    bool virtualSubEnabled = true;
    bool crossfeedEnabled = true;
    bool stereoWidenEnabled = false;
    float virtualSubAmount = 0.72f;
    float crossfeedAmount = 0.62f;
    float stereoWidenAmount = 0.45f;
};

class PerceptualPlaybackProcessor
{
public:
    void prepare(double sampleRate, int maxBlockSize);
    void reset();
    void setSettings(const PerceptualPlaybackSettings& settings);
    void process(juce::AudioBuffer<float>& buffer);

private:
    using IIR = juce::dsp::IIR::Filter<float>;
    using Coefs = juce::dsp::IIR::Coefficients<float>;
    using Shelf = juce::dsp::ProcessorDuplicator<IIR, Coefs>;

    void updateCrossfeedCoeffs();
    void processVirtualSub(juce::AudioBuffer<float>& buffer);
    void processCrossfeed(juce::AudioBuffer<float>& buffer);
    void processStereoWiden(juce::AudioBuffer<float>& buffer);

    PerceptualPlaybackSettings playbackSettings;
    double sampleRate = 48000.0;
    int maxBlock = 512;
    int crossDelaySamples = 10;

    juce::AudioBuffer<float> delayBuffer;
    int delayWritePos = 0;

    juce::dsp::ProcessorDuplicator<IIR, Coefs> lowSumLp;
    juce::dsp::ProcessorDuplicator<IIR, Coefs> subHpfMain;
    juce::dsp::ProcessorDuplicator<IIR, Coefs> harmBandBp;

    Shelf directHighShelf;
    Shelf directPeak;
    Shelf crossHighShelf;
    Shelf crossPeak;
    juce::dsp::ProcessorDuplicator<IIR, Coefs> crossLowPass;

    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> subMix { 0.72f };
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> crossMix { 0.62f };
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> widenMix { 0.45f };

    juce::dsp::ProcessorDuplicator<IIR, Coefs> sideAllPassA;
    juce::dsp::ProcessorDuplicator<IIR, Coefs> sideAllPassB;
    juce::dsp::ProcessorDuplicator<IIR, Coefs> sideLfHpf;

    juce::AudioBuffer<float> scratchMonoLow;
    juce::AudioBuffer<float> scratchHarmonics;
    juce::AudioBuffer<float> scratchDry;
    juce::AudioBuffer<float> scratchDirectL;
    juce::AudioBuffer<float> scratchDirectR;
    juce::AudioBuffer<float> scratchCrossL;
    juce::AudioBuffer<float> scratchCrossR;
    juce::AudioBuffer<float> scratchMidSide;
};

}
