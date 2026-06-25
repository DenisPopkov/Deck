#pragma once

#include <juce_core/juce_core.h>

namespace cassette
{

class AppLocale
{
public:
    static AppLocale& instance();

    juce::String tr(const juce::String& key) const;
    juce::String removeTracksMessage(int count) const;

private:
    AppLocale() = default;
};

inline juce::String tr(const juce::String& key)
{
    return AppLocale::instance().tr(key);
}

inline juce::String trf(const juce::String& key, const juce::String& arg)
{
    return tr(key).replace("%s", arg);
}

inline juce::String trf(const juce::String& key, int value)
{
    return tr(key).replace("%d", juce::String(value));
}

inline juce::String trRemoveTracksMessage(int count)
{
    return AppLocale::instance().removeTracksMessage(count);
}

}
