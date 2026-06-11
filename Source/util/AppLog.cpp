#include "AppLog.h"
#include <juce_events/juce_events.h>

namespace cassette
{

namespace
{
juce::File logFilePath()
{
    return juce::File::getSpecialLocation(juce::File::userDesktopDirectory)
        .getChildFile("CassetteAutoMaster")
        .getChildFile("session.log");
}

class FileLogger : public juce::Logger
{
public:
    void logMessage(const juce::String& message) override
    {
        const auto line = juce::Time::getCurrentTime().toString(true, true, true, true) + "  " + message + "\n";
        const auto file = logFilePath();
        file.getParentDirectory().createDirectory();
        file.appendText(line, false, false, nullptr);
    }
};

FileLogger logger;
}

void initLogging()
{
    juce::Logger::setCurrentLogger(&logger);
    logFilePath().deleteFile();
}

}
