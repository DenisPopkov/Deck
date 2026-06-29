#include "PiTapeSessionState.h"
#include "PiTapeSettings.h"

namespace cassette
{

juce::File PiTapeSessionState::stateFile()
{
    return PiTapeSettings::settingsFile().getSiblingFile("pi-tape-session.json");
}

PiTapeSessionState PiTapeSessionState::load()
{
    const auto file = stateFile();
    if (!file.existsAsFile())
        return {};

    const auto parsed = juce::JSON::parse(file);
    if (auto* o = parsed.getDynamicObject())
    {
        PiTapeSessionState state;
        state.uploadedAtUnixMs = static_cast<juce::int64>(o->getProperty("uploaded_at_ms"));
        return state;
    }

    return {};
}

void PiTapeSessionState::clearPersisted()
{
    stateFile().deleteFile();
}

void PiTapeSessionState::save() const
{
    auto* o = new juce::DynamicObject();
    o->setProperty("uploaded_at_ms", uploadedAtUnixMs);
    const auto json = juce::JSON::toString(juce::var(o), true);
    stateFile().replaceWithText(json);
}

} // namespace cassette
