#pragma once

#include "../analysis/AudioFeatures.h"
#include "CassetteProfile.h"
#include "MasteringOptions.h"

namespace cassette
{

struct CassetteMasteringPlan
{
    MasteringOptions options;
    float tapeThreatScore = 0.0f;
    juce::String summary;
};

class CassetteMasteringPlanner
{
public:
    static CassetteMasteringPlan plan(const AudioFeatures& features, const CassetteProfile& profile);
};

}
