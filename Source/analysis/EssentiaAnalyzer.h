#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include "AudioFeatures.h"

namespace cassette
{

class EssentiaAnalyzer
{
public:
    static AudioFeatures extractFeatures(const juce::AudioBuffer<float>& buffer, double sampleRate);
    static AudioFeatures extractFeaturesForMastering(const juce::AudioBuffer<float>& buffer, double sampleRate);
    static juce::AudioBuffer<float> excerpt(const juce::AudioBuffer<float>& buffer,
                                            double sampleRate,
                                            double maxSec = 45.0);

private:
    static float estimateIntegratedLufs(const juce::AudioBuffer<float>& buffer, double sampleRate);
    static float lufsFromMeanSquare(double meanSquare);
    static void estimateShortTermLoudness(const juce::AudioBuffer<float>& buffer,
                                          double sampleRate,
                                          float& maxLufs,
                                          float& minLufs,
                                          float& loudnessRangeLu);
    static float estimateTruePeakDbfs(const juce::AudioBuffer<float>& buffer, double sampleRate);
    static float estimateSamplePeakDbfs(const juce::AudioBuffer<float>& buffer);
    static void estimateBandEnergyRatios(const juce::AudioBuffer<float>& buffer,
                                         double sampleRate,
                                         float& lf,
                                         float& mf,
                                         float& hf);
    static float stereoCorrelation(const juce::AudioBuffer<float>& buffer, double sampleRate, double lpCutoffHz);
    static float estimateStereoWidthPercent(const juce::AudioBuffer<float>& buffer);
    static float estimateNoiseFloorDbfs(const juce::AudioBuffer<float>& buffer, double sampleRate);
    static float estimateClippingPercent(const juce::AudioBuffer<float>& buffer);
    static float estimateDcOffsetDbfs(const juce::AudioBuffer<float>& buffer);
    static float estimateDynamicRangeDb(const juce::AudioBuffer<float>& buffer, double sampleRate);
};

}
