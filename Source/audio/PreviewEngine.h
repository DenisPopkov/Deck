#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include "PerceptualPlaybackProcessor.h"

namespace cassette
{

class PreviewEngine : public juce::AudioSource
{
public:
    void prepareToPlay(int samplesPerBlockExpected, double sampleRate) override;
    void releaseResources() override;
    void getNextAudioBlock(const juce::AudioSourceChannelInfo& bufferToFill) override;

    void setBuffer(juce::AudioBuffer<float> buffer, double sr);
    void setPlayheadSec(double sec);
    double getPlayheadSec() const { return playheadSec; }
    double getDurationSec() const;
    bool isPlaying() const { return playing; }
    void setPlaying(bool shouldPlay);

    void setMonitoringEnabled(bool enabled);
    void setPlaybackSettings(const PerceptualPlaybackSettings& settings);

private:
    void rebuildPlaybackBuffer();

    juce::CriticalSection bufferLock;
    juce::AudioBuffer<float> sourceBuffer;
    juce::AudioBuffer<float> mixBuffer;
    PerceptualPlaybackProcessor playbackFx;
    double sourceSampleRate = 44100.0;
    double deviceSampleRate = 44100.0;
    double playheadSec = 0.0;
    bool playing = false;
    bool monitoringEnabled = false;
};

}
