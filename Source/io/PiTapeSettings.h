#pragma once

#include <juce_core/juce_core.h>

namespace cassette
{

struct PiTapeSettings
{
    bool enabled = false;
    juce::String host = "raspberrypi.local";
    int port = 21;
    int controlPort = 8765;
    juce::String username = "deck";
    juce::String password;
    juce::String remoteDir = "inbox";

    bool isConfigured() const
    {
        return host.trim().isNotEmpty() && username.trim().isNotEmpty() && port > 0 && port < 65536
               && controlPort > 0 && controlPort < 65536;
    }

    static juce::File settingsFile();
    static PiTapeSettings load();
    bool save() const;

    juce::var toVar() const;
    static PiTapeSettings fromVar(const juce::var& v);
};

} // namespace cassette
