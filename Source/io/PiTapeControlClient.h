#pragma once

#include "PiTapeRemoteStatus.h"
#include "PiTapeSettings.h"

namespace cassette
{

struct PiTapeHttpResult
{
    bool success = false;
    int statusCode = 0;
    juce::String body;
    juce::String error;
};

class PiTapeControlClient
{
public:
    PiTapeRemoteStatus fetchStatus(const PiTapeSettings& settings) const;
    PiTapeHttpResult playSide(const PiTapeSettings& settings,
                              const juce::String& sideLabel,
                              double offsetSec = -1.0) const;
    PiTapeHttpResult stopPlayback(const PiTapeSettings& settings) const;
    PiTapeHttpResult cleanupSession(const PiTapeSettings& settings) const;

private:
    PiTapeHttpResult request(const PiTapeSettings& settings,
                             const juce::String& method,
                             const juce::String& path,
                             const juce::String& body) const;
};

} // namespace cassette
