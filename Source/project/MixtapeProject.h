#pragma once

#include <juce_core/juce_core.h>
#include <vector>
#include "../dsp/CassetteProfile.h"
#include "../dsp/MasteringOptions.h"

namespace cassette
{

struct TapeClip
{
    juce::File sourceFile;
    double startTimeSec = 0.0;
    double durationSec = 0.0;
    juce::String displayTitle;
    int trackIndex = 1;

    bool overlaps(const TapeClip& other) const
    {
        const auto a0 = startTimeSec;
        const auto a1 = startTimeSec + durationSec;
        const auto b0 = other.startTimeSec;
        const auto b1 = other.startTimeSec + other.durationSec;
        return a0 < b1 && b0 < a1;
    }
};

struct TapeSide
{
    static constexpr double defaultMaxDurationSec = 45.0 * 60.0;

    double maxDurationSec = defaultMaxDurationSec;
    std::vector<TapeClip> clips;

    double usedDurationSec() const
    {
        double end = 0.0;
        for (const auto& c : clips)
            end = juce::jmax(end, c.startTimeSec + c.durationSec);
        return end;
    }

    double remainingSec() const { return juce::jmax(0.0, maxDurationSec - usedDurationSec()); }

    bool fitsOnTape() const { return usedDurationSec() <= maxDurationSec + 1.0e-6; }

    bool validateClipPlacement(const TapeClip& candidate) const
    {
        if (candidate.startTimeSec < 0.0)
            return false;
        if (candidate.startTimeSec + candidate.durationSec > maxDurationSec)
            return false;
        for (const auto& existing : clips)
            if (existing.overlaps(candidate))
                return false;
        return true;
    }
};

struct MixtapeProject
{
    juce::String name { "Untitled Mixtape" };
    TapeSide sideA;
    TapeSide sideB;
    CassetteProfile profile = CassetteProfile::fromFormulation(TapeFormulation::TypeIV);
    MasteringOptions mastering;
    float recLevelDb = 0.0f;
    float biasDb = 0.0f;

    static juce::File defaultProjectsFolder();
    bool saveToFile(const juce::File& file) const;
    bool loadFromFile(const juce::File& file);

    static MixtapeProject demoProject()
    {
        MixtapeProject p;
        p.name = "Demo Mixtape";

        TapeClip a1;
        a1.displayTitle = "Track 1";
        a1.startTimeSec = 0.0;
        a1.durationSec = 4.0 * 60.0;
        a1.trackIndex = 1;
        p.sideA.clips.push_back(a1);

        TapeClip a2;
        a2.displayTitle = "Track 2";
        a2.startTimeSec = 4.0 * 60.0 + 8.0;
        a2.durationSec = 3.5 * 60.0;
        a2.trackIndex = 2;
        p.sideA.clips.push_back(a2);

        return p;
    }
};

}
