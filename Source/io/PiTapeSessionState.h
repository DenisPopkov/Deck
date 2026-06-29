#pragma once

#include <juce_core/juce_core.h>

namespace cassette
{

struct PiTapeSessionState
{
    static constexpr juce::int64 kExpiryMs = 2LL * 60 * 60 * 1000;

    juce::int64 uploadedAtUnixMs = 0;

    bool hasActiveSession() const { return uploadedAtUnixMs > 0; }

    bool isExpired(juce::int64 nowMs = juce::Time::currentTimeMillis()) const
    {
        if (uploadedAtUnixMs <= 0)
            return false;
        return nowMs - uploadedAtUnixMs >= kExpiryMs;
    }

    void markUploadedNow() { uploadedAtUnixMs = juce::Time::currentTimeMillis(); }

    static juce::File stateFile();
    static PiTapeSessionState load();
    static void clearPersisted();
    void save() const;
};

} // namespace cassette
