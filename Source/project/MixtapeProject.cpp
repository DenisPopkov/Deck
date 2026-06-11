#include "MixtapeProject.h"

namespace cassette
{

static juce::var clipToVar(const TapeClip& c)
{
    auto* o = new juce::DynamicObject();
    o->setProperty("path", c.sourceFile.getFullPathName());
    o->setProperty("title", c.displayTitle);
    o->setProperty("start", c.startTimeSec);
    o->setProperty("duration", c.durationSec);
    o->setProperty("track", c.trackIndex);
    return juce::var(o);
}

static TapeClip clipFromVar(const juce::var& v)
{
    TapeClip c;
    if (auto* o = v.getDynamicObject())
    {
        c.sourceFile = juce::File(o->getProperty("path").toString());
        c.displayTitle = o->getProperty("title").toString();
        c.startTimeSec = static_cast<double>(o->getProperty("start"));
        c.durationSec = static_cast<double>(o->getProperty("duration"));
        c.trackIndex = static_cast<int>(o->getProperty("track"));
    }
    return c;
}

static juce::var sideToVar(const TapeSide& side)
{
    auto* o = new juce::DynamicObject();
    o->setProperty("maxDuration", side.maxDurationSec);
    juce::Array<juce::var> arr;
    for (const auto& c : side.clips)
        arr.add(clipToVar(c));
    o->setProperty("clips", arr);
    return juce::var(o);
}

static void sideFromVar(TapeSide& side, const juce::var& v)
{
    if (auto* o = v.getDynamicObject())
    {
        side.maxDurationSec = o->getProperty("maxDuration");
        side.clips.clear();
        if (auto* arr = o->getProperty("clips").getArray())
            for (const auto& item : *arr)
                side.clips.push_back(clipFromVar(item));
    }
}

juce::File MixtapeProject::defaultProjectsFolder()
{
    const auto dir = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                         .getChildFile("CassetteBurner")
                         .getChildFile("projects");
    dir.createDirectory();
    return dir;
}

bool MixtapeProject::saveToFile(const juce::File& file) const
{
    auto* root = new juce::DynamicObject();
    root->setProperty("name", name);
    root->setProperty("tapeType", static_cast<int>(profile.formulation));
    root->setProperty("maximumDigital", mastering.maximumDigital);
    root->setProperty("hfTamerIntensity", mastering.hfTamerIntensity);
    root->setProperty("perceptualAutoFallback", mastering.perceptualAutoFallback);
    root->setProperty("recLevelDb", recLevelDb);
    root->setProperty("biasDb", biasDb);
    root->setProperty("sideA", sideToVar(sideA));
    root->setProperty("sideB", sideToVar(sideB));

    const auto json = juce::JSON::toString(juce::var(root), true);
    return file.replaceWithText(json);
}

bool MixtapeProject::loadFromFile(const juce::File& file)
{
    const auto parsed = juce::JSON::parse(file);
    if (parsed.isVoid())
        return false;

    if (auto* root = parsed.getDynamicObject())
    {
        name = root->getProperty("name").toString();
        profile = CassetteProfile::fromFormulation(
            static_cast<TapeFormulation>(static_cast<int>(root->getProperty("tapeType"))));
        mastering.maximumDigital = static_cast<bool>(root->getProperty("maximumDigital"));
        mastering.hfTamerIntensity = static_cast<float>(root->getProperty("hfTamerIntensity"));
        if (mastering.hfTamerIntensity <= 0.0f)
            mastering.hfTamerIntensity = 1.0f;
        mastering.perceptualAutoFallback = root->getProperty("perceptualAutoFallback").isVoid()
            ? true
            : static_cast<bool>(root->getProperty("perceptualAutoFallback"));
        recLevelDb = static_cast<float>(static_cast<double>(root->getProperty("recLevelDb")));
        biasDb = static_cast<float>(static_cast<double>(root->getProperty("biasDb")));
        sideFromVar(sideA, root->getProperty("sideA"));
        sideFromVar(sideB, root->getProperty("sideB"));
        return true;
    }
    return false;
}

}
