#pragma once

#include <juce_core/juce_core.h>

namespace cassette
{

void initLogging();

/** App log → stdout + ~/Desktop/Deck/session.log */
void log(const juce::String& message);

/** Log with elapsed milliseconds, e.g. logTiming("folder-scan", 1234.5, "track.wav"); */
void logTiming(const juce::String& tag, double ms, const juce::String& detail = {});

/** RAII timer that logs on destruction. */
class ScopedTimer
{
public:
    ScopedTimer(juce::String tag, juce::String detail = {});
    ~ScopedTimer();

    ScopedTimer(const ScopedTimer&) = delete;
    ScopedTimer& operator=(const ScopedTimer&) = delete;

private:
    juce::String tag;
    juce::String detail;
    double t0 { 0.0 };
};

} // namespace cassette
