#include "AppLog.h"
#include <juce_events/juce_events.h>
#include <cstdio>

namespace cassette
{

namespace
{
juce::CriticalSection logLock;
juce::File logFilePath()
{
    return juce::File::getSpecialLocation(juce::File::userDesktopDirectory)
        .getChildFile("Deck")
        .getChildFile("session.log");
}

juce::String timestamp()
{
    return juce::Time::getCurrentTime().toString(true, true, true, true);
}

void writeLine(const juce::String& line)
{
    const juce::ScopedLock sl(logLock);
    std::fprintf(stdout, "%s\n", line.toRawUTF8());
    std::fflush(stdout);

    const auto file = logFilePath();
    file.getParentDirectory().createDirectory();
    file.appendText(line + "\n", false, false, nullptr);
}

class FileLogger : public juce::Logger
{
public:
    void logMessage(const juce::String& message) override { writeLine(timestamp() + "  " + message); }
};

FileLogger logger;
} // namespace

void initLogging()
{
    juce::Logger::setCurrentLogger(&logger);
    logFilePath().deleteFile();
    log("Deck logging started");
}

void log(const juce::String& message)
{
    juce::Logger::writeToLog(message);
}

void logTiming(const juce::String& tag, double ms, const juce::String& detail)
{
    const auto msText = juce::String(ms, 1) + " ms";
    if (detail.isNotEmpty())
        log("[" + tag + "] " + msText + " - " + detail);
    else
        log("[" + tag + "] " + msText);
}

ScopedTimer::ScopedTimer(juce::String tagIn, juce::String detailIn)
    : tag(std::move(tagIn)), detail(std::move(detailIn)), t0(juce::Time::getMillisecondCounterHiRes())
{
}

ScopedTimer::~ScopedTimer()
{
    const double ms = juce::Time::getMillisecondCounterHiRes() - t0;
    logTiming(tag, ms, detail);
}

} // namespace cassette
