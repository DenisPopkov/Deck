#include "PiTapeSettings.h"

namespace cassette
{

juce::File PiTapeSettings::settingsFile()
{
    const auto dir = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory).getChildFile("Deck");
    dir.createDirectory();
    return dir.getChildFile("pi-tape-settings.json");
}

PiTapeSettings PiTapeSettings::load()
{
    const auto file = settingsFile();
    if (!file.existsAsFile())
        return {};

    const auto parsed = juce::JSON::parse(file);
    if (parsed.isVoid())
        return {};

    return fromVar(parsed);
}

bool PiTapeSettings::save() const
{
    const auto file = settingsFile();
    const auto json = juce::JSON::toString(toVar(), true);
    return file.replaceWithText(json);
}

juce::var PiTapeSettings::toVar() const
{
    auto* o = new juce::DynamicObject();
    o->setProperty("enabled", enabled);
    o->setProperty("host", host);
    o->setProperty("port", port);
    o->setProperty("controlPort", controlPort);
    o->setProperty("username", username);
    o->setProperty("password", password);
    o->setProperty("remoteDir", remoteDir);
    return juce::var(o);
}

PiTapeSettings PiTapeSettings::fromVar(const juce::var& v)
{
    PiTapeSettings s;
    if (auto* o = v.getDynamicObject())
    {
        s.enabled = static_cast<bool>(o->getProperty("enabled"));
        s.host = o->getProperty("host").toString();
        s.port = static_cast<int>(o->getProperty("port"));
        s.controlPort = static_cast<int>(o->getProperty("controlPort"));
        s.username = o->getProperty("username").toString();
        s.password = o->getProperty("password").toString();
        s.remoteDir = o->getProperty("remoteDir").toString();
    }

    if (s.port <= 0)
        s.port = 21;
    if (s.controlPort <= 0)
        s.controlPort = 8765;
    if (s.remoteDir.trim().isEmpty())
        s.remoteDir = "inbox";

    return s;
}

} // namespace cassette
