#pragma once

#include "../util/AppLog.h"
#include <unistd.h>

namespace cassette
{

/** Enabled when DECK_PI_TRACE=1 or Deck stdout is a terminal. */
inline bool piTapeTraceEnabled()
{
    static const bool enabled = [] {
        const auto env = juce::SystemStats::getEnvironmentVariable("DECK_PI_TRACE", "");
        if (env.isNotEmpty())
            return ! env.equalsIgnoreCase("0") && ! env.equalsIgnoreCase("false");
        return isatty(fileno(stdout)) != 0;
    }();
    return enabled;
}

inline void piTapeLog(const juce::String& message)
{
    if (piTapeTraceEnabled())
        log("[pi-tape] " + message);
}

inline void piTapeLogTiming(const juce::String& phase, double ms, const juce::String& detail = {})
{
    if (! piTapeTraceEnabled())
        return;

    if (detail.isNotEmpty())
        logTiming("pi-tape/" + phase, ms, detail);
    else
        logTiming("pi-tape/" + phase, ms, {});
}

class PiTapeScopedTimer
{
public:
    PiTapeScopedTimer(juce::String phaseIn, juce::String detailIn = {})
        : phase(std::move(phaseIn)), detail(std::move(detailIn)), t0(juce::Time::getMillisecondCounterHiRes())
    {
        if (piTapeTraceEnabled())
            piTapeLog("→ " + phase + (detail.isNotEmpty() ? " (" + detail + ")" : juce::String()));
    }

    ~PiTapeScopedTimer()
    {
        if (! piTapeTraceEnabled())
            return;

        const double ms = juce::Time::getMillisecondCounterHiRes() - t0;
        piTapeLogTiming(phase, ms, detail);
    }

    PiTapeScopedTimer(const PiTapeScopedTimer&) = delete;
    PiTapeScopedTimer& operator=(const PiTapeScopedTimer&) = delete;

private:
    juce::String phase;
    juce::String detail;
    double t0 { 0.0 };
};

} // namespace cassette
