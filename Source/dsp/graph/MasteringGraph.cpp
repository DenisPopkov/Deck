#include "MasteringGraph.h"
#include "../CassetteAutoMaster.h"

namespace cassette
{

void MasteringGraph::prepare(double sr, int maxBlockSize)
{
    sampleRate = sr;
    juce::ignoreUnused(maxBlockSize);
}

void MasteringGraph::process(juce::AudioBuffer<float>& buffer,
                             const CassetteProfile& profile,
                             const AudioFeatures& features,
                             const MasteringOptions& options)
{
    CassetteAutoMaster master;
    master.prepare(sampleRate, buffer.getNumSamples());
    master.processTrack(buffer, profile, features, options);
}

}
