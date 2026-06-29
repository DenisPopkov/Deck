#pragma once

#include <juce_core/juce_core.h>

namespace cassette
{

struct PiTapeSideQueueState
{
    bool ready = false;
    bool played = false;
    juce::String side;
    juce::String filename;
    double durationSec = 0.0;
};

struct PiTapePlaybackState
{
    juce::String state { "idle" };
    juce::String side;
    double positionSec = 0.0;
    double durationSec = 0.0;

    bool isPlaying() const { return state.equalsIgnoreCase("playing"); }
    bool isStarting() const { return state.equalsIgnoreCase("starting"); }
    bool isStopped() const { return state.equalsIgnoreCase("stopped"); }
    bool isIdle() const { return state.equalsIgnoreCase("idle"); }
    double progress01() const
    {
        if (durationSec <= 0.0)
            return 0.0;
        return juce::jlimit(0.0, 1.0, positionSec / durationSec);
    }
};

struct PiTapeRemoteStatus
{
    bool online = false;
    juce::String error;
    juce::String queueJobId;
    juce::String projectName;
    PiTapePlaybackState playback;
    PiTapeSideQueueState sideA;
    PiTapeSideQueueState sideB;

    static PiTapeRemoteStatus fromJson(const juce::String& jsonText);
};

} // namespace cassette
