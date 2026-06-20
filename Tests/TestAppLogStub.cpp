#include "../Source/util/AppLog.h"

namespace cassette
{

void initLogging() {}

void log(const juce::String&) {}

void logTiming(const juce::String&, double, const juce::String&) {}

ScopedTimer::ScopedTimer(juce::String tagIn, juce::String detailIn)
    : tag(std::move(tagIn)), detail(std::move(detailIn)) {}

ScopedTimer::~ScopedTimer() {}

}
