#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

namespace cassette
{

struct PreflightOptions
{
    juce::String artist;
    juce::String title;
    juce::String sideLabel = "Side A";
    juce::String dolbyNote = "no Dolby";
    bool includeSlate = true;
    bool includeTones = true;
    bool kenwoodKX1100Calibration = false;
};

class PreflightTones
{
public:
    static void prependToBuffer(juce::AudioBuffer<float>& musicBuffer,
                                double sampleRate,
                                const PreflightOptions& options);

private:
    static void appendTone(juce::AudioBuffer<float>& dest,
                           double sampleRate,
                           float frequencyHz,
                           float levelDbfs,
                           double durationSeconds);
    static void appendPinkNoise(juce::AudioBuffer<float>& dest, double sampleRate, float levelDbfs, double durationSeconds);
    static void appendSilence(juce::AudioBuffer<float>& dest, double sampleRate, double durationSeconds);
};

}
